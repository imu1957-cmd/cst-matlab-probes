#ifndef __SERVO_H
#define __SERVO_H

#include <stdint.h>

void Servo_Init(void);
void Servo_Angle1(float Angle);
void Servo_Angle2(float Angle);
void Servo_Angle3(float Angle);
void Servo_Angle4(float Angle);

/* 限流版四路置角: 每次最多两路同时变化.
   先写前两路(PA0/PA1), 阻塞 step_ms 后再写后两路(PA2/PA3);
   目标角与上次写入值相同的通道直接跳过, 不计入同时动作数 */
void Servo_SetAnglesLimited(const int16_t a[4], uint16_t step_ms);

#endif

