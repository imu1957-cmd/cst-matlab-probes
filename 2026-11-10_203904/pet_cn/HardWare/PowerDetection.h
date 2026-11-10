#ifndef __POWER_DETECTION_H_
#define __POWER_DETECTION_H_

void GetBattery_Init(void);
uint16_t GetBattery(void);
void GetCur_Power(void);
	
#endif

/* 充电检测（PB12）：5V 输入经 10k 分压进 PA12/PB12，高电平=正在充电 */
void    Charge_Detect_Init(void);
uint8_t Charge_Detect(void);
