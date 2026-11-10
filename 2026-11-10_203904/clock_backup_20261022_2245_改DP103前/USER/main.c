#include "stm32f10x.h"
#include "wifi.h"
#include "clock_link.h"
#include "screen_ui.h"
/***********************************************************
基于物联网的辉光管时钟系统

作者：酷酷的胖~  QQ：2328155252

转载代码需声明来处  严禁商用
************************************************************/

//以下函数的在SYSTEM-control.c中
void My_Init(void);			//系统外设初始化函数
void key_control(void);	//按键控制函数
void working(void);			//工作模式
void HGG_Display(void);	//辉光管显示
void Atmosphere_Scan(void);//氛围灯刷新
void LCD_Scan(void);//屏幕刷新

int main(void)
{		
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //设置NVIC中断分组2: 2位抢占优先级，2位响应优先级		
	My_Init();//系统初始化
	ClockLink_Init();//时钟-宠物链路(USART3)初始化
	Screen_Init();//5寸串口屏UI初始化
	
	while(1)
	{
		wifi_uart_service();//涂鸦模组数据处理
		ClockLink_Poll();//解析宠物端上报
		Screen_Poll();//处理屏幕触摸
	}
}

//定时器2中断服务程序
void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)  
		{
			ClockLink_Tick20ms();
			key_control();//按键控制函数	
			working();//控制函数
			Atmosphere_Scan();//氛围灯刷新
			TIM_ClearITPendingBit(TIM2, TIM_IT_Update  );  
		}
} 

//定时器3中断服务程序
void TIM3_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) 
		{
			Screen_Tick100ms();
			TIM_ClearITPendingBit(TIM3, TIM_IT_Update  );
		}
}
