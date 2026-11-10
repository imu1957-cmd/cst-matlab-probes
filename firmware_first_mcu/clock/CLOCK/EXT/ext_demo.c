#include "stm32f10x.h"
#include "screen_ui.h"
#include "clock_link.h"
#include "led_fx.h"
#include "ext_demo.h"

/* ======================================================================
 *  ext_demo.c  —— 时钟端「一键演示模式」
 *
 *  30 秒自动演示，全部由 20ms 节拍推进，没有任何阻塞延时：
 *    0s   主页 + 状态行「演示开始」
 *    2s   推一条备忘录上屏 + 同时转发给宠物
 *    6s   切到备忘录页
 *    10s  叫宠物回充 + 状态行「叫宠物回充」
 *    16s  充电灯效（绿流水）
 *    22s  低电灯效（红呼吸）
 *    26s  恢复待机灯效（白呼吸）
 *    30s  回主页 + 状态行「演示结束」
 *    32s  保持 2 秒后真正退出（IsRunning 归 0）
 *
 *  为什么 30s 后还要再保持 2 秒：
 *    working() 每 20ms 都会调 Screen_SetPetOnline()，该函数无条件把 t5
 *    状态行改写为 ON/OFF；若 30s 立刻退出，主流程会在 20ms 内把
 *    「演示结束」冲掉，人眼看不到。保持期间 ExtDemo_IsRunning() 仍返回 1，
 *    working() 的 IsRunning 门控会继续让出状态行和灯效。
 *    （不需要这个效果时把 DEMO_HOLD_MS 改成 0 即可立即退出。）
 *
 *  依赖：screen_ui / clock_link / led_fx；与 ext_pomo 完全独立。
 *  文件编码：GBK（Keil 下中文注释不乱码）。
 * ====================================================================== */

#define DEMO_TICK_MS        20u     /* TIM2 节拍周期（毫秒） */
#define DEMO_STEP_NUM       8u      /* 动作点个数 */
#define DEMO_HOLD_MS        2000u   /* 结束后「演示结束」的保持时间 */
#define DEMO_PAGE_MAIN      0u      /* screen_ui 页面号：主页 */
#define DEMO_PAGE_MEMO      1u      /* screen_ui 页面号：备忘录 */

/* 8 个动作点的时间轴（毫秒） */
static const uint16_t s_demo_ms[DEMO_STEP_NUM] =
{
    0u, 2000u, 6000u, 10000u, 16000u, 22000u, 26000u, 30000u
};

/* 演示用备忘录「15:00 组会」。
 * 用 GBK 转义字节写死，不依赖编译器源字符集：组 = D7 E9，会 = BB E1。
 * 屏幕与宠物链路都按 GBK 收文本（main.c 的备忘录路径是 Utf8ToGbk 后再下发），
 * 所以同一条字符串两边通用；ASCII 部分（15:00 ）两边都安全。 */
static const char s_demo_memo[] = "15:00 \xD7\xE9\xBB\xE1";

static uint8_t  s_running = 0;      /* 1 = 演示进行中 */
static uint8_t  s_step    = 0;      /* 下一个待执行的动作点下标 */
static uint16_t s_ms      = 0;      /* 演示已运行的毫秒数 */
static uint8_t  s_hold    = 0;      /* 1 = 结束语保持中 */
static uint16_t s_hold_ms = 0;      /* 结束语剩余毫秒 */

/* 执行第 step 个动作点（step = 0..7） */
static void Demo_RunStep(uint8_t step)
{
    switch(step)
    {
    case 0:
        Screen_ShowPage(DEMO_PAGE_MAIN);
        Screen_SetStatus("演示开始");
        break;

    case 1:
        Screen_PushMemo(s_demo_memo);       /* 上屏（内部会翻到备忘录页） */
        ClockLink_SendMemo(s_demo_memo);    /* 同一时刻转发给宠物念 */
        break;

    case 2:
        Screen_ShowPage(DEMO_PAGE_MEMO);    /* 明确停在备忘录页 */
        break;

    case 3:
        ClockLink_StartHome();              /* 叫宠物回充 */
        Screen_SetStatus("叫宠物回充");
        break;

    case 4:
        LedFx_SetMode(1);                   /* 充电：绿色流水 */
        break;

    case 5:
        LedFx_SetMode(2);                   /* 低电：红色呼吸 */
        break;

    case 6:
        LedFx_SetMode(0);                   /* 待机：白色呼吸 */
        break;

    case 7:
        Screen_ShowPage(DEMO_PAGE_MAIN);
        Screen_SetStatus("演示结束");
        s_hold    = 1;                      /* 30s 到点，进入结束语保持 */
        s_hold_ms = DEMO_HOLD_MS;
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------ */

void ExtDemo_Init(void)
{
    s_running = 0;
    s_step    = 0;
    s_ms      = 0;
    s_hold    = 0;
    s_hold_ms = 0;
}

void ExtDemo_Start(void)
{
    /* 重复调用 = 从头重新开始，不会叠加两份流程 */
    s_running = 1;
    s_step    = 0;
    s_ms      = 0;
    s_hold    = 0;
    s_hold_ms = 0;

    Demo_RunStep(0);        /* 0s 的动作立即生效，不用等第一个 tick */
    s_step = 1;
}

void ExtDemo_Stop(void)
{
    s_running = 0;
    s_step    = 0;
    s_ms      = 0;
    s_hold    = 0;
    s_hold_ms = 0;
}

void ExtDemo_Tick20ms(void)
{
    uint8_t step;

    if(!s_running) return;

    /* 结束语保持：只倒计时，不再走动作点 */
    if(s_hold)
    {
        if(s_hold_ms > (uint16_t)DEMO_TICK_MS)
        {
            s_hold_ms = (uint16_t)(s_hold_ms - (uint16_t)DEMO_TICK_MS);
        }
        else
        {
            s_hold_ms = 0;
            s_hold    = 0;
            s_running = 0;      /* 真正退出，把状态行/灯效交回 working() */
        }
        return;
    }

    s_ms = (uint16_t)(s_ms + DEMO_TICK_MS);

    /* 一次性把到点的动作点全部走完（一般每 tick 最多一个） */
    while(s_running && !s_hold && (s_step < (uint8_t)DEMO_STEP_NUM)
          && (s_ms >= s_demo_ms[s_step]))
    {
        step = s_step;
        s_step++;
        Demo_RunStep(step);
    }
}

uint8_t ExtDemo_IsRunning(void)
{
    return s_running;
}
