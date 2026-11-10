#ifndef __EXT_ENV_H__
#define __EXT_ENV_H__

#include "stm32f10x.h"

/* ======================================================================
 *  扩展功能模块：夜间自动亮度 + 温湿度历史曲线（环形缓冲）
 *
 *  调用方式（每 20ms 一次的节拍，即 TIM2 的 20ms 任务）：
 *      My_Init()/Screen_Init() 之后调用一次   ExtEnv_Init();
 *      每个 20ms 节拍调用一次                 ExtEnv_Tick20ms();
 *
 *  设计约束：
 *    - 无动态内存（不用 malloc）、无浮点（不用 float/double）
 *    - 静态变量全部 static，不污染全局命名空间（函数统一 ExtEnv_ 前缀）
 *    - 可在中断上下文运行（只做整数运算和小量状态机，比较轻重）
 *    - 不修改 g_cfg.backlight、不调用 Cfg_Save（夜间亮度只是临时覆盖，
 *      用户设定值保存在 flash 里不动，详见 ext_env.c 里的说明）
 *
 *  全局依赖：screen_ui.h / screen_cfg.h / rtc.h / sht30.h
 * ====================================================================== */

/* ---------------- 夜间自动亮度参数 ---------------- */
#define EXT_ENV_NIGHT_START_HOUR    22u   /* 22:00 起进入夜间模式          */
#define EXT_ENV_NIGHT_END_HOUR       7u   /* 07:00 退出，即 22:00-06:59    */
#define EXT_ENV_NIGHT_BRIGHTNESS    30u   /* 夜间背光 30%（白天用用户设定） */

/* ---------------- 温湿度曲线参数 ---------------- */
#define EXT_ENV_CURVE_MAX           60u   /* 环形缓冲点数（最旧->最新）    */
#define EXT_ENV_SAMPLE_TICKS     30000u   /* 采样间隔：30000*20ms = 10 分钟 */
#define EXT_ENV_FIRST_TICKS        100u   /* 上电首个点：2s 后（等 SHT30） */

/* ---------------- 接口 ---------------- */
void ExtEnv_Init(void);              /* 上电调用一次                       */
void ExtEnv_Tick20ms(void);          /* 每 20ms 调用一次                   */

/* 曲线点数：0..EXT_ENV_CURVE_MAX */
uint8_t ExtEnv_CurveCount(void);

/* 读取第 i 个点：i 从 0 = 最旧，到 count-1 = 最新
 *   temp_x10  : 温度，单位 0.1 摄氏度（有符号，已做 -40.0..80.0 限幅）
 *   hum       : 湿度，单位整数 %（0..100，已做限幅）
 *   min_of_day: 采样时刻的 (hour*60 + min) 取低 8 位（见 ext_env.c 说明）
 * 越界（i >= count）或指针为 NULL 时不做任何写操作。 */
void ExtEnv_CurveGet(uint8_t i, int16_t *temp_x10, uint8_t *hum, uint8_t *min_of_day);

#endif
