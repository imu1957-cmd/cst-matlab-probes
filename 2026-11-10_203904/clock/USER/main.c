#include "stm32f10x.h"
#include "wifi.h"
#include "clock_link.h"
#include "screen_ui.h"
#include "ext_env.h"
#include "ext_demo.h"
#include "ext_pomo.h"
#include "ext_repeat.h"
#include "ext_diag.h"
#include "ext_weather.h"
#include "ext_linkstat.h"
#include "codec.h"
#include "led_fx.h"
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

static char s_memo_gbk[96];   /* memo 转 GBK 后的缓冲 */
int main(void)
{		
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //设置NVIC中断分组2: 2位抢占优先级，2位响应优先级		
	My_Init();//系统初始化

	LedFx_Init();                /* LED status lights (WS2812 on PA7) */
	ClockLink_Init();
	LinkStat_Init();              /* 链路统计 *///时钟-宠物链路(USART3)初始化
	Screen_Init();//5寸串口屏UI初始化
	ExtEnv_Init();//扩展：夜间自动亮度 + 温湿度曲线
	ExtDemo_Init();   /* 演示模式 */
	ExtPomo_Init();   /* 番茄钟 */
	ExtRepeat_Init(); /* 恢复掉电保存的定时提醒 */
	Diag_Init();
	Weather_Init();               /* weather: 初始显示 "--" */      /* 调试/自检模块 */
	
	while(1)
	{
		wifi_uart_service();//涂鸦模组数据处理
		ClockLink_Poll();//解析宠物端上报
		Screen_Poll();
		Diag_Poll();      /* 自检 + 串口命令 + 喂狗 */

		if (Tuya_TakeMemo())          /* 云端下发备忘：UTF-8 转 GBK 后上屏 + 转发宠物 */
		{
			Utf8ToGbk(Tuya_GetMemo(), s_memo_gbk, sizeof(s_memo_gbk));
			Screen_PushMemo(s_memo_gbk);
			ClockLink_SendMemo(s_memo_gbk);
			Diag_TryAddReminder(s_memo_gbk);  /* "15:00 组会" 自动建提醒 */
		}//处理屏幕触摸
	}
}

//定时器2中断服务程序
void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)  
		{
			ClockLink_Tick20ms();
			LinkStat_Tick20ms();
			key_control();//按键控制函数	
			working();//控制函数
			ExtEnv_Tick20ms();
			ExtDemo_Tick20ms();
			ExtPomo_Tick20ms();
			ExtRepeat_Tick20ms();//扩展：亮度/曲线采样
			Diag_Tick20ms();
		Weather_Tick20ms();       /* weather: 120s 超时上屏去重 */
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
			LedFx_Tick100ms();

			/* page4 番茄钟：每 100ms 刷新（内部按秒去重）；曲线每 60s 推一点 */
			{
				static uint16_t s_curve_div = 0;
				if(Screen_GetPage() == PG_POMO)
				{
					Screen_PomoShow(ExtPomo_RemainSec(), ExtPomo_IsResting());
					if(++s_curve_div >= 600)
					{
						uint8_t cnt = ExtEnv_CurveCount();
						s_curve_div = 0;
						if(cnt > 0)
						{
							int16_t t10 = 0; uint8_t h = 0, mo = 0;
							ExtEnv_CurveGet((uint8_t)(cnt - 1), &t10, &h, &mo);
							Screen_CurveAdd(t10, h);
						}
					}
				}
				else
				{
					s_curve_div = 0;
				}
			}
			TIM_ClearITPendingBit(TIM3, TIM_IT_Update  );
		}
}
