#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "BlueTooth.h"
#include "Servo.h"
#include "PetAction.h"
#include "Face_Config.h"
#include "PowerDetection.h"
#include "Variable.h"
#include "Led_Breathing.h"
#include "Timer.h"
#include "Voice.h"
#include "Voice_TTS.h"
#include "home.h"
#include "PetLink.h"
#include "Guard.h"

/***************************************************************************************
  * 本程序由博主我Sngels_wyh创建并免费开源共享
  * 你可以任意查看、使用和修改，并应用到自己的项目之中
  * 
  * 程序名称：				基于STM32F103C8T6单片机的桌面宠物小项目
  * 此程序更新时间：			2026.3.23
  * 
  * 主要在立创开源平台、CSDN网站、哔哩哔哩、抖音分享自己的作品。
	*	相关的资料可以去立创开源硬件平台网站上搜索,作品附件放置更新网盘，如有更新可方便快速查看
	* 地址:https://oshwhub.com/sngelswyh/stm32-smart-desktop-pet
  ***************************************************************************************/

int main(void)
{	
	GetBattery_Init();//电量采样初始化
	Charge_Detect_Init();//充电检测(PB12)初始化
	Servo_Init();//舵机初始化
	OLED_Init();//OLED初始化
	Timer_Init();//定时器初始化
	BlueTooth_Init();//蓝牙(USART3)初始化
	Home_Init();//低电量自动归航状态机
	PetLink_Init();//时钟-宠物链路(AA55 帧)初始化
	Voice_Init();//CI-03T 离线语音(USART1 9600)
	TTS_Init();//SYN6288 中文播报
	Guard_Init();//超声波避障 + 电子围栏(PA5/PA15/PA8/PA11)

	OLED_ShowImage(0,0,128,64,Face_eyes);
	OLED_Update();
	while(1)
	{	
		/*显示表情(电量)*/
		Face_Config();
		/*超声波测距（阻塞 1~25ms，只放主循环）*/
		Guard_Poll();
		/*桌沿/障碍：先中止归航，再后退一步，最后站定*/
		if(Guard_ShouldRetreat())
		{
			if(Home_GetState()!=HOME_IDLE){Home_Abort();}
			PetAction_Judgmengt(0x34);//后退
			Delay_ms(250);
			PetAction_Judgmengt(0x31);//站立
		}
		/*不后退但被挡：左右试探绕过去*/
		else if(Guard_BlockForward()){Guard_AvoidOnce();}
		/*执行动作*/
		PetAction_Perform();
	}
}

/*每20ms进一次中断*/
void TIM3_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM3,TIM_IT_Update)==SET)
	{	
		/*电量检测*/
		GetCur_Power();
		/*呼吸灯*/
		LED_Breathing();
		/*低电归航状态机（20ms 推进，含四足步态）*/
		Home_Tick();
		/*时钟-宠物链路节拍*/
		PetLink_Tick20ms();
		/*SYN6288 播放锁超时*/
		TTS_Tick20ms();

		TIM_ClearITPendingBit(TIM3,TIM_IT_Update);
	}
}
