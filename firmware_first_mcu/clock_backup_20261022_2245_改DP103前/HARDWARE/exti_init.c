#include "exti_init.h"

void get_time(void);							//获取时间  //本地校时 定时器中断中每一小时执行一次

/////////////////////////////////////////////////////////////////////////////////
//函数名：EXTI_key_init													  															 
//函数功能：对EXTI4与EXTI5的IO口配置  																 
//输入值：无   																																 
//输出值：无																																		 
/////////////////////////////////////////////////////////////////////////////////
void EXTI_key_init()
{
	GPIO_InitTypeDef  GPIO_InitStructure;											//定义结构体变量
	 
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	    //使能PA时钟
  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;			//EXTI5_KEY-->PA.5 端口配置
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 	 					//上拉输入
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;					//IO口速度为50MHz	
  GPIO_Init(GPIOA, &GPIO_InitStructure);			     					//初始化GPIOA.5
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;			//EXTI4_KEY-->PA.4 端口配置
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 	 					//上拉输入
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;					//IO口速度为50MHz	
  GPIO_Init(GPIOA, &GPIO_InitStructure);			     					//初始化GPIOA.4
}

/////////////////////////////////////////////////////////////////////////////////
//函数名：EXTI0_init													  															 
//函数功能：初始化EXTI4与EXTI5													 
//输入值：无   																																 
//输出值：无																																		 
/////////////////////////////////////////////////////////////////////////////////
void EXTI_init()
{
	EXTI_InitTypeDef EXTI_InitStructer;
	NVIC_InitTypeDef NVIC_InitStructer;//定义结构体变量
	
	//对EXTI0的IO口配置
	EXTI_key_init();
	
	//使能时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
	
	//中短线5配置
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA,GPIO_PinSource5);

	//初始化EXTI5
	EXTI_InitStructer.EXTI_Line=EXTI_Line5;//中断线:线5
	EXTI_InitStructer.EXTI_LineCmd=ENABLE;//使能
	EXTI_InitStructer.EXTI_Mode=EXTI_Mode_Interrupt;//模式：中断  另外一个是事件
	EXTI_InitStructer.EXTI_Trigger=EXTI_Trigger_Falling;//触发方式：下降沿
	EXTI_Init(&EXTI_InitStructer);
	
	//中断5初始化配置
	NVIC_InitStructer.NVIC_IRQChannel=EXTI9_5_IRQn;
	NVIC_InitStructer.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructer.NVIC_IRQChannelPreemptionPriority=1;
	NVIC_InitStructer.NVIC_IRQChannelSubPriority=1;
	NVIC_Init(&NVIC_InitStructer);

}

/////////////////////////////////////////////////////////////////////////////////
//函数名：EXTI9_5_IRQHandler													  															 
//函数功能：EXTI5中断函数     																 
//输入值：无   																																 
//输出值：无																																		 
/////////////////////////////////////////////////////////////////////////////////
void EXTI9_5_IRQHandler()
{
	delay_ms(10);//消抖
	if(EXTI5_KEY==0)
	{
		
		//do you want
	}
		EXTI_ClearITPendingBit(EXTI_Line5);//清除中断0标志位
}
