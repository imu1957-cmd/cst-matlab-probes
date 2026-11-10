#ifndef __EXT_REPEAT_H__
#define __EXT_REPEAT_H__

#include "stm32f10x.h"

/* ======================================================================
 *  扩展功能：定时备忘录（重复提醒）
 *  目标：STM32F103C8，标准外设库 3.5.0，Keil（GBK 源文件）
 *
 *  调用方式（与 ext_env 模块一致）：
 *      My_Init() 里 AT24CXX_Init() / RTC_Init() 之后调用一次：
 *          ExtRepeat_Init();
 *      TIM2 的 20ms 节拍里调用（control.c 的 working()）：
 *          ExtRepeat_Tick20ms();
 *
 *  设计约束：
 *    - 无动态内存（不用 malloc）、无浮点（不用 float/double）
 *    - 全部状态 static，函数统一 ExtRepeat_ 前缀
 *    - 文本为 GBK 编码，最长 35 字节（双字节汉字不拆半，纯中文最多 17 字）
 *    - ExtRepeat_Tick20ms() 可在中断上下文运行；Add/Remove/ClearAll 会写
 *      EEPROM（每字节约 10ms），必须在主循环上下文调用。
 *
 *  掉电存储（AT24C02，软 I2C，只读写 0x80..0xF4）：
 *      0x80 + i*38 : 第 i 条 {hour, min, text[36]}     i = 0..2，共 3 条
 *      0xF2        : 有效标志 EXT_RPT_MAGIC
 *      0xF3        : 掉电保存条数 0..3
 *      0xF4        : 校验和
 *    RAM 里最多 8 条，掉电只保前 3 条。
 *    0x00..0x12 是 control.c 的定时/氛围灯参数，0xFF 是"首次使用"标志，
 *    本模块都不碰。
 *
 *  提醒触发（calendar.sec == 0 且 hour/min 命中、当天未触发过）：
 *      Screen_PushMemo() + ClockLink_SendMemo()
 *      + Screen_SetStatus("提醒: HH:MM") + Screen_ShowPage(1)
 * ====================================================================== */

#define EXT_RPT_RAM_MAX    8u    /* RAM 里最多几条                      */
#define EXT_RPT_SAVE_MAX   3u    /* 掉电保存几条（AT24C02 只有 256 字节） */
#define EXT_RPT_OFFSET     0x80u /* 本模块在 AT24C02 里的起始地址        */

#define EXT_RPT_TEXT_LEN   36u   /* 单条文本缓冲，含结尾 0               */
#define EXT_RPT_TEXT_MAX   35u   /* 文本最大字节数，不含结尾 0           */
#define EXT_RPT_REC_LEN    38u   /* 单条记录长度 = 1 + 1 + 36            */

/* 0x80 + 3*38 = 0xF2 */
#define EXT_RPT_MAGIC_ADDR ((u16)(EXT_RPT_OFFSET + EXT_RPT_SAVE_MAX * EXT_RPT_REC_LEN))
#define EXT_RPT_CNT_ADDR   ((u16)(EXT_RPT_MAGIC_ADDR + 1u))   /* 0xF3 */
#define EXT_RPT_CHK_ADDR   ((u16)(EXT_RPT_MAGIC_ADDR + 2u))   /* 0xF4 */
#define EXT_RPT_MAGIC      0xA5u

/* 上电恢复：从 AT24C02 读回前 3 条。必须在 AT24CXX_Init() 之后调用 */
void ExtRepeat_Init(void);

/* 20ms 节拍：到点触发（到点 = sec == 0 且 hour/min 命中且当天没响过） */
void ExtRepeat_Tick20ms(void);

/* 新增一条：返回 1 成功，0 失败。会写 EEPROM，主循环调用 */
uint8_t ExtRepeat_Add(uint8_t hour, uint8_t min, const char *gbk_text);

/* 删除第 idx 条（0..Count-1）：返回 1 成功，0 失败。会写 EEPROM */
uint8_t ExtRepeat_Remove(uint8_t idx);

/* 当前条数 0..EXT_RPT_RAM_MAX */
uint8_t ExtRepeat_Count(void);

/* 读第 idx 条：越界时 hour/min 给 0、text 给空串。
 * text 指向模块内部 static 缓冲，调用方要用请立即拷贝，不要长期保存。 */
void ExtRepeat_Get(uint8_t idx, uint8_t *hour, uint8_t *min, const char **text);

/* 清空全部：返回后 Count() == 0。会写 EEPROM，主循环调用 */
void ExtRepeat_ClearAll(void);

#endif
