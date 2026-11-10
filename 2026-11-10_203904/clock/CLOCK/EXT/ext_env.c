/* ======================================================================
 *  ext_env.c  扩展功能：夜间自动亮度 + 温湿度历史曲线
 *  目标：STM32F103C8，标准外设库 3.5.0，Keil（GBK 源文件）
 *
 *  对外接口见 ext_env.h。本文件只依赖以下已有 API：
 *      screen_ui.h   Screen_SetBrightness()
 *      screen_cfg.h  g_cfg.backlight（只读语义，见下方说明）
 *      rtc.h         calendar.hour / calendar.min / calendar.w_year
 *      sht30.h       SHT30_temperature / SHT30_humidity
 * ====================================================================== */

#include "ext_env.h"
#include "screen_ui.h"
#include "screen_cfg.h"
#include "rtc.h"
#include "sht30.h"

/* ----------------------------------------------------------------------
 *  重要说明 1：关于 Screen_SetBrightness()
 *  现有实现 screen_ui.c:86 里，Screen_SetBrightness(pct) 除了发 "dim=pct"
 *  之外，还会在 pct != g_cfg.backlight 时把 pct 写进 g_cfg.backlight 并
 *  调用 Cfg_Save()（擦写内部 flash！）。如果夜间直接调用
 *  Screen_SetBrightness(30)，会有两个严重后果：
 *      1) 用户设定的白天亮度被永久改成 30，掉电后也回不来；
 *      2) 每次都从 20ms 中断里擦 flash（一次擦除几十 ms），严重卡节拍。
 *  本模块的做法：调用前把 g_cfg.backlight 临时对齐成要设置的值，这样
 *  Screen_SetBrightness 内部的 "pct != g_cfg.backlight" 不成立，不会
 *  触发 Cfg_Save；返回后再把 g_cfg.backlight 还原成用户设定值。
 *  净效果：屏幕背光是临时覆盖，g_cfg / flash 里的用户设定值保持原样。
 *  这段代码依赖于 screen_ui.c 当前实现，若以后 Screen_SetBrightness 改了
 *  行为（例如去掉写 g_cfg 的副作用），这里仍然安全（只是多做两次赋值）。
 * ---------------------------------------------------------------------- */

/* 采样时刻说明：min_of_day 取 (hour*60 + min) 的低 8 位（保留接口为 uint8）。
 * 由于采样间隔固定 10 分钟，相邻点的 min_of_day 差值恒为 10（模 256）；
 * 画图时用"最新点 = 当前时刻，往前第 i 个点 = 当前时刻 - 10*i 分钟"来还原
 * 绝对时间最省事，不必依赖这个被截断的值。 */

/* ---------------- 模块内部状态（全部 static） ---------------- */
static int16_t  s_temp[EXT_ENV_CURVE_MAX];   /* 温度 0.1 摄氏度            */
static uint8_t  s_hum[EXT_ENV_CURVE_MAX];    /* 湿度整数 %                  */
static uint8_t  s_min[EXT_ENV_CURVE_MAX];    /* (hour*60+min) & 0xFF        */

static uint8_t  s_count = 0;                 /* 已存点数 0..60              */
static uint8_t  s_head  = 0;                 /* 下一个写入位置              */
static uint16_t s_tick  = 0;                 /* 20ms 采样计数               */
static uint8_t  s_applied = 0xFF;            /* 已经下发过的亮度（0xFF=没发过）*/
static uint8_t  s_ready = 0;                 /* ExtEnv_Init() 之后才跑节拍   */

/* ---------------- 内部函数 ---------------- */

/* 是否夜间：22:00-06:59 */
static uint8_t ExtEnv_IsNight(void)
{
    uint8_t h = (uint8_t)calendar.hour;      /* calendar.hour 为 vu8（volatile） */

    if (h >= EXT_ENV_NIGHT_START_HOUR) return 1;
    if (h <  EXT_ENV_NIGHT_END_HOUR)   return 1;
    return 0;
}

/* 把背光设成 pct：只在 s_applied 变化时下发，且不弄脏 g_cfg / flash */
static void ExtEnv_SetBrightness(uint8_t pct)
{
    uint8_t saved;

    if (pct == s_applied) return;            /* 没变化就不发，串口很慢 */

    saved = g_cfg.backlight;                 /* 用户设定值（RAM 原样）  */
    g_cfg.backlight = pct;                   /* 临时对齐，抑制 Cfg_Save */
    Screen_SetBrightness(pct);               /* 真正发 "dim=pct"         */
    g_cfg.backlight = saved;                 /* 还原用户设定值            */
    s_applied = pct;
}

/* 夜间自动亮度状态机：RTC 未校准（w_year == 2015）时保持用户设定 */
static void ExtEnv_AutoBrightness(void)
{
    uint8_t base, want;

    base = g_cfg.backlight;                  /* 用户设定的白天亮度        */
    if (base < 10)  base = 10;               /* 与 screen_cfg.c 的限幅一致 */
    if (base > 100) base = 100;

    want = base;
    if (calendar.w_year != 2015)             /* 首次未校准要跳过          */
    {
        if (ExtEnv_IsNight()) want = EXT_ENV_NIGHT_BRIGHTNESS;
    }

    ExtEnv_SetBrightness(want);
}

/* 采一个点进环形缓冲（调用前保证 RTC 已校准） */
static void ExtEnv_PushSample(void)
{
    int16_t  t;
    uint16_t h10;
    uint8_t  h;
    uint8_t  mod;

    /* 温度：SHT30_temperature 声明为 u16，但 sht30.c 里是按 int 赋值的
     * (175.0*data/65535.0 - 45.0)*10，负温是二进制补码，转回 int16 即可。 */
    t = (int16_t)SHT30_temperature;
    if (t >  800) t =  800;                  /* 限幅 +80.0 摄氏度          */
    if (t < -400) t = -400;                   /* 限幅 -40.0 摄氏度          */

    /* 湿度：SHT30_humidity 单位是 0.1%（0..1000），曲线里存整数 %（0..100）*/
    h10 = (uint16_t)SHT30_humidity;
    h   = (uint8_t)(h10 / 10u);
    if (h > 100u) h = 100u;

    /* 时间戳：(hour*60 + min) 取低 8 位 */
    mod = (uint8_t)(((uint16_t)calendar.hour * 60u + (uint16_t)calendar.min) & 0xFFu);

    s_temp[s_head] = t;
    s_hum[s_head]  = h;
    s_min[s_head]  = mod;

    s_head++;
    if (s_head >= EXT_ENV_CURVE_MAX) s_head = 0;
    if (s_count < EXT_ENV_CURVE_MAX) s_count++;
}

/* ---------------- 对外接口 ---------------- */

void ExtEnv_Init(void)
{
    uint8_t i;

    s_ready   = 0;
    s_count   = 0;
    s_head    = 0;
    s_tick    = 0;
    s_applied = 0xFF;                        /* 让第一次节拍同步一次亮度    */

    for (i = 0; i < (uint8_t)EXT_ENV_CURVE_MAX; i++)
    {
        s_temp[i] = 0;
        s_hum[i]  = 0;
        s_min[i]  = 0;
    }

    s_ready = 1;
}

void ExtEnv_Tick20ms(void)
{
    uint16_t period;

    if (!s_ready) return;                    /* ExtEnv_Init() 之前不动      */

    /* 1) 夜间自动亮度：内部只在"应设亮度"变化时才真的发串口命令 */
    ExtEnv_AutoBrightness();

    /* 2) 温湿度采样：第一个点 2s 后采，之后每 10 分钟一个点 */
    period = (s_count == 0u) ? (uint16_t)EXT_ENV_FIRST_TICKS
                             : (uint16_t)EXT_ENV_SAMPLE_TICKS;

    if (s_tick < period) s_tick++;           /* 不进位溢出：最多停在 period */

    if ((s_tick >= period) && (calendar.w_year != 2015))
    {
        ExtEnv_PushSample();
        s_tick = 0;
    }
}

uint8_t ExtEnv_CurveCount(void)
{
    return s_count;
}

void ExtEnv_CurveGet(uint8_t i, int16_t *temp_x10, uint8_t *hum, uint8_t *min_of_day)
{
    uint8_t idx;

    if (i >= s_count) return;                /* 越界：不改输出              */

    if (s_count < (uint8_t)EXT_ENV_CURVE_MAX)
    {
        idx = i;                             /* 未满：0 就是最旧            */
    }
    else
    {
        idx = (uint8_t)(s_head + i);         /* 满了：s_head 处是最旧        */
        if (idx >= (uint8_t)EXT_ENV_CURVE_MAX)
            idx = (uint8_t)(idx - (uint8_t)EXT_ENV_CURVE_MAX);
    }

    if (temp_x10)  *temp_x10  = s_temp[idx];
    if (hum)       *hum       = s_hum[idx];
    if (min_of_day) *min_of_day = s_min[idx];
}
