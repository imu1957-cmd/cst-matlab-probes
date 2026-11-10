#ifndef __EXT_POMO_H_
#define __EXT_POMO_H_

#include "stm32f10x.h"

/* ======================================================================
 *  番茄钟（工作 / 休息循环）
 *
 *  ── 接线（由 control.c / main.c 负责，本模块不碰现有文件）──
 *   1) main() 里 Screen_Init() 之后调用：
 *          ExtPomo_Init();
 *   2) TIM2_IRQHandler() 20ms 节拍里追加：
 *          ExtPomo_Tick20ms();
 *   3) 启动（0 表示用默认值，即 25 / 5 分钟）：
 *          ExtPomo_Start(25, 5);
 *   4) 手动停止：
 *          ExtPomo_Stop();
 *   5) working() 里建议用 ExtPomo_IsRunning() 屏蔽主流程对灯效和
 *      t5 状态行的每 20ms 抢写。
 *
 *  行为：
 *    工作阶段  状态行「剩余 MM:SS」，每秒刷一次（不是每 20ms）
 *    工作结束  「休息 M 分钟」+ 宠物打招呼(0x43) + 红灯提示 3 秒
 *    休息结束  「继续工作」+ 宠物左转(0x35) + 回到工作阶段，循环
 *
 *  全程非阻塞，只靠 20ms tick 累加；与 ext_demo 完全独立。
 *  文件编码：GBK（Keil 下中文注释不乱码）。
 * ====================================================================== */

void ExtPomo_Init(void);                                    /* 上电初始化 */
void ExtPomo_Tick20ms(void);                                /* 20ms 推进 */
void ExtPomo_Start(uint8_t work_min, uint8_t rest_min);     /* 启动，0 = 默认 */
void ExtPomo_Stop(void);                                    /* 停止并交回灯效 */
uint8_t ExtPomo_IsRunning(void);                            /* 1 = 计时中 */
uint8_t ExtPomo_IsResting(void);                            /* 1 = 休息阶段 */
uint16_t ExtPomo_RemainSec(void);                           /* 本阶段剩余秒 */

#endif
