#ifndef __EXT_DEMO_H_
#define __EXT_DEMO_H_

#include "stm32f10x.h"

/* ======================================================================
 *  一键演示模式（30 秒自动流程 + 2 秒结束语保持）
 *
 *  ── 接线（由 control.c / main.c 负责，本模块不碰现有文件）──
 *   1) main() 里 Screen_Init() 之后调用：
 *          ExtDemo_Init();
 *   2) TIM2_IRQHandler() 20ms 节拍里追加：
 *          ExtDemo_Tick20ms();
 *   3) 屏按钮 / 按键想触发演示时调用：
 *          ExtDemo_Start();        // 重复调用会重新开始，不会叠加
 *   4) working() 里用 ExtDemo_IsRunning() 屏蔽主流程对灯效和 t5 状态行
 *      的每 20ms 抢写（Screen_SetPetOnline 会无条件写 t5）。
 *
 *  流程（全部靠 20ms tick 累加，无任何 Delay，不阻塞主循环）：
 *    0s 主页 +「演示开始」 / 2s 推备忘录并转发宠物 / 6s 备忘录页
 *    10s 叫宠物回充 / 16s 充电灯效 / 22s 低电灯效 / 26s 待机灯效
 *    30s 主页 +「演示结束」 / 32s 退出（IsRunning 归 0）
 *
 *  文件编码：GBK（Keil 下中文注释不乱码）。
 * ====================================================================== */

void ExtDemo_Init(void);        /* 上电初始化，清零状态 */
void ExtDemo_Tick20ms(void);    /* 20ms 节拍推进，须在 TIM2 中断里调用 */
void ExtDemo_Start(void);       /* 触发演示，重复调用 = 重新开始 */
uint8_t ExtDemo_IsRunning(void);/* 1 = 演示中（含结束语保持的 2 秒） */
void ExtDemo_Stop(void);        /* 中途强制退出，立即交回状态行 */

#endif
