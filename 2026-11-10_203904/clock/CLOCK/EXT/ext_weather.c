/*******************************************************************************
 * 文件名   : ext_weather.c
 * 模块     : 天气状态（EXT 层，独立模块）
 * 目标平台 : STM32F103 + 标准外设库 + Keil armcc5（C89）
 * 说明     : 缓存上层（涂鸦云）天气数据，变化时调用 Screen_SetWeather() 上屏；
 *            120 秒无更新自动转未知（显示 "--"）。
 *            本文件不使用动态分配，不改动任何现有文件。
 ******************************************************************************/

#include "stm32f10x.h"
#include <stdio.h>
#include "screen_ui.h"
#include "ext_weather.h"

/* 天气 ID 到 GBK 中文的静态映射表。
 * 中文一律用 GBK 转义字节书写，避免源文件编码歧义。
 * 0=晴 1=多云 2=阴 3=小雨 4=中雨 5=大雨 6=雷阵雨 7=小雪
 * 8=中雪 9=大雪 10=雾 11=霾 12=沙尘 13=风 14=冰雹 15=未知
 */
static const char * const s_cond_text[16] = {
    "\xC7\xE7",                     /* 0  晴     */
    "\xB6\xE0\xD4\xC6",             /* 1  多云   */
    "\xD2\xF5",                     /* 2  阴     */
    "\xD0\xA1\xD3\xEA",             /* 3  小雨   */
    "\xD6\xD0\xD3\xEA",             /* 4  中雨   */
    "\xB4\xF3\xD3\xEA",             /* 5  大雨   */
    "\xC0\xD7\xD5\xF3\xD3\xEA",     /* 6  雷阵雨 */
    "\xD0\xA1\xD1\xA9",             /* 7  小雪   */
    "\xD6\xD0\xD1\xA9",             /* 8  中雪   */
    "\xB4\xF3\xD1\xA9",             /* 9  大雪   */
    "\xCE\xED",                     /* 10 雾     */
    "\xF6\xB2",                     /* 11 霾     */
    "\xC9\xB3\xB3\xBE",             /* 12 沙尘   */
    "\xB7\xE7",                     /* 13 风     */
    "\xB1\xF9\xB1\xA2",             /* 14 冰雹   */
    "\xCE\xB4\xD6\xAA"              /* 15 未知   */
};

/* 当前有效数据 */
static int16_t  s_temp_c;          /* 最近温度 */
static uint8_t  s_cond_id;         /* 最近天气 ID(0..15) */
static uint8_t  s_valid;           /* 1=有效未超时 */
static uint16_t s_elapsed;         /* 距上次更新的 tick 数，饱和到超时值 */

/* 已上屏快照，用于去重，避免重复刷屏 */
static uint8_t  s_disp_ready;      /* 1=已有上屏记录 */
static uint8_t  s_disp_valid;      /* 上屏时的有效标志 */
static int16_t  s_disp_temp;       /* 上屏时的温度 */
static uint8_t  s_disp_cond;       /* 上屏时的天气 ID */

/* 天气 ID -> GBK 文字；越界按未知处理 */
static const char *Weather_CondText(uint8_t id)
{
    if (id > (uint8_t)WEATHER_COND_MAX)
    {
        id = (uint8_t)WEATHER_COND_MAX;
    }
    return s_cond_text[id];
}

/* 将当前状态推送到屏幕（内部去重：与上次上屏内容相同则跳过） */
static void Weather_PushDisplay(void)
{
    const char *text;

    if (s_valid != (uint8_t)0u)
    {
        if ((s_disp_ready != (uint8_t)0u) &&
            ((uint8_t)s_disp_valid == s_valid) &&
            ((int16_t)s_disp_temp == s_temp_c) &&
            ((uint8_t)s_disp_cond == s_cond_id))
        {
            return;
        }

        text = Weather_CondText(s_cond_id);
        Screen_SetWeather(s_temp_c, text);
        printf("[WEA] %dC %s\r\n", (int)s_temp_c, text);
    }
    else
    {
        if ((s_disp_ready != (uint8_t)0u) && (s_disp_valid == (uint8_t)0u))
        {
            return;
        }

        Screen_SetWeather(0, "--");
        printf("[WEA] --\r\n");
    }

    s_disp_ready = (uint8_t)1u;
    s_disp_valid = s_valid;
    s_disp_temp  = s_temp_c;
    s_disp_cond  = s_cond_id;
}

/* 上电初始化：状态置未知，并强制上屏一次 "--" */
void Weather_Init(void)
{
    s_temp_c   = (int16_t)0;
    s_cond_id  = (uint8_t)0u;
    s_valid    = (uint8_t)0u;
    s_elapsed  = (uint16_t)WEATHER_TIMEOUT_TICKS;

    s_disp_ready = (uint8_t)0u;
    s_disp_valid = (uint8_t)0u;
    s_disp_temp  = (int16_t)0;
    s_disp_cond  = (uint8_t)0u;

    Weather_PushDisplay();
}

/* 20ms 节拍：累计超时计数，超时后转未知 */
void Weather_Tick20ms(void)
{
    if (s_elapsed < (uint16_t)WEATHER_TIMEOUT_TICKS)
    {
        s_elapsed++;
    }

    if ((s_valid != (uint8_t)0u) && (s_elapsed >= (uint16_t)WEATHER_TIMEOUT_TICKS))
    {
        Weather_SetUnknown();
    }
}

/* 写入天气数据：仅在有效数据发生变化时上屏并打印 */
uint8_t Weather_Set(int16_t temp_c, uint8_t cond_id)
{
    if ((temp_c < (int16_t)WEATHER_TEMP_MIN) || (temp_c > (int16_t)WEATHER_TEMP_MAX))
    {
        return (uint8_t)0u;         /* 温度越界：忽略，不改任何状态 */
    }

    if (cond_id > (uint8_t)WEATHER_COND_MAX)
    {
        cond_id = (uint8_t)WEATHER_COND_MAX;   /* 未知/越界 ID 统一按"未知"处理 */
    }

    if ((s_valid != (uint8_t)0u) &&
        ((int16_t)s_temp_c == temp_c) &&
        ((uint8_t)s_cond_id == cond_id))
    {
        s_elapsed = (uint16_t)0u;   /* 数据无变化：只刷新超时计时 */
        return (uint8_t)0u;
    }

    s_temp_c  = temp_c;
    s_cond_id = cond_id;
    s_valid   = (uint8_t)1u;
    s_elapsed = (uint16_t)0u;

    Weather_PushDisplay();
    return (uint8_t)1u;
}

/* 云端超时/无数据：立即置未知并显示 "--" */
void Weather_SetUnknown(void)
{
    s_valid   = (uint8_t)0u;
    s_elapsed = (uint16_t)WEATHER_TIMEOUT_TICKS;   /* 保持饱和，避免计时溢出 */

    Weather_PushDisplay();
}

/* 最近温度：有效时返回 (uint8_t)(int8_t)temp，无效返回 0xFF */
uint8_t Weather_GetTemp(void)
{
    if (s_valid == (uint8_t)0u)
    {
        return (uint8_t)WEATHER_TEMP_INVALID;
    }
    return (uint8_t)((int8_t)s_temp_c);
}

/* 最近天气文字：无有效数据时返回 "未知" */
const char *Weather_GetCondText(void)
{
    if (s_valid == (uint8_t)0u)
    {
        return Weather_CondText((uint8_t)WEATHER_COND_MAX);
    }
    return Weather_CondText(s_cond_id);
}

/* 1=有有效数据且未超时 */
uint8_t Weather_IsValid(void)
{
    return s_valid;
}
