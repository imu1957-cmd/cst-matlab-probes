#ifndef __PET_LINK_H_
#define __PET_LINK_H_

#include "stm32f10x.h"

/* 宠物端链路：USART3(PB10/PB11) <-> 时钟端
   中断只做「收字节进解析器」，派发放在主循环 PetLink_Poll() 里 */

void PetLink_Init(void);
uint8_t PetLink_RxByte(uint8_t b);    /* USART3 中断里调用；返回 1=属于帧，0=单字节指令 */
uint8_t PetLink_InFrame(void);        /* 1=正在收一帧（此时不要当单字节指令处理） */
void PetLink_Poll(void);              /* 主循环里调用，解析并派发 */
void PetLink_Tick20ms(void);          /* 20ms 定时器里调用，负责定时上报 */

void PetLink_SendBattery(uint16_t pct);
void PetLink_SendHomeState(uint8_t state, uint16_t pct);
void PetLink_SendAck(uint8_t seq, uint8_t ok);
void PetLink_SendEvent(uint8_t code);

uint16_t PetLink_GetCrcErrors(void);

/* 备忘录：时钟下发后放在这两个变量里，语音（SYN6288）以后从这里取 */
extern char    PetLink_Memo[66];
extern uint8_t PetLink_MemoLen;
extern uint8_t PetLink_MemoNew;

#endif
