#include "Guard.h"
#include "stm32f10x.h"
#include "PetAction.h"
#include "Delay.h"

/* ================================================================
 * 超声波避障 + 电子围栏（防跌落）
 *  HC-SR04 : TRIG=PA5  ECHO=PA15(FT,5V 容忍)
 *  TCRT5000: 左=PA8  右=PA11（低电平=桌沿/无反射 -> 危险）
 *  时基    : TIM4 1MHz 自由计数
 * ================================================================ */

#define TRIG_PORT   GPIOA
#define TRIG_PIN    GPIO_Pin_5
#define ECHO_PORT   GPIOA
#define ECHO_PIN    GPIO_Pin_15
#define FEN_PORT    GPIOA
#define FEN_L_PIN   GPIO_Pin_8
#define FEN_R_PIN   GPIO_Pin_11

#define GUARD_CM_BLOCK   12u   /* 小于此距离禁止前进 */
#define GUARD_CM_BACK     7u   /* 小于此距离主动后退 */
#define GUARD_FENCE_CNT   2u   /* 连续 2 次(40ms) 判定桌沿 */

static uint16_t s_dist_cm   = 200;
static uint8_t  s_fence     = 0;   /* 1 = 桌沿危险 */
static uint8_t  s_fence_cnt = 0;
static uint8_t  s_meas_req  = 0;
static uint16_t s_tick_ms   = 0;

/* ---------- TIM4: 1MHz ---------- */
static void guard_tim4_init(void)
{
    TIM_TimeBaseInitTypeDef tb;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    tb.TIM_Prescaler         = 71;      /* 72MHz / 72 = 1MHz */
    tb.TIM_Period            = 0xFFFF;
    tb.TIM_CounterMode       = TIM_CounterMode_Up;
    tb.TIM_ClockDivision     = TIM_CKD_DIV1;
    tb.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &tb);
    TIM_SetCounter(TIM4, 0);
    TIM_Cmd(TIM4, ENABLE);
}

void Guard_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Pin   = TRIG_PIN;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(TRIG_PORT, &g);
    GPIO_ResetBits(TRIG_PORT, TRIG_PIN);

    g.GPIO_Pin  = ECHO_PIN;
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(ECHO_PORT, &g);

    g.GPIO_Pin  = FEN_L_PIN | FEN_R_PIN;
    g.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(FEN_PORT, &g);

    guard_tim4_init();
    s_dist_cm   = 200;
    s_fence     = 0;
    s_fence_cnt = 0;
    s_meas_req  = 0;
    s_tick_ms   = 0;
}

/* ---------- 单次测距（阻塞约 1~25ms，只在主循环调用） ---------- */
static uint16_t guard_measure_cm(void)
{
    uint16_t t0, dt, t_start;
    volatile uint16_t n;

    GPIO_SetBits(TRIG_PORT, TRIG_PIN);
    for (n = 0; n < 40u; n++) { __NOP(); }      /* ~12us 触发脉冲 */
    GPIO_ResetBits(TRIG_PORT, TRIG_PIN);

    t_start = TIM4->CNT;
    while (GPIO_ReadInputDataBit(ECHO_PORT, ECHO_PIN) == Bit_RESET) {
        if ((uint16_t)(TIM4->CNT - t_start) > 2000u) return s_dist_cm;  /* 2ms 无回波 */
    }

    t0 = TIM4->CNT;
    for (;;) {
        dt = (uint16_t)(TIM4->CNT - t0);
        if (GPIO_ReadInputDataBit(ECHO_PORT, ECHO_PIN) == Bit_RESET) break;
        if (dt > 30000u) return s_dist_cm;       /* 30ms 异常 */
    }
    dt = (uint16_t)(dt / 58u);                   /* 声速换算 */
    if (dt > 300u) dt = 300u;
    return dt;
}

/* ---------- 20ms 节拍：围栏采样 + 100ms 请求测距 ---------- */
void Guard_Tick20ms(void)
{
    uint8_t l = (GPIO_ReadInputDataBit(FEN_PORT, FEN_L_PIN) == Bit_RESET) ? 1 : 0;
    uint8_t r = (GPIO_ReadInputDataBit(FEN_PORT, FEN_R_PIN) == Bit_RESET) ? 1 : 0;

    if (l || r) {
        if (s_fence_cnt < 250u) s_fence_cnt++;
    } else {
        s_fence_cnt = 0;
    }
    s_fence = (s_fence_cnt >= GUARD_FENCE_CNT) ? 1 : 0;

    s_tick_ms += 20u;
    if (s_tick_ms >= 100u) { s_tick_ms = 0; s_meas_req = 1; }
}

/* ---------- 主循环调用：执行测距 ---------- */
void Guard_Poll(void)
{
    if (s_meas_req) {
        s_meas_req = 0;
        s_dist_cm  = guard_measure_cm();
    }
}

uint16_t Guard_DistanceCm(void) { return s_dist_cm; }
uint8_t  Guard_FenceHit(void)   { return s_fence; }
uint8_t  Guard_BlockForward(void)
{
    if (s_fence) return 1;
    if (s_dist_cm < GUARD_CM_BLOCK) return 1;
    return 0;
}
uint8_t Guard_ShouldRetreat(void)
{
    if (s_fence) return 1;
    if (s_dist_cm < GUARD_CM_BACK) return 1;
    return 0;
}

/* ---------------- 绕行试探 ----------------
 * 左转 300ms -> 前进 300ms；还挡 -> 右转 600ms -> 前进 300ms
 * 返回 1 = 已绕开或至少前进过；0 = 仍被挡
 * 所有动作走 PetAction 的模式变量，块内用 Delay_ms 推进步态
 * ------------------------------------------ */
uint8_t Guard_AvoidOnce(void)
{
    if (Guard_BlockForward() == 0) return 1;

    /* 第一招：左转让出空间 */
    PetAction_Judgmengt(0x35);          /* 左转 */
    Delay_ms(300);
    PetAction_Judgmengt(0x31);          /* 站立 */
    Delay_ms(60);
    Guard_Poll();
    if (Guard_BlockForward() == 0) {
        PetAction_Judgmengt(0x33);      /* 前进 */
        Delay_ms(300);
        PetAction_Judgmengt(0x31);
        return 1;
    }

    /* 第二招：右转更大角度 */
    PetAction_Judgmengt(0x36);          /* 右转 */
    Delay_ms(600);
    PetAction_Judgmengt(0x31);
    Delay_ms(60);
    Guard_Poll();
    if (Guard_BlockForward() == 0) {
        PetAction_Judgmengt(0x33);
        Delay_ms(300);
        PetAction_Judgmengt(0x31);
        return 1;
    }
    return 0;
}
