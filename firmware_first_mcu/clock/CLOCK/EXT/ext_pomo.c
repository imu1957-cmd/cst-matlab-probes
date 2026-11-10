#include "stm32f10x.h"
#include <stdio.h>
#include "screen_ui.h"
#include "clock_link.h"
#include "led_fx.h"
#include "ext_pomo.h"

/* ======================================================================
 *  ext_pomo.c  —— 时钟端「番茄钟」
 *
 *  N 分钟工作 -> M 分钟休息，循环到 ExtPomo_Stop()。
 *  计时全部由 20ms 节拍累加，每攒满 1000ms 才动一次秒数，
 *  状态行每秒最多刷新一次，不阻塞、不刷屏。
 *
 *  工作阶段：状态行「剩余 MM:SS」
 *  工作结束：状态「休息 M 分钟」+ 宠物打招呼(0x43) + 红灯提示 3 秒
 *  休息结束：状态「继续工作」+ 宠物左转(0x35)  + 回到工作阶段
 *
 *  依赖：screen_ui / clock_link / led_fx；与 ext_demo 完全独立。
 *  文件编码：GBK（Keil 下中文注释不乱码）。
 * ====================================================================== */

#define POMO_TICK_MS        20u                          /* TIM2 节拍 */
#define POMO_TICKS_PER_SEC  (1000u / POMO_TICK_MS)       /* 50 tick = 1s */
#define POMO_DEF_WORK_MIN   25u                          /* 默认工作时长 */
#define POMO_DEF_REST_MIN   5u                           /* 默认休息时长 */
#define POMO_MAX_MIN        99u                          /* 参数上限 */
#define POMO_FLASH_MS       3000u                        /* 红灯提示时长 */

#define POMO_CMD_REST_START 0x43u   /* 动作命令：打招呼（进入休息） */
#define POMO_CMD_WORK_START 0x35u   /* 动作命令：左转（回去工作） */

static uint8_t  s_run      = 0;     /* 1 = 计时中 */
static uint8_t  s_rest     = 0;     /* 1 = 休息阶段 */
static uint8_t  s_work_min = POMO_DEF_WORK_MIN;
static uint8_t  s_rest_min = POMO_DEF_REST_MIN;
static uint16_t s_remain   = 0;     /* 当前阶段剩余秒数 */
static uint8_t  s_sec_cnt  = 0;     /* 20ms 计数，满 50 进 1 秒 */
static uint8_t  s_flash_on = 0;     /* 1 = 红灯提示进行中 */
static uint16_t s_flash_ms = 0;     /* 红灯提示剩余毫秒 */

/* 状态行显示「剩余 MM:SS」（GBK 约 10 字节，远小于 t5 的 20 字节上限） */
static void Pomo_ShowRemain(void)
{
    char buf[24];

    sprintf(buf, "剩余 %02d:%02d", (int)(s_remain / 60u), (int)(s_remain % 60u));
    Screen_SetStatus(buf);
}

/* 进入工作阶段；from_rest=1 表示刚从休息切回来（先亮「继续工作」） */
static void Pomo_EnterWork(uint8_t from_rest)
{
    s_rest    = 0;
    s_remain  = (uint16_t)s_work_min * 60u;
    s_sec_cnt = 0;

    if(from_rest)
    {
        Screen_SetStatus("继续工作");
        ClockLink_SendCmd(POMO_CMD_WORK_START);
        /* 不立刻刷倒计时，让「继续工作」在屏幕上停满约 1 秒 */
    }
    else
    {
        Pomo_ShowRemain();
    }
}

/* 进入休息阶段：提示 + 宠物打招呼 + 红灯提示 3 秒 */
static void Pomo_EnterRest(void)
{
    char buf[24];

    s_rest    = 1;
    s_remain  = (uint16_t)s_rest_min * 60u;
    s_sec_cnt = 0;

    sprintf(buf, "休息 %d 分钟", (int)s_rest_min);
    Screen_SetStatus(buf);

    ClockLink_SendCmd(POMO_CMD_REST_START);

    LedFx_SetMode(2);           /* 借低电红呼吸当提示，3 秒后恢复待机 */
    s_flash_on = 1;
    s_flash_ms = POMO_FLASH_MS;
}

/* ------------------------------------------------------------------ */

void ExtPomo_Init(void)
{
    s_run      = 0;
    s_rest     = 0;
    s_work_min = POMO_DEF_WORK_MIN;
    s_rest_min = POMO_DEF_REST_MIN;
    s_remain   = 0;
    s_sec_cnt  = 0;
    s_flash_on = 0;
    s_flash_ms = 0;
}

void ExtPomo_Start(uint8_t work_min, uint8_t rest_min)
{
    if(work_min == 0) work_min = POMO_DEF_WORK_MIN;     /* 0 = 用默认值 */
    if(rest_min == 0) rest_min = POMO_DEF_REST_MIN;
    if(work_min > (uint8_t)POMO_MAX_MIN) work_min = (uint8_t)POMO_MAX_MIN;
    if(rest_min > (uint8_t)POMO_MAX_MIN) rest_min = (uint8_t)POMO_MAX_MIN;

    /* 重复调用 = 重新开始一个完整工作周期 */
    s_work_min = work_min;
    s_rest_min = rest_min;
    s_run      = 1;
    s_rest     = 0;
    s_remain   = 0;
    s_sec_cnt  = 0;
    s_flash_on = 0;
    s_flash_ms = 0;

    Pomo_EnterWork(0);
}

void ExtPomo_Stop(void)
{
    s_run     = 0;
    s_rest    = 0;
    s_remain  = 0;
    s_sec_cnt = 0;

    if(s_flash_on)
    {
        s_flash_on = 0;
        s_flash_ms = 0;
        LedFx_SetMode(0);       /* 灯效交回主流程（working() 会重设） */
    }
}

void ExtPomo_Tick20ms(void)
{
    if(!s_run) return;

    /* 红灯提示计时：3 秒到就恢复待机灯效，休息本身继续走 */
    if(s_flash_on)
    {
        if(s_flash_ms > (uint16_t)POMO_TICK_MS)
        {
            s_flash_ms = (uint16_t)(s_flash_ms - (uint16_t)POMO_TICK_MS);
        }
        else
        {
            s_flash_ms = 0;
            s_flash_on = 0;
            LedFx_SetMode(0);
        }
    }

    /* 20ms 攒够 1 秒 */
    s_sec_cnt++;
    if(s_sec_cnt < (uint8_t)POMO_TICKS_PER_SEC) return;
    s_sec_cnt = 0;

    if(s_remain > 0) s_remain--;

    if(s_remain > 0)
    {
        if(!s_rest) Pomo_ShowRemain();      /* 休息阶段不刷倒计时 */
        return;
    }

    /* 本阶段走完，切到下一阶段 */
    if(!s_rest) Pomo_EnterRest();
    else        Pomo_EnterWork(1);
}

uint8_t ExtPomo_IsRunning(void)
{
    return s_run;
}

uint8_t ExtPomo_IsResting(void)
{
    return (uint8_t)(s_run && s_rest);
}

uint16_t ExtPomo_RemainSec(void)
{
    return s_run ? s_remain : 0;
}
