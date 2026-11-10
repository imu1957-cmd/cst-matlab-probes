#ifndef __CLOCK_LINK_H_
#define __CLOCK_LINK_H_

#include "stm32f10x.h"

/* 时钟端链路：USART3 (PB10=TX, PB11=RX, 115200) <-> 宠物端 HC-05/JDY-31 */

void ClockLink_Init(void);
void ClockLink_Tick20ms(void);              /* TIM2 20ms 里调用：重发/超时/心跳 */
void ClockLink_Poll(void);                  /* 主循环调用：解析并派发 */

void ClockLink_SendMemo(const char *text);  /* 下发备忘录（<=64 字节） */
void ClockLink_SendCmd(uint8_t cmd);        /* 下发动作命令字 0x29..0x50 */
void ClockLink_QueryBattery(void);
void ClockLink_StartHome(void);             /* 叫宠物回充 */

/* 宠物上报的数据（屏上显示用） */
extern uint8_t ClockLink_PetBat;            /* 宠物电量 % */
extern uint8_t ClockLink_PetOnline;         /* 1=最近 10s 内收到过帧 */
extern uint8_t ClockLink_HomeState;         /* 0xFF=未知，其余同宠物端状态机 */

#endif
