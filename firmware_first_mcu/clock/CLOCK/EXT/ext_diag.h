#ifndef __EXT_DIAG_H
#define __EXT_DIAG_H
#include "stm32f10x.h"

/* 看门狗开关：1=开启（答辩/成品），0=关闭（调试，可单步） */
#define DIAG_WDG_EN        0
/* 自检开始延时（tick，20ms/tick）：100 = 2s，等模组/屏上电稳定 */
#define DIAG_START_TICKS   100

void Diag_Init(void);          /* 上电调用：清状态、看门狗初始化 */
void Diag_Tick20ms(void);      /* TIM2 中断里调用：计时 */
void Diag_Poll(void);          /* 主循环里调用：自检 + 命令解析 + 喂狗 */
void Diag_RxByte(uint8_t b);   /* USART1 接收中断里调用：行缓冲 */
uint8_t Diag_IsOk(void);       /* 1=自检通过 */
const char *Diag_ErrText(void);/* 失败项名称，成功返回 "OK" */

/* 解析 "HH:MM 文本"（GBK），成功则加入定时提醒并返回 1 */
uint8_t Diag_TryAddReminder(const char *gbk);

#endif
