#include "HV57708.h"

void Delay(u32 time)
{
	u32 i,j;
	for(i=0;i < time;i++)
		for(j=0;j < 250;j++);
}
/**
 * void HV57708_Init(bool HV57708_Mode)
 * 简介：HV57708 初始化
 * 功能：HV57708 初始化，并设置输出模式
 * 输入：
 *       HV57708_Mode:
 *           HV57708_MODE_NORMAL    - 标准输出模式
 *           HV57708_MODE_INVERTION - 反相输出模式
 * 输出：无
 */
void HV57708_Init(u8 HV57708_Mode)
{
	GPIO_InitTypeDef	GPIO_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_AFIO,ENABLE);  
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8|GPIO_Pin_11|GPIO_Pin_12;	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;														//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;														//速度50MHZ
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9;	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;														//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;														//速度50MHZ
	GPIO_Init(GPIOB,&GPIO_InitStructure);
	
/***********************************************************************************************************/
	
	HV57708_POL_L;

	HV57708_DI1_L;
	HV57708_DI2_L;
	HV57708_DI3_L;
	HV57708_DI4_L;


	HV57708_CLK_L;
	HV57708_LE_L;
	 
	
}

/**
 * void HV57708_SendData(u32 datapart1, u32 datapart2)
 * 简介：向 HV57708 发送数据
 * 功能：向 HV57708 发送 64 位的数据
 * 输入：
 *       datapart1 - 第一部分数据，32位
 *       datapart2 - 第二部分数据，32位
 * 输出：无
 */
void HV57708_SendData(u32 datapart2, u32 datapart1)
{
	u8 i;
	u32 tmp;
	tmp = datapart1;
  for(i=0; i < 8; i++)
	{
		HV57708_CLK_L;
 		if((tmp & 0x00000001) == 1) {
 			HV57708_DI4_H;
        } else {
            HV57708_DI4_L;
        }
 		tmp = tmp >> 1;
 		if((tmp & 0x00000001) == 1) {
 			HV57708_DI3_H;
        } else {
            HV57708_DI3_L;
        }
        tmp = tmp >> 1;
        if((tmp & 0x00000001) == 1) {
 			HV57708_DI2_H;
        } else {
            HV57708_DI2_L;
        }
        tmp = tmp >> 1;
        if((tmp & 0x00000001) == 1) {
 			HV57708_DI1_H;
        } else {
            HV57708_DI1_L;
        }
        tmp = tmp >> 1;
        
//		HV57708_DI1_H;
//		HV57708_DI2_L;
//		HV57708_DI3_H;
//		HV57708_DI4_H;
		
	Delay(20);
		HV57708_CLK_H;
	Delay(20);

		HV57708_CLK_L;
	}
	tmp = datapart2;
  for(i=0; i < 8; i++)
	{
		HV57708_CLK_L;
 		if((tmp & 0x00000001) == 1) {
 			HV57708_DI4_H;
        } else {
            HV57708_DI4_L;
        }
 		tmp = tmp >> 1;
 		if((tmp & 0x00000001) == 1) {
 			HV57708_DI3_H;
        } else {
            HV57708_DI3_L;
        }
        tmp = tmp >> 1;
        if((tmp & 0x00000001) == 1) {
 			HV57708_DI2_H;
        } else {
            HV57708_DI2_L;
        }
        tmp = tmp >> 1;
        if((tmp & 0x00000001) == 1) {
 			HV57708_DI1_H;
        } else {
            HV57708_DI1_L;
        }
        tmp = tmp >> 1;
//		HV57708_DI1_H;
//		HV57708_DI2_H;
//		HV57708_DI3_H;
//		HV57708_DI4_H;

			Delay(20);

		HV57708_CLK_H;
			Delay(20);

		HV57708_CLK_L;
			Delay(20);		
	}
}

/**
 * void HV57708_OutputData(void)
 * 简介：将 HV57708 寄存器中的数据输出至引脚
 * 功能：将 HV57708 寄存器中的数据输出至引脚
 * 输入：无
 * 输出：无
 */
void HV57708_OutputData(void)
{
	HV57708_LE_H;
	Delay(50);
	HV57708_LE_L;
	Delay(20);
}
