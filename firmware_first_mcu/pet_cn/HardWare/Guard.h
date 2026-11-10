#ifndef __GUARD_H_
#define __GUARD_H_

#include "stm32f10x.h"

void     Guard_Init(void);
void     Guard_Tick20ms(void);
void     Guard_Poll(void);
uint16_t Guard_DistanceCm(void);
uint8_t  Guard_FenceHit(void);
uint8_t  Guard_BlockForward(void);
uint8_t  Guard_ShouldRetreat(void);
uint8_t  Guard_AvoidOnce(void);

#endif
