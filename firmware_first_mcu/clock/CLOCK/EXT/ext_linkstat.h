#ifndef __EXT_LINKSTAT_H__
#define __EXT_LINKSTAT_H__

#include "stm32f10x.h"

/* ======================================================================
 * 扩展功能：链路质量统计（时钟 <-> 宠物 自定义帧链路，帧协议见 LINK/link_proto.h）
 * 目标：STM32F103 标准外设库 3.5.0，Keil armcc5（C89），GBK 源文件
 *
 * 用途：
 *   - 答辩展示"误帧 0/100"：窗口内最近 100 帧的错帧数
 *   - 现场排障：串口打印一行 [LK] 统计；屏幕显示 "LK OK e0" / "LK DOWN"
 *
 * 接入方式（本模块不调用其它模块，只被别人调用）：
 *   1) main() 里 ClockLink_Init() 之后调用一次：
 *          LinkStat_Init();
 *   2) TIM2_IRQHandler() 的 20ms 节拍里追加一行：
 *          LinkStat_Tick20ms();
 *   3) clock_link.c 的 USART3_IRQHandler() 里，每收完一帧调用：
 *          LinkStat_OnFrameRx(1);   好帧
 *          LinkStat_OnFrameRx(0);   CRC 错 / 长度格式错
 *   4) clock_link.c 的 ClockLink_Tick20ms() 重发分支里调用：
 *          LinkStat_OnTimeout();    每等待应答超时一次
 *          LinkStat_OnRetry();      每真正重发一次
 *   5) 命令台（ext_diag.c）的 ":lstat" 分支里调用：
 *          LinkStat_Report();
 *
 * 硬约束：
 *   - 无动态内存（malloc）、无浮点、无 Delay
 *   - 全部状态为文件内 static，统一 s_ 前缀，RAM 约 80 字节
 *   - LinkStat_OnFrameRx()/LinkStat_Tick20ms() 可在中断里调用，内部不含
 *     printf/sprintf；StatusLine()/Report() 只放主循环里调用
 * ====================================================================== */

#define LINKSTAT_WIN        100u  /* 错帧率窗口：最近 100 帧 */
#define LINKSTAT_DOWN_SEC     5u  /* 距上次好帧 > 5 秒判为断链 */

void LinkStat_Init(void);                  /* 清零全部统计 */
void LinkStat_Tick20ms(void);              /* 20ms 节拍：维护 1s/1min 窗口与断链超时 */
void LinkStat_OnFrameRx(uint8_t ok);       /* 每收完一帧调用：1=好帧，0=CRC错/格式错 */
void LinkStat_OnRetry(void);               /* 每次重发调用 */
void LinkStat_OnTimeout(void);             /* 每次等待应答超时调用 */
uint16_t LinkStat_ErrRate_x100(void);      /* 错帧率（百分比 x100，0..10000；无收包返回 0） */
void LinkStat_Report(void);                /* 用 printf 打印一行 [LK] 统计 */
const char *LinkStat_StatusLine(void);     /* "LK OK eN" / "LK DOWN"，ASCII，可直接上屏 */

#endif
