#ifndef __HOME_H_
#define __HOME_H_

#include "stm32f10x.h"

/* 归航状态机的 6 个状态 */
typedef enum
{
	HOME_IDLE = 0,      /* 正常待机 */
	HOME_LOWBAT,        /* 低电，准备返航 */
	HOME_SCAN,          /* 原地慢转，找红外信标 */
	HOME_HOMING,        /* 对着信标修正方向靠近 */
	HOME_ALIGN,         /* 进 V 槽，低速顶到触点 */
	HOME_CHARGING,      /* 已接触，充电中 */
	HOME_FAULT          /* 超时/失败，停在原地等人处理 */
} Home_State_t;

void Home_Init(void);                 /* 初始化：引脚 + 状态 + 日志 */
void Home_Tick(void);                 /* 20ms 调用一次，全部逻辑在状态机里 */
Home_State_t Home_GetState(void);
const char*  Home_StateName(Home_State_t s);

void Home_Start(void);                /* 手动触发返航（语音/蓝牙命令用） */
void Home_Abort(void);                /* 中止返航，回到 IDLE */
void Home_SimBattery(uint16_t pct);   /* 离线测试：设定模拟电量 */

/* 运动输出：默认只记录，留给四足步态/轮式驱动去实现 */
void Home_ApplyMotion(int16_t v, int16_t w);   /* v: 前后 -100..100  w: 转向 -100..100 */

#endif
