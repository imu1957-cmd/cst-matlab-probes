#ifndef __EXT_WEATHER_H
#define __EXT_WEATHER_H

#include "stm32f10x.h"

/* 天气状态模块（EXT 层，独立模块）
 * 作用：缓存上层（涂鸦云）下发的天气数据，去重后驱动屏幕 t3 区域上屏。
 * 依赖：screen_ui.c 的 Screen_SetWeather()，以及 printf 重定向到 USART1。
 * 用法：上电调用 Weather_Init() 一次；
 *       在 20ms 节拍中调用 Weather_Tick20ms()；
 *       云端数据到达时调用 Weather_Set()；
 *       云端超时/断连时调用 Weather_SetUnknown()。
 *
 * 有效数据超时时间 WEATHER_TIMEOUT_MS（120 秒），按 20ms tick 累计，
 * 超时后自动调用 Weather_SetUnknown()，屏幕显示 "--"。
 */

#define WEATHER_TICK_MS        20u        /* Tick20ms 节拍长度(ms) */
#define WEATHER_TIMEOUT_MS     120000u    /* 无更新超时(ms) */
#define WEATHER_TIMEOUT_TICKS  (WEATHER_TIMEOUT_MS / WEATHER_TICK_MS) /* 6000 */

#define WEATHER_TEMP_MIN       (-40)      /* 温度下限(摄氏度) */
#define WEATHER_TEMP_MAX       (80)       /* 温度上限(摄氏度) */
#define WEATHER_TEMP_INVALID   0xFFu      /* Weather_GetTemp() 无效返回值 */
#define WEATHER_COND_MAX       15u        /* 天气 ID 上限(15=未知) */

/* 上电初始化：状态置为未知并强制上屏一次 */
void Weather_Init(void);

/* 20ms 节拍：负责超时检测与上屏去重 */
void Weather_Tick20ms(void);

/* 写入一条天气数据。temp_c: -40..80；cond_id: 0..15，越界按 15(未知)处理。
 * 返回 1=有变化且已上屏；0=无变化或温度越界（温度越界时不更新任何状态）。 */
uint8_t Weather_Set(int16_t temp_c, uint8_t cond_id);

/* 云端超时/无数据：立即置为未知并显示 "--" */
void Weather_SetUnknown(void);

/* 最近温度：有效时返回 (uint8_t)(int8_t)temp_c（0..80 原值，-40..-1 为补码），
 * 无效时返回 0xFF。注意 -1 摄氏度的补码也是 0xFF，需配合 Weather_IsValid() 判断。 */
uint8_t Weather_GetTemp(void);

/* 最近天气的 GBK 文字，如 "晴"（内部转义字节）；无有效数据时返回 "未知" 的 GBK 串。 */
const char *Weather_GetCondText(void);

/* 1=有有效数据且未超时；0=未知/超时 */
uint8_t Weather_IsValid(void);

#endif
