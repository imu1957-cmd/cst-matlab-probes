#include "stm32f10x.h"                  // Device header
#include "AD.h"
#include "Variable.h"



/*初始化ADC函数*/

void GetBattery_Init(void)
{
	AD_Init();
}

/*获取一次电量(平均前)*/
uint16_t GetBattery(void)
{
	Battery_Value=(3.3*4*Get_ADC()/4096)*100-300;
	return (uint16_t)Battery_Value;
}

/*获取平均电量(读取10次adc对应的电量值，再取平均数)*/
void GetCur_Power(void)
{
		if(++Battery_num<=100)
		{
			temp=GetBattery();
			temp1+=temp;
		}
		else
		{
			CurBattery=(uint16_t)(temp1/100);
			temp1=0;
			Battery_num=0;
		}
}

/* ======================================================================
 *  充电检测（真检测，替代 Face_Config.c 里 CurBattery>=110 的假判断）
 *
 *  硬件：车库 5V 充电电压经 10k 串到触点，宠物侧 10k 对地分压后接 PB12。
 *        吸合且车库通电 -> PB12 = 高电平（约 2.5V）；
 *        没吸合/车库断电 -> PB12 = 低电平（下拉）。
 *  注意：不要用 PA12，那是 USB D+；这里用 PB12。
 * ====================================================================== */
void Charge_Detect_Init(void)
{
	GPIO_InitTypeDef gpio;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	gpio.GPIO_Pin   = GPIO_Pin_12;
	gpio.GPIO_Mode  = GPIO_Mode_IPD;      /* 下拉，没接时读 0 */
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);
}

uint8_t Charge_Detect(void)
{
	static uint8_t cnt = 0;
	static uint8_t val = 0;
	uint8_t now = (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == Bit_SET) ? 1 : 0;

	/* 连续 3 次一致才认，防抖（20ms 调一次 = 60ms 去抖） */
	if(now == val) { if(cnt < 3) cnt++; }
	else { val = now; cnt = 0; }

	return (cnt >= 3) ? val : 0;
}
