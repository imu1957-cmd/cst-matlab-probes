#include "stm32f10x.h"
#include "delay.h"
#include "usart_init.h"
#include "rtc.h" 	
#include "timer_init.h"
#include <string.h>
#include "iic_init.h"
#include "sht30.h"
#include "screen_ui.h"
#include "led_fx.h"
#include "ext_demo.h"
#include "ext_pomo.h"
#include "clock_link.h"
#include "ws2812b_init.h"
#include "24Cxx_init.h"
#include "wifi.h"
#include "lcd.h"
#include "lcd_init.h"
#include "HV57708.h"

#define KEY1 0     /* PB11 已让给 USART3_RX（蓝牙），旧触摸键作废 */
#define KEY2 0     /* PB10 已让给 USART3_TX（蓝牙），旧触摸键作废 */
#define KEY3 PBin(2)

#define Colon_on 	PBout(3)=1;PBout(4)=1;PBout(5)=1;PAout(15)=1
#define Colon_off PBout(3)=0;PBout(4)=0;PBout(5)=0;PAout(15)=0
#define BOOST_on  PAout(0)=0
#define BOOST_off PAout(0)=1

void read_eeprom(void);
void write_eeprom(void);

u8 mod=1;//当前模式
u32 time=0;//定时器累计时间
u16 wether_time=0;//显示天气状态下累计值
u8 Ty_mod=0;//涂鸦模组工作模式

u8 duan_num=3;
u8 duan1_on_hour=9,duan1_on_min=00,duan1_off_hour=10,duan1_off_min=00;
u8 duan2_on_hour=13,duan2_on_min=00,duan2_off_hour=14,duan2_off_min=00;
u8 duan3_on_hour=19,duan3_on_min=00,duan3_off_hour=20,duan3_off_min=00;//辉光管显示时间  其设定值为首次初始化值

u8 mod5_cursor;//设置界面中 光标位置
u8 mod6_cursor;//辉光管显示时间设置界面中 光标位置
u8 mod7_cursor;//阴极防中毒设置界面中 光标位置
u8 mod8_cursor;//氛围灯配置界面中 光标位置

u8 Ty_humidity=0,Ty_windDir=0,Ty_conditionNum=0,Ty_aqi=0,Ty_windLevel=0,Ty_sunRise_hour=0,Ty_sunRise_min=0,Ty_sunSet_hour=0,Ty_sunSet_min=0;
int Ty_temp=0;//用于存储接收到的天气情况

u8 Protection_interval=10;//阴极防中毒程序间隔（分）
u16 Protection_interval_sec;//阴极防中毒程序间隔换算为秒
u8 Protection_duration=6;//阴极防中毒程序持续时间（秒） 
u8 Protection_duration_num;//阴极防中毒程序持续次数  其设定值为首次初始化值

u8 Atmosphere_mode=1;//氛围灯模式
u8 Atmosphere_brightness=50;//氛围灯亮度
u8 Atmosphere_color=1;//氛围灯主题色  其设定值为首次初始化值

u8 bj_display=0;//板上器件是否在时间内标记

u16 Protection_count=0;//保护程序计数
u16 temporary_time=0;//临时开启时间

u8 buf_pic;

void KEY_GPIO_Init()//按键初始化
{
	GPIO_InitTypeDef  GPIO_InitStructure;											//定义结构体变量
	 
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	    //使能PB时钟
  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;   /* 只留 KEY3(PB2)，PB10/PB11 归蓝牙 */			   
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD; 	 					//下拉输入
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	 				//IO口速度为50MHz
	
  GPIO_Init(GPIOB, &GPIO_InitStructure);			     					//初始化GPIOB
}

void OUT_GPIO_Init()//输出IO初始化
{
	GPIO_InitTypeDef  GPIO_InitStructure;											//定义结构体变量
	 
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	    //使能PB时钟
  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5;			   
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 	 					//下拉输入
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	 				//IO口速度为50MHz
	
  GPIO_Init(GPIOB, &GPIO_InitStructure);			     					//初始化GPIOB	
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	    //使能PA时钟
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;			   
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 	 					//下拉输入
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	 				//IO口速度为50MHz
	
  GPIO_Init(GPIOA, &GPIO_InitStructure);			     					//初始化GPIOA
  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;			   
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 	 					//开漏输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	 				//IO口速度为50MHz
	
  GPIO_Init(GPIOA, &GPIO_InitStructure);			     					//初始化GPIOA
  
	
	BOOST_on;
	Colon_on;
}

void My_Init()//初始化系统
{
	delay_init();
	
	KEY_GPIO_Init();				//初始化触摸按键接口
	/* OUT_GPIO_Init 已删：冒号管/升压驱动脚已释放 */
	/* HV57708_Init 已删：辉光管移位寄存器在新板上不存在 */
	
	
	
	REG_LED_Init();					//初始化REG灯WS2812
	/* LCD_Init 已删：1.3 寸 LCD 换成 5 寸串口屏 */
	IIC_Init();							//初始化IIC接口
	SHT_Init();							//初始化温湿度传感器SHT30
	AT24CXX_Init();					//初始化存储器AT24C02
	RTC_Init();							//初始化内部RTC时钟
	
	if(AT24CXX_ReadOneByte(255) == 66)//判断是否首次使用EEPROM
		read_eeprom();
	else 
		write_eeprom();
	
	Protection_interval_sec=Protection_interval*60;//间隔分钟换算为秒
	Protection_duration_num=Protection_duration*10;//持续时间换算为一秒多少次
	
	USART1_init();					//初始化串口1 调试用
	USART2_init();					//初始化串口2 用于与涂鸦模组通信
	
	wifi_protocol_init();		//涂鸦模组初始化	
	
	TIM3_Int_Init(119,59999);	//初始化定时器3 间隔100ms
	TIM2_Int_Init(23,59999);	//初始化定时器2 间隔20ms
}

void read_eeprom()//读取EEPROM
{
	duan_num=AT24CXX_ReadOneByte(1);
	duan1_on_hour=AT24CXX_ReadOneByte(2);
	duan1_on_min=AT24CXX_ReadOneByte(3);
	duan1_off_hour=AT24CXX_ReadOneByte(4);
	duan1_off_min=AT24CXX_ReadOneByte(5);
	duan2_on_hour=AT24CXX_ReadOneByte(6);
	duan2_on_min=AT24CXX_ReadOneByte(7);
	duan2_off_hour=AT24CXX_ReadOneByte(8);
	duan2_off_min=AT24CXX_ReadOneByte(9);
	duan3_on_hour=AT24CXX_ReadOneByte(10);
	duan3_on_min=AT24CXX_ReadOneByte(11);
	duan3_off_hour=AT24CXX_ReadOneByte(12);
	duan3_off_min=AT24CXX_ReadOneByte(13);
	Protection_interval=AT24CXX_ReadOneByte(14);
	Protection_duration=AT24CXX_ReadOneByte(15);
	Atmosphere_mode=AT24CXX_ReadOneByte(16);
	Atmosphere_brightness=AT24CXX_ReadOneByte(17);
	Atmosphere_color=AT24CXX_ReadOneByte(18);
}
void write_eeprom()//写入EEPROM
{
	AT24CXX_WriteOneByte(1,duan_num);
	AT24CXX_WriteOneByte(2,duan1_on_hour);
	AT24CXX_WriteOneByte(3,duan1_on_min);
	AT24CXX_WriteOneByte(4,duan1_off_hour);
	AT24CXX_WriteOneByte(5,duan1_off_min);
	AT24CXX_WriteOneByte(6,duan2_on_hour);
	AT24CXX_WriteOneByte(7,duan2_on_min);
	AT24CXX_WriteOneByte(8,duan2_off_hour);
	AT24CXX_WriteOneByte(9,duan2_off_min);
	AT24CXX_WriteOneByte(10,duan3_on_hour);
	AT24CXX_WriteOneByte(11,duan3_on_min);
	AT24CXX_WriteOneByte(12,duan3_off_hour);
	AT24CXX_WriteOneByte(13,duan3_off_min);
	AT24CXX_WriteOneByte(14,Protection_interval);
	AT24CXX_WriteOneByte(15,Protection_duration);
	AT24CXX_WriteOneByte(16,Atmosphere_mode);
	AT24CXX_WriteOneByte(17,Atmosphere_brightness);
	AT24CXX_WriteOneByte(18,Atmosphere_color);
	
	AT24CXX_WriteOneByte(255,66);
}
void Conflict_protection()//阴极防中毒设置时间防冲突
{
	if(duan_num == 1)
	{
		if(duan1_on_hour*60+duan1_on_min >= duan1_off_hour*60+duan1_off_min)
			mod6_cursor=3;
		else
		{
			mod=5;
			write_eeprom();
		}
	}
	else if(duan_num == 2)
	{
		if(duan1_on_hour*60+duan1_on_min >= duan1_off_hour*60+duan1_off_min)
			mod6_cursor=3;
		else if(duan2_on_hour*60+duan2_on_min >= duan2_off_hour*60+duan2_off_min)
			mod6_cursor=12;
		else if(duan1_off_hour*60+duan1_off_min >= duan2_on_hour*60+duan2_on_min)
			mod6_cursor=3;
		else
		{
			mod=5;
			write_eeprom();
		}
	}
	else if(duan_num == 3)
	{
		if(duan1_on_hour*60+duan1_on_min >= duan1_off_hour*60+duan1_off_min)
			mod6_cursor=3;
		else if(duan2_on_hour*60+duan2_on_min >= duan2_off_hour*60+duan2_off_min)
			mod6_cursor=12;
		else if(duan3_on_hour*60+duan3_on_min >= duan3_off_hour*60+duan3_off_min)
			mod6_cursor=21;
		else if(duan1_off_hour*60+duan1_off_min >= duan2_on_hour*60+duan2_on_min)
			mod6_cursor=3;
		else if(duan2_off_hour*60+duan2_off_min >= duan3_on_hour*60+duan3_on_min)
			mod6_cursor=12;
		else
		{
			mod=5;
			write_eeprom();
		}
	}
	LCD_ShowChinese24x24(0,215,"白",WHITE,BLACK);
}

/* ======================================================================
 *  单触摸键 KEY3(PB2)：短按切页 / 双击清备忘录 / 长按 2s 调亮度
 *  （KEY1/KEY2 已让给 USART3 蓝牙，原 LCD 页面逻辑废弃）
 *  由 TIM2 的 20ms 节拍调用
 * ====================================================================== */
#define KEY_TICK_MS     20u
#define KEY_LONG_TICKS  (2000u / KEY_TICK_MS)   /* 长按 2s */
#define KEY_DBL_TICKS   (400u  / KEY_TICK_MS)   /* 双击间隔 400ms */
#define KEY_DEMO_TICKS  (4000u / KEY_TICK_MS)   /* 长按 4s = 启动演示 */
#define KEY_STABLE_CNT  2u                      /* 40ms 消抖 */

void key_control()//按键控制函数（单键：KEY3 / PB2）
{
	static u8  s_last = 0;      /* 上次电平 */
	static u8  s_stable = 0;    /* 消抖计数 */
	static u8  s_hold = 0;      /* 按住 tick 数 */
	static u8  s_vlong_hit = 0; /* 长按已触发演示 */
	static u8  s_long_hit = 0;  /* 本次按住已触发长按 */
	static u8  s_idle = 0;      /* 松开后计时（判双击） */
	static u8  s_click = 0;     /* 点击数 */
	static u8  s_page = 0;      /* 当前页 */
	static u8  s_bright = 2;    /* 2=100% 1=60% 0=30% */

	u8 now = (KEY3 == 1) ? 1 : 0;   /* PB2 下拉输入：按下=高 */

	/* --- 电平变化：重新消抖 --- */
	if(now != s_last)
	{
		s_stable = 0;
		s_last = now;
		return;
	}
	if(s_stable < KEY_STABLE_CNT)
	{
		s_stable++;
		return;
	}

	/* --- 按住 --- */
	if(now)
	{
		if(s_hold == 0)
		{
			s_long_hit = 0;
			s_vlong_hit = 0;
			s_idle = 0;
		}
		if(s_hold < 250) s_hold++;

		if(!s_long_hit && s_hold >= KEY_LONG_TICKS)
		{
			s_long_hit = 1;
			s_bright = (s_bright == 0) ? 2 : (u8)(s_bright - 1);
			Screen_SetBrightness((s_bright == 2) ? 100 : ((s_bright == 1) ? 60 : 30));
		}

		/* 继续按住到 4s：启动一次 30s 演示 */
		if(!s_vlong_hit && s_hold >= KEY_DEMO_TICKS)
		{
			s_vlong_hit = 1;
			ExtDemo_Start();
			Screen_SetStatus("DEMO ON");
		}
		return;
	}

	/* --- 松开 --- */
	if(s_hold)
	{
		if(!s_long_hit)
		{
			s_click++;
			s_idle = 0;
			if(s_click >= 2)
			{
				s_click = 0;
				Screen_ClearMemo();              /* 双击：清空备忘录 */
				Screen_SetStatus("MEMO CLR");
			}
		}
		s_hold = 0;
		return;
	}

	/* --- 空闲：双击窗口过去后判单击 --- */
	if(s_click == 1)
	{
		if(++s_idle >= KEY_DBL_TICKS)
		{
			s_click = 0;
			s_page = (u8)((s_page + 1) & 0x03);
			Screen_ShowPage(s_page);             /* 短按：切页 0->1->2->3->0 */
		}
	}
}



void LCD_Scan()//屏幕刷新
{
	static u8 buf_mod,buf_mod5_cursor;
	static u16 buf_temp, buf_hum;
	static u8 buf_hour, buf_min, buf_sec, buf_month, buf_date, buf_week;
	static u8 buf_Ty_humidity,buf_Ty_windDir,buf_Ty_conditionNum,buf_Ty_aqi,buf_Ty_windLevel,buf_Ty_sunRise_hour,buf_Ty_sunRise_min,buf_Ty_sunSet_hour,buf_Ty_sunSet_min;
	
	static u8 buf_duan_num,buf_mod6_cursor;
	static u8 buf_duan1_on_hour,buf_duan1_on_min,buf_duan1_off_hour,buf_duan1_off_min;
	static u8 buf_duan2_on_hour,buf_duan2_on_min,buf_duan2_off_hour,buf_duan2_off_min;
	static u8 buf_duan3_on_hour,buf_duan3_on_min,buf_duan3_off_hour,buf_duan3_off_min;
	
	static int buf_Ty_temp;
	
	static u8 buf_mod7_cursor;
	static u8 buf_Protection_interval;
	static u8 buf_Protection_duration;
	
	static u8 buf_mod8_cursor;
	static u8 buf_Atmosphere_mode;
	static u8 buf_Atmosphere_brightness;
	static u8 buf_Atmosphere_color;
	
	if(mod == 1)//配网界面
	{
		if(buf_mod != mod)
		{
			buf_mod = 1;
			
			LCD_Fill(0,0,240,240,BLACK);//清空屏幕
			
			LCD_ShowChinese32x32(71,71,"请",BLACK,WHITE);
			LCD_ShowChinese32x32(103,71,"配",BLACK,WHITE);
			LCD_ShowChinese32x32(135,71,"网",BLACK,WHITE);
			LCD_ShowChinese32x32(13,123,"当",WHITE,BLACK);
			LCD_ShowChinese32x32(45,123,"前",WHITE,BLACK);
			LCD_ShowChinese32x32(77,123,"网",WHITE,BLACK);
			LCD_ShowChinese32x32(109,123,"络",WHITE,BLACK);
			LCD_ShowChinese32x32(141,123,"状",WHITE,BLACK);
			LCD_ShowChinese32x32(173,123,"态",WHITE,BLACK);
		}
		LCD_Num4020(205,121,Ty_mod,WHITE,BLACK);
	}
	if(mod == 3)//时间显示界面
	{
		if(buf_mod != mod)//判断是否刷新背景
		{
			buf_mod = 3;
			LCD_Fill(0,0,240,240,BLACK);//清空屏幕
			
			buf_temp=65535;
			buf_hum=65535;
			buf_hour=255; 
			buf_min=255; 
			buf_sec=255; 
			buf_month=255; 
			buf_date=255;
			buf_week=255;
			buf_pic=0;
			
			LCD_ShowChinese40x40(39,0,"月",WHITE,BLACK);
			LCD_ShowChinese40x40(119,0,"日",WHITE,BLACK);
			LCD_ShowChinese40x40(159,0,"周",WHITE,BLACK);
			
			LCD_Pic5050(0,119,1,WHITE,BLACK);//温度图标
			LCD_Pic5050(0,179,0,WHITE,BLACK);//湿度图标
			
			LCD_Fill(186,162,190,166,WHITE);//温度逗号
			LCD_Fill(186,222,190,226,WHITE);//湿度逗号
			
			Draw_Circle(224,134,5,WHITE);
			Draw_Circle(224,134,4,WHITE);//度数符号
			
			LCD_Num5025(214,179,10,WHITE,BLACK);//百分号
		}
		
		if(SHT30_temperature>800)
			SHT30_temperature=800;//防止气温超限
		if(buf_temp != SHT30_temperature)//温度变化
		{
			buf_temp = SHT30_temperature;
			if(buf_temp >= 100)
			{
				LCD_Num5025(137,119,buf_temp/100,WHITE,BLACK);
				LCD_Num5025(162,119,buf_temp/10%10,WHITE,BLACK);
				LCD_Num5025(189,119,buf_temp%10,WHITE,BLACK);
			}
			else if(buf_temp >= 10)
			{
				LCD_Fill(137,119,162,169,BLACK);//清空十位
				LCD_Num5025(162,119,buf_temp/10%10,WHITE,BLACK);
				LCD_Num5025(189,119,buf_temp%10,WHITE,BLACK);
			}
			else
			{
				LCD_Fill(137,119,162,169,BLACK);//清空十位
				LCD_Num5025(162,119,0,WHITE,BLACK);
				LCD_Num5025(189,119,buf_temp%10,WHITE,BLACK);
			}//温度数字刷新
			
			if(buf_temp < 150)
				LCD_strip80(52,144,buf_temp/10+20,WHITE,DARKBLUE);
			else if(buf_temp < 200)
				LCD_strip80(52,144,buf_temp/10+20,WHITE,LIGHTBLUE);
			else if(buf_temp < 250)
				LCD_strip80(52,144,buf_temp/10+20,WHITE,ORANGE);
			else if(buf_temp < 800)
				LCD_strip80(52,144,buf_temp/10+20,WHITE,RED);//温度进度条
		}
		
		if(buf_hum != SHT30_humidity)//湿度变化
		{
			buf_hum = SHT30_humidity;
			if(buf_hum >= 100)
			{
				LCD_Num5025(137,179,buf_hum/100,WHITE,BLACK);
				LCD_Num5025(162,179,buf_hum/10%10,WHITE,BLACK);
				LCD_Num5025(189,179,buf_hum%10,WHITE,BLACK);
			}
			else if(buf_hum >= 10)
			{
				LCD_Fill(137,179,162,229,BLACK);//清空十位
				LCD_Num5025(162,179,buf_hum/10%10,WHITE,BLACK);
				LCD_Num5025(189,179,buf_hum%10,WHITE,BLACK);
			}
			else
			{
				LCD_Fill(137,179,162,229,BLACK);//清空十位
				LCD_Num5025(162,179,0,WHITE,BLACK);
				LCD_Num5025(189,179,buf_hum%10,WHITE,BLACK);
			}//湿度数字刷新
			LCD_strip80(52,204,buf_hum*8/100,WHITE,CYAN);//湿度进度条
		}
		
		
		if(buf_month != calendar.w_month)//刷新月份
		{
			buf_month=calendar.w_month;
			
			LCD_Num4020(0,0,buf_month/10,WHITE,BLACK);
			LCD_Num4020(19,0,buf_month%10,WHITE,BLACK);
		}
		
		if(buf_date != calendar.w_date)//刷新日期
		{
			buf_date=calendar.w_date;
			
			LCD_Num4020(79,0,buf_date/10,WHITE,BLACK);
			LCD_Num4020(99,0,buf_date%10,WHITE,BLACK);
		}
		
		if(buf_week != calendar.week)//刷新周
		{
			buf_week=calendar.week;
			
			switch(buf_week)
			{
				case 1:LCD_ShowChinese40x40(199,0,"一",WHITE,BLACK);break;
				case 2:LCD_ShowChinese40x40(199,0,"二",WHITE,BLACK);break;
				case 3:LCD_ShowChinese40x40(199,0,"三",WHITE,BLACK);break;
				case 4:LCD_ShowChinese40x40(199,0,"四",WHITE,BLACK);break;
				case 5:LCD_ShowChinese40x40(199,0,"五",WHITE,BLACK);break;
				case 6:LCD_ShowChinese40x40(199,0,"六",WHITE,BLACK);break;
				case 0:LCD_ShowChinese40x40(199,0,"日",WHITE,BLACK);break;
			}
		}
		
		if(bj_display==1)
		{
			if(buf_pic!=1)
			{
				LCD_ShowPicture(0,49,240,60);
				buf_pic=1;
			}
		}
		else
		{
			if(buf_pic!=2)
			{
				LCD_Fill(0,49,240,109,BLACK);
				buf_pic=2;
				buf_hour=255; 
				buf_min=255; 
				buf_sec=255; 
			}
			else
			{
				if(buf_hour != calendar.hour)//刷新小时
				{
					buf_hour=calendar.hour;
					
					LCD_Num6040(0,49,buf_hour/10,WHITE,BLACK);
					LCD_Num6040(39,49,buf_hour%10,WHITE,BLACK);
				}
				if(buf_min != calendar.min)//刷新分钟
				{
					buf_min=calendar.min;
					
					LCD_Num6040(89,49,buf_min/10,BRRED,BLACK);
					LCD_Num6040(129,49,buf_min%10,BRRED,BLACK);
				}
				if(buf_sec != calendar.sec)//刷新秒
				{
					buf_sec=calendar.sec;
					
					LCD_Num4530(179,64,buf_sec/10,WHITE,BLACK);
					LCD_Num4530(209,64,buf_sec%10,WHITE,BLACK);
				}	
			}
		}
	}
	
	if(mod == 4)//天气显示界面
	{
		if(buf_mod != mod)//判断是否首次刷新
		{
			buf_mod=4;
			LCD_Fill(0,0,240,240,BLACK);//清空屏幕
			
			buf_Ty_temp = 111;
			buf_Ty_humidity = 255;
			buf_Ty_windDir = 255;
			buf_Ty_conditionNum = 255;
			buf_Ty_aqi = 255;
			buf_Ty_windLevel = 255;
			buf_Ty_sunRise_hour = 255;
			buf_Ty_sunRise_min = 255;
			buf_Ty_sunSet_hour = 255;
			buf_Ty_sunSet_min = 255;
			
			LCD_Pic3636(199,45,11,WHITE,BLACK);//级
			
			LCD_Pic5050(0,132,1,WHITE,BLACK);//温度图标
			LCD_Pic5050(0,189,0,WHITE,BLACK);//湿度图标
			
			
			
			LCD_Num5025(214,189,10,WHITE,BLACK);//百分号
		}
		
		if(buf_Ty_aqi != Ty_aqi)//刷新空气质量指数
		{
			buf_Ty_aqi = Ty_aqi;
			
			if(buf_Ty_aqi >= 100)
			{
				LCD_Num3618(163,0,buf_Ty_aqi/100,WHITE,BLACK);
				LCD_Num3618(181,0,buf_Ty_aqi/10%10,WHITE,BLACK);
				LCD_Num3618(199,0,buf_Ty_aqi%10,WHITE,BLACK);
			}
			else if(buf_Ty_aqi >= 10)
			{
				LCD_Num3618(172,0,buf_Ty_aqi/10,WHITE,BLACK);
				LCD_Num3618(190,0,buf_Ty_aqi%10,WHITE,BLACK);			
			}
			else 
				LCD_Num3618(181,0,buf_Ty_aqi,WHITE,BLACK);
			
			if(buf_Ty_aqi<=50)
				LCD_Pic3636(127,0,10,GREEN,BLACK);
			else if(buf_Ty_aqi<=100)
				LCD_Pic3636(127,0,10,YELLOW,BLACK);
			else if(buf_Ty_aqi<=150)
				LCD_Pic3636(127,0,10,ORANGE,BLACK);
			else if(buf_Ty_aqi<=200)
				LCD_Pic3636(127,0,10,RED,BLACK);
			else if(buf_Ty_aqi<=300)
				LCD_Pic3636(127,0,10,PURPLE,BLACK);
			else
				LCD_Pic3636(127,0,10,ORANGE,BLACK);
		}
		
		if(buf_Ty_windDir != Ty_windDir)//判断是否刷新风向
		{
			buf_Ty_windDir = Ty_windDir;
			
			LCD_Pic3636(127,45,buf_Ty_windDir-1,WHITE,BLACK);//风向
		}
		
		if(buf_Ty_windLevel != Ty_windLevel)//判断是否刷新级数
		{
			buf_Ty_windLevel = Ty_windLevel;
			
			if(buf_Ty_windLevel>=10)
			{
				LCD_Num3618(163,45,buf_Ty_windLevel/10,WHITE,BLACK);
				LCD_Num3618(181,45,buf_Ty_windLevel%10,WHITE,BLACK);
			}
			else 
				LCD_Num3618(172,45,buf_Ty_windLevel,WHITE,BLACK);
		}
		
		if((calendar.hour>=9) && (calendar.hour<=20))//9点到八点显示太阳落山时间
		{
			if((buf_Ty_sunSet_hour != Ty_sunSet_hour) || (buf_Ty_sunSet_min != Ty_sunSet_min))
			{
				buf_Ty_sunSet_hour = Ty_sunSet_hour;
				buf_Ty_sunSet_min = Ty_sunSet_min;

				LCD_Num3618(163,91,buf_Ty_sunSet_hour/10,WHITE,BLACK);
				LCD_Num3618(181,91,buf_Ty_sunSet_hour%10,WHITE,BLACK);//时
				LCD_Fill(199,101,203,105,WHITE);
				LCD_Fill(199,113,203,117,WHITE);//冒号
				LCD_Num3618(203,91,buf_Ty_sunSet_min/10,WHITE,BLACK);
				LCD_Num3618(221,91,buf_Ty_sunSet_min%10,WHITE,BLACK);//分
				LCD_Pic3636(127,91,9,WHITE,BLACK);//日落
			}
		}
		else
		{
			if((buf_Ty_sunRise_hour != Ty_sunRise_hour) || (buf_Ty_sunRise_min != Ty_sunRise_min))
			{
				buf_Ty_sunRise_hour = Ty_sunRise_hour;
				buf_Ty_sunRise_min = Ty_sunRise_min;
				
				LCD_Num3618(163,91,buf_Ty_sunRise_hour/10,WHITE,BLACK);
				LCD_Num3618(181,91,buf_Ty_sunRise_hour%10,WHITE,BLACK);//时
				LCD_Fill(199,101,203,105,WHITE);
				LCD_Fill(199,113,203,117,WHITE);//冒号
				LCD_Num3618(203,91,buf_Ty_sunRise_min/10,WHITE,BLACK);
				LCD_Num3618(221,91,buf_Ty_sunRise_min%10,WHITE,BLACK);//分
				LCD_Pic3636(127,91,8,WHITE,BLACK);//日出
			}
		}

		if(buf_Ty_temp != Ty_temp)//判断是否需要刷新温度
		{
			buf_Ty_temp = Ty_temp;
			
			LCD_Fill(49,132,239,182,BLACK);//由于负温度 进度条大小不一致 清空区域
			
			if(buf_Ty_temp <= -10)
				LCD_strip80(54,157,buf_Ty_temp+20,WHITE,BLUE);
			else if(buf_Ty_temp < 15)
				LCD_strip100(56,157,(buf_Ty_temp+20)*100/80,WHITE,BLUE);
			else if(buf_Ty_temp < 20)
				LCD_strip100(56,157,(buf_Ty_temp+20)*100/80,WHITE,LIGHTBLUE);
			else if(buf_Ty_temp < 25)
				LCD_strip100(56,157,(buf_Ty_temp+20)*100/80,WHITE,ORANGE);
			else if(buf_Ty_temp < 80)
				LCD_strip100(56,157,(buf_Ty_temp+20)*100/80,WHITE,RED);//温度进度条
			
			if(buf_Ty_temp <= -10)
			{
				LCD_Num5025(139,132,11,WHITE,BLACK);//负号
				LCD_Num5025(164,132,(-buf_Ty_temp)/10%10,WHITE,BLACK);
				LCD_Num5025(189,132,(-buf_Ty_temp)%10,WHITE,BLACK);
				Draw_Circle(224,139,5,WHITE);
				Draw_Circle(224,139,4,WHITE);//度数符号
			}
			else if(buf_Ty_temp <= -1)
			{
				LCD_Num5025(164,132,11,WHITE,BLACK);//负号
				LCD_Num5025(189,132,(-buf_Ty_temp),WHITE,BLACK);
				Draw_Circle(224,139,5,WHITE);
				Draw_Circle(224,139,4,WHITE);//度数符号
			}
			else if(buf_Ty_temp < 10)
			{
				LCD_Num5025(189,132,buf_Ty_temp,WHITE,BLACK);
				Draw_Circle(224,139,5,WHITE);
				Draw_Circle(224,139,4,WHITE);//度数符号
			}else if(buf_Ty_temp < 100)
			{
				LCD_Num5025(164,132,buf_Ty_temp/10,WHITE,BLACK);//负号
				LCD_Num5025(189,132,buf_Ty_temp%10,WHITE,BLACK);
				Draw_Circle(224,139,5,WHITE);
				Draw_Circle(224,139,4,WHITE);//度数符号
			}	
		}

		if(buf_Ty_humidity != Ty_humidity)//判断湿度是否刷新
		{
			buf_Ty_humidity = Ty_humidity;
			
			if(buf_Ty_humidity >= 10)
			{
				LCD_Num5025(164,189,buf_Ty_humidity/10%10,WHITE,BLACK);
				LCD_Num5025(189,189,buf_Ty_humidity%10,WHITE,BLACK);
			}
			else
			{
				LCD_Fill(164,189,189,239,BLACK);//清空十位
				LCD_Num5025(189,189,buf_Ty_humidity%10,WHITE,BLACK);
			}//湿度数字刷新
			LCD_strip100(56,214,buf_Ty_humidity,WHITE,CYAN);//湿度进度条
		}

		if(buf_Ty_conditionNum != Ty_conditionNum)//判断天气是否需要刷新
		{
			buf_Ty_conditionNum = Ty_conditionNum;
			
			LCD_Fill(0,95,127,127,BLACK);//清空字部分
			if(buf_Ty_conditionNum  ==  105)
			{
				LCD_Pic9696(15,0,3,WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"雪",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  112)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  120)
			{
				LCD_Pic9696(15,0,6,WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"晴",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  121)
			{
				LCD_Pic9696(15,0,7,WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"雾",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  132)
			{
				LCD_Pic9696(15,0,7,WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"阴",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  140)
			{
				LCD_Pic9696(15,0,2,WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"霾",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  133)
			{
				LCD_Pic9696(15,0,5,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"冰",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"针",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  129)
			{
				LCD_Pic9696(15,0,7,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"少",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"云",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  142)
			{
				LCD_Pic9696(15,0,7,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"多",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"云",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  101)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"大",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  107)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"暴",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  122)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"阵",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  137)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"冻",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  139)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"小",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  141)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"中",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  104)
			{
				LCD_Pic9696(15,0,3,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"小",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雪",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  124)
			{
				LCD_Pic9696(15,0,3,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"大",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雪",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  126)
			{
				LCD_Pic9696(15,0,3,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"暴",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雪",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  131)
			{
				LCD_Pic9696(15,0,3,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"中",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雪",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  138)
			{
				LCD_Pic9696(15,0,3,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"阵",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雪",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  106)
			{
				LCD_Pic9696(15,0,5,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"冻",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雾",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  117)
			{
				LCD_Pic9696(15,0,2,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"扬",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"沙",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  115)
			{
				LCD_Pic9696(15,0,5,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"冰",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"粒",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  146)
			{
				LCD_Pic9696(15,0,6,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"晴",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"朗",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  110)
			{
				LCD_Pic9696(15,0,1,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"雷",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"电",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  109)
			{
				LCD_Pic9696(15,0,2,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"浮",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"尘",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  102)
			{
				LCD_Pic9696(15,0,1,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"雷",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"暴",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  127)
			{
				LCD_Pic9696(15,0,5,WHITE,BLACK);
				LCD_ShowChinese32x32(31,95,"冰",WHITE,BLACK);
				LCD_ShowChinese32x32(63,95,"雹",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  111)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(15,95,"小",WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"阵",WHITE,BLACK);
				LCD_ShowChinese32x32(79,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  123)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(15,95,"强",WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"阵",WHITE,BLACK);
				LCD_ShowChinese32x32(79,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  143)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(15,95,"雷",WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"阵",WHITE,BLACK);
				LCD_ShowChinese32x32(79,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  134)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(15,95,"大",WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"暴",WHITE,BLACK);
				LCD_ShowChinese32x32(79,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  130)
			{
				LCD_Pic9696(15,0,3,WHITE,BLACK);
				LCD_ShowChinese32x32(15,95,"小",WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"阵",WHITE,BLACK);
				LCD_ShowChinese32x32(79,95,"雪",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  113)
			{
				LCD_Pic9696(15,0,3,WHITE,BLACK);
				LCD_ShowChinese32x32(15,95,"雨",WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"夹",WHITE,BLACK);
				LCD_ShowChinese32x32(79,95,"雪",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  114)
			{
				LCD_Pic9696(15,0,2,WHITE,BLACK);
				LCD_ShowChinese32x32(15,95,"尘",WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"卷",WHITE,BLACK);
				LCD_ShowChinese32x32(79,95,"风",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  103)
			{
				LCD_Pic9696(15,0,2,WHITE,BLACK);
				LCD_ShowChinese32x32(15,95,"沙",WHITE,BLACK);
				LCD_ShowChinese32x32(47,95,"尘",WHITE,BLACK);
				LCD_ShowChinese32x32(79,95,"暴",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  118)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(0,95,"小",WHITE,BLACK);
				LCD_ShowChinese32x32(32,95,"到",WHITE,BLACK);
				LCD_ShowChinese32x32(64,95,"中",WHITE,BLACK);
				LCD_ShowChinese32x32(96,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  108)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(0,95,"局",WHITE,BLACK);
				LCD_ShowChinese32x32(32,95,"部",WHITE,BLACK);
				LCD_ShowChinese32x32(64,95,"阵",WHITE,BLACK);
				LCD_ShowChinese32x32(96,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  144)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(0,95,"中",WHITE,BLACK);
				LCD_ShowChinese32x32(32,95,"到",WHITE,BLACK);
				LCD_ShowChinese32x32(64,95,"大",WHITE,BLACK);
				LCD_ShowChinese32x32(96,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  145)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(0,95,"大",WHITE,BLACK);
				LCD_ShowChinese32x32(32,95,"到",WHITE,BLACK);
				LCD_ShowChinese32x32(64,95,"暴",WHITE,BLACK);
				LCD_ShowChinese32x32(96,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  125)
			{
				LCD_Pic9696(15,0,0,WHITE,BLACK);
				LCD_ShowChinese32x32(0,95,"特",WHITE,BLACK);
				LCD_ShowChinese32x32(32,95,"大",WHITE,BLACK);
				LCD_ShowChinese32x32(64,95,"暴",WHITE,BLACK);
				LCD_ShowChinese32x32(96,95,"雨",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  128)
			{
				LCD_Pic9696(15,0,3,WHITE,BLACK);
				LCD_ShowChinese32x32(0,95,"小",WHITE,BLACK);
				LCD_ShowChinese32x32(32,95,"到",WHITE,BLACK);
				LCD_ShowChinese32x32(64,95,"中",WHITE,BLACK);
				LCD_ShowChinese32x32(96,95,"雪",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  119)
			{
				LCD_Pic9696(15,0,6,WHITE,BLACK);
				LCD_ShowChinese32x32(0,95,"大",WHITE,BLACK);
				LCD_ShowChinese32x32(32,95,"部",WHITE,BLACK);
				LCD_ShowChinese32x32(64,95,"晴",WHITE,BLACK);
				LCD_ShowChinese32x32(96,95,"朗",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  116)
			{
				LCD_Pic9696(15,0,2,WHITE,BLACK);
				LCD_ShowChinese32x32(0,95,"强",WHITE,BLACK);
				LCD_ShowChinese32x32(32,95,"沙",WHITE,BLACK);
				LCD_ShowChinese32x32(64,95,"尘",WHITE,BLACK);
				LCD_ShowChinese32x32(96,95,"暴",WHITE,BLACK);
			}
			else if(buf_Ty_conditionNum  ==  136)
			{
				LCD_Pic9696(15,0,5,WHITE,BLACK);
				LCD_ShowChinese32x32(0,95,"阵",WHITE,BLACK);
				LCD_ShowChinese32x32(32,95,"雨",WHITE,BLACK);
				LCD_ShowChinese32x32(64,95,"冰",WHITE,BLACK);
				LCD_ShowChinese32x32(96,95,"雹",WHITE,BLACK);
			}
		}
		
	}//mod4结束
	
	
	
	
	
	

	if(mod == 5)//设置界面
	{
		if(buf_mod != mod)
		{
			buf_mod = 5;
			LCD_Fill(0,0,240,240,BLACK);//清空屏幕
			
			LCD_ShowChinese24x24(95,0,"设",BLACK,WHITE);
			LCD_ShowChinese24x24(119,0,"置",BLACK,WHITE);
			
			LCD_ShowChinese24x24(35,33,"辉",WHITE,BLACK);
			LCD_ShowChinese24x24(59,33,"光",WHITE,BLACK);
			LCD_ShowChinese24x24(83,33,"管",WHITE,BLACK);
			LCD_ShowChinese24x24(107,33,"显",WHITE,BLACK);
			LCD_ShowChinese24x24(131,33,"示",WHITE,BLACK);
			LCD_ShowChinese24x24(155,33,"时",WHITE,BLACK);
			LCD_ShowChinese24x24(179,33,"间",WHITE,BLACK);
			
			LCD_ShowChinese24x24(35,57,"阴",WHITE,BLACK);
			LCD_ShowChinese24x24(59,57,"极",WHITE,BLACK);
			LCD_ShowChinese24x24(83,57,"防",WHITE,BLACK);
			LCD_ShowChinese24x24(107,57,"中",WHITE,BLACK);
			LCD_ShowChinese24x24(131,57,"毒",WHITE,BLACK);
			LCD_ShowChinese24x24(155,57,"配",WHITE,BLACK);
			LCD_ShowChinese24x24(179,57,"置",WHITE,BLACK);
			
			LCD_ShowChinese24x24(47,81,"重",WHITE,BLACK);
			LCD_ShowChinese24x24(71,81,"置",WHITE,BLACK);
			LCD_ShowChinese24x24(95,81,"涂",WHITE,BLACK);
			LCD_ShowChinese24x24(119,81,"鸦",WHITE,BLACK);
			LCD_ShowChinese24x24(143,81,"模",WHITE,BLACK);
			LCD_ShowChinese24x24(167,81,"组",WHITE,BLACK);
			
			LCD_ShowChinese24x24(59,105,"氛",WHITE,BLACK);
			LCD_ShowChinese24x24(83,105,"围",WHITE,BLACK);
			LCD_ShowChinese24x24(107,105,"灯",WHITE,BLACK);
			LCD_ShowChinese24x24(131,105,"配",WHITE,BLACK);
			LCD_ShowChinese24x24(155,105,"置",WHITE,BLACK);
			
			LCD_ShowChinese24x24(95,215,"退",WHITE,BLACK);
			LCD_ShowChinese24x24(119,215,"回",WHITE,BLACK);
			
			buf_mod5_cursor=0;
		}
			
		if(buf_mod5_cursor != mod5_cursor)
		{
			switch(buf_mod5_cursor)
			{
				case 1: LCD_Fill(0,33,24,57,BLACK);break;
				case 2: LCD_Fill(0,57,24,81,BLACK);break;
				case 3: LCD_Fill(0,81,24,105,BLACK);break;
				case 4: LCD_Fill(0,105,24,239,BLACK);break;
				case 5: LCD_Fill(0,215,24,239,BLACK);break;
			}
			buf_mod5_cursor = mod5_cursor;
			switch(buf_mod5_cursor)
			{
				case 1: LCD_ShowChinese24x24(0,33,"空",WHITE,BLACK);break;
				case 2: LCD_ShowChinese24x24(0,57,"空",WHITE,BLACK);break;
				case 3: LCD_ShowChinese24x24(0,81,"空",WHITE,BLACK);break;
				case 4: LCD_ShowChinese24x24(0,105,"空",WHITE,BLACK);break;
				case 5: LCD_ShowChinese24x24(0,215,"空",WHITE,BLACK);break;
			}
		}
	}
	
	if(mod == 6)//辉光管显示时间段设置界面
	{
		if(buf_mod != mod)
		{
			buf_mod =6;
			LCD_Fill(0,0,240,240,BLACK);//清空屏幕
			
			LCD_ShowChinese24x24(23,0,"时",WHITE,BLACK);
			LCD_ShowChinese24x24(47,0,"间",WHITE,BLACK);
			LCD_ShowChinese24x24(71,0,"段",WHITE,BLACK);
			LCD_ShowChinese24x24(95,0,"数",WHITE,BLACK);
			
			LCD_ShowChinese24x24(23,35,"第",WHITE,BLACK);
			LCD_ShowChinese24x24(47,35,"一",WHITE,BLACK);
			LCD_ShowChinese24x24(71,35,"段",WHITE,BLACK);
			
			LCD_ShowChinese24x24(23,88,"第",WHITE,BLACK);
			LCD_ShowChinese24x24(47,88,"二",WHITE,BLACK);
			LCD_ShowChinese24x24(71,88,"段",WHITE,BLACK);
			
			LCD_ShowChinese24x24(23,141,"第",WHITE,BLACK);
			LCD_ShowChinese24x24(47,141,"三",WHITE,BLACK);
			LCD_ShowChinese24x24(71,141,"段",WHITE,BLACK);
			
			LCD_ShowChinese24x24(107,59,"到",WHITE,BLACK);
			LCD_ShowChinese24x24(107,112,"到",WHITE,BLACK);
			LCD_ShowChinese24x24(107,165,"到",WHITE,BLACK);
			
			LCD_ShowChinese24x24(95,215,"退",WHITE,BLACK);
			LCD_ShowChinese24x24(119,215,"回",WHITE,BLACK);
			
			LCD_colon(119,0);
			LCD_colon(95,35);
			LCD_colon(95,88);
			LCD_colon(95,141);
			LCD_colon(77,59);
			LCD_colon(155,59);
			LCD_colon(77,112);
			LCD_colon(155,112);
			LCD_colon(77,165);
			LCD_colon(155,165);
			
			buf_duan_num=255;buf_mod6_cursor=255;
			buf_duan1_on_hour=255,buf_duan1_on_min=255,buf_duan1_off_hour=255,buf_duan1_off_min=255;
			buf_duan2_on_hour=255,buf_duan2_on_min=255,buf_duan2_off_hour=255,buf_duan2_off_min=255;
			buf_duan3_on_hour=255,buf_duan3_on_min=255,buf_duan3_off_hour=255,buf_duan3_off_min=255;
		}
		
		if(buf_duan_num != duan_num)
		{
			buf_duan_num = duan_num;
			LCD_Num2412(125,0,buf_duan_num,WHITE,BLACK);
		}
		if(buf_duan1_on_hour != duan1_on_hour)
		{
			buf_duan1_on_hour = duan1_on_hour;
			LCD_Num2412(53,59,buf_duan1_on_hour/10,WHITE,BLACK);
			LCD_Num2412(65,59,buf_duan1_on_hour%10,WHITE,BLACK);
		}
		if(buf_duan1_on_min != duan1_on_min)
		{
			buf_duan1_on_min = duan1_on_min;
			LCD_Num2412(83,59,buf_duan1_on_min/10,WHITE,BLACK);
			LCD_Num2412(95,59,buf_duan1_on_min%10,WHITE,BLACK);
		}
		if(buf_duan1_off_hour != duan1_off_hour)
		{
			buf_duan1_off_hour = duan1_off_hour;
			LCD_Num2412(131,59,buf_duan1_off_hour/10,WHITE,BLACK);
			LCD_Num2412(143,59,buf_duan1_off_hour%10,WHITE,BLACK);
		}
		if(buf_duan1_off_min != duan1_off_min)
		{
			buf_duan1_off_min = duan1_off_min;
			LCD_Num2412(161,59,buf_duan1_off_min/10,WHITE,BLACK);
			LCD_Num2412(173,59,buf_duan1_off_min%10,WHITE,BLACK);
		}
		
		
		if(buf_duan2_on_hour != duan2_on_hour)
		{
			buf_duan2_on_hour = duan2_on_hour;
			LCD_Num2412(53,112,buf_duan2_on_hour/10,WHITE,BLACK);
			LCD_Num2412(65,112,buf_duan2_on_hour%10,WHITE,BLACK);
		}
		if(buf_duan2_on_min != duan2_on_min)
		{
			buf_duan2_on_min = duan2_on_min;
			LCD_Num2412(83,112,buf_duan2_on_min/10,WHITE,BLACK);
			LCD_Num2412(95,112,buf_duan2_on_min%10,WHITE,BLACK);
		}
		if(buf_duan2_off_hour != duan2_off_hour)
		{
			buf_duan2_off_hour = duan2_off_hour;
			LCD_Num2412(131,112,buf_duan2_off_hour/10,WHITE,BLACK);
			LCD_Num2412(143,112,buf_duan2_off_hour%10,WHITE,BLACK);
		}
		if(buf_duan2_off_min != duan2_off_min)
		{
			buf_duan2_off_min = duan2_off_min;
			LCD_Num2412(161,112,buf_duan2_off_min/10,WHITE,BLACK);
			LCD_Num2412(173,112,buf_duan2_off_min%10,WHITE,BLACK);
		}
		
		if(buf_duan3_on_hour != duan3_on_hour)
		{
			buf_duan3_on_hour = duan3_on_hour;
			LCD_Num2412(53,165,buf_duan3_on_hour/10,WHITE,BLACK);
			LCD_Num2412(65,165,buf_duan3_on_hour%10,WHITE,BLACK);
		}
		if(buf_duan3_on_min != duan3_on_min)
		{
			buf_duan3_on_min = duan3_on_min;
			LCD_Num2412(83,165,buf_duan3_on_min/10,WHITE,BLACK);
			LCD_Num2412(95,165,buf_duan3_on_min%10,WHITE,BLACK);
		}
		if(buf_duan3_off_hour != duan3_off_hour)
		{
			buf_duan3_off_hour = duan3_off_hour;
			LCD_Num2412(131,165,buf_duan3_off_hour/10,WHITE,BLACK);
			LCD_Num2412(143,165,buf_duan3_off_hour%10,WHITE,BLACK);
		}
		if(buf_duan3_off_min != duan3_off_min)
		{
			buf_duan3_off_min = duan3_off_min;
			LCD_Num2412(161,165,buf_duan3_off_min/10,WHITE,BLACK);
			LCD_Num2412(173,165,buf_duan3_off_min%10,WHITE,BLACK);
		}
		
		
		if(buf_mod6_cursor != mod6_cursor)
		{
			buf_mod6_cursor = mod6_cursor;
			switch(buf_mod6_cursor)
			{
				case 1:LCD_ShowChinese24x24(0,0,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,215,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,35,"白",WHITE,BLACK);LCD_underline(125,23,BLACK);break;
				case 2:LCD_underline(125,23,WHITE);LCD_ShowChinese24x24(0,0,"实",WHITE,BLACK);break;
				case 3:LCD_ShowChinese24x24(0,35,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,0,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,88,"白",WHITE,BLACK);LCD_underline(173,83,BLACK);break;
				case 4:LCD_underline(53,83,WHITE);LCD_ShowChinese24x24(0,35,"实",WHITE,BLACK);break;
				case 5:LCD_underline(65,83,WHITE);LCD_underline(53,83,BLACK);break;
				case 6:LCD_underline(83,83,WHITE);LCD_underline(65,83,BLACK);break;
				case 7:LCD_underline(95,83,WHITE);LCD_underline(83,83,BLACK);break;
				case 8:LCD_underline(131,83,WHITE);LCD_underline(95,83,BLACK);break;
				case 9:LCD_underline(143,83,WHITE);LCD_underline(131,83,BLACK);break;
				case 10:LCD_underline(161,83,WHITE);LCD_underline(143,83,BLACK);break;
				case 11:LCD_underline(173,83,WHITE);LCD_underline(161,83,BLACK);break;
				case 12:LCD_ShowChinese24x24(0,88,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,35,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,141,"白",WHITE,BLACK);LCD_underline(173,136,BLACK);break;	
				case 13:LCD_underline(53,136,WHITE);LCD_ShowChinese24x24(0,88,"实",WHITE,BLACK);break;
				case 14:LCD_underline(65,136,WHITE);LCD_underline(53,136,BLACK);break;
				case 15:LCD_underline(83,136,WHITE);LCD_underline(65,136,BLACK);break;
				case 16:LCD_underline(95,136,WHITE);LCD_underline(83,136,BLACK);break;
				case 17:LCD_underline(131,136,WHITE);LCD_underline(95,136,BLACK);break;
				case 18:LCD_underline(143,136,WHITE);LCD_underline(131,136,BLACK);break;
				case 19:LCD_underline(161,136,WHITE);LCD_underline(143,136,BLACK);break;
				case 20:LCD_underline(173,136,WHITE);LCD_underline(161,136,BLACK);break;
				case 21:LCD_ShowChinese24x24(0,141,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,88,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,215,"白",WHITE,BLACK);LCD_underline(173,189,BLACK);break;
				case 22:LCD_underline(53,189,WHITE);LCD_ShowChinese24x24(0,141,"实",WHITE,BLACK);break;
				case 23:LCD_underline(65,189,WHITE);LCD_underline(53,189,BLACK);break;
				case 24:LCD_underline(83,189,WHITE);LCD_underline(65,189,BLACK);break;
				case 25:LCD_underline(95,189,WHITE);LCD_underline(83,189,BLACK);break;
				case 26:LCD_underline(131,189,WHITE);LCD_underline(95,189,BLACK);break;
				case 27:LCD_underline(143,189,WHITE);LCD_underline(131,189,BLACK);break;
				case 28:LCD_underline(161,189,WHITE);LCD_underline(143,189,BLACK);break;
				case 29:LCD_underline(173,189,WHITE);LCD_underline(161,189,BLACK);break;
				case 30:LCD_ShowChinese24x24(0,215,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,141,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,0,"白",WHITE,BLACK);break;
		
			}
		}
//			LCD_underline
	}

	if(mod == 7)
	{
		if(buf_mod != mod)
		{
			buf_mod =7;
			LCD_Fill(0,0,240,240,BLACK);//清空屏幕
			
			LCD_ShowChinese24x24(23,0,"间",WHITE,BLACK);
			LCD_ShowChinese24x24(47,0,"隔",WHITE,BLACK);
			LCD_ShowChinese24x24(71,0,"时",WHITE,BLACK);
			LCD_ShowChinese24x24(95,0,"间",WHITE,BLACK);
			LCD_ShowChinese24x24(119,23,"分",WHITE,BLACK);
			
			LCD_ShowChinese24x24(23,53,"持",WHITE,BLACK);
			LCD_ShowChinese24x24(47,53,"续",WHITE,BLACK);
			LCD_ShowChinese24x24(71,53,"时",WHITE,BLACK);
			LCD_ShowChinese24x24(95,53,"间",WHITE,BLACK);
			LCD_ShowChinese24x24(119,76,"秒",WHITE,BLACK);
			
			LCD_ShowChinese24x24(95,215,"退",WHITE,BLACK);
			LCD_ShowChinese24x24(119,215,"回",WHITE,BLACK);
			
			LCD_colon(119,0);
			LCD_colon(119,53);
			
			buf_mod7_cursor=0;
			buf_Protection_interval=255;
			buf_Protection_duration=255;
		}
		
		if(buf_mod7_cursor != mod7_cursor)
		{
			buf_mod7_cursor = mod7_cursor;
			switch(buf_mod7_cursor)
			{
				case 1:LCD_ShowChinese24x24(0,0,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,215,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,52,"白",WHITE,BLACK);LCD_underline(95,47,BLACK);LCD_underline(107,47,BLACK);break;
				case 2:LCD_underline(95,47,WHITE);LCD_underline(107,47,WHITE);LCD_ShowChinese24x24(0,0,"实",WHITE,BLACK);break;
				case 3:LCD_ShowChinese24x24(0,52,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,0,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,215,"白",WHITE,BLACK);LCD_underline(95,100,BLACK);LCD_underline(107,100,BLACK);break;
				case 4:LCD_underline(95,100,WHITE);LCD_underline(107,100,WHITE);LCD_ShowChinese24x24(0,52,"实",WHITE,BLACK);break;
				case 5:LCD_ShowChinese24x24(0,215,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,0,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,52,"白",WHITE,BLACK);break;
			}
		}
		if(buf_Protection_interval != Protection_interval)
		{
			buf_Protection_interval = Protection_interval;
			
			if(buf_Protection_interval <10)
			{
				LCD_ShowChinese24x24(95,23,"白",WHITE,BLACK);
				LCD_Num2412(101,23,buf_Protection_interval,WHITE,BLACK);
			}
			else
			{
				LCD_Num2412(95,23,buf_Protection_interval/10,WHITE,BLACK);
				LCD_Num2412(107,23,buf_Protection_interval%10,WHITE,BLACK);
			}
		}
		
		if(buf_Protection_duration != Protection_duration)
		{
			buf_Protection_duration = Protection_duration;
			
			if(buf_Protection_duration <10)
			{
				LCD_ShowChinese24x24(95,75,"白",WHITE,BLACK);
				LCD_Num2412(101,75,buf_Protection_duration,WHITE,BLACK);
			}
			else
			{
				LCD_Num2412(95,75,buf_Protection_duration/10,WHITE,BLACK);
				LCD_Num2412(107,75,buf_Protection_duration%10,WHITE,BLACK);
			}
		}
	}
	if(mod == 8)
	{
		if(buf_mod != mod)
		{
			buf_mod =8;
			LCD_Fill(0,0,240,240,BLACK);//清空屏幕
			
			LCD_ShowChinese24x24(23,0,"模",WHITE,BLACK);
			LCD_ShowChinese24x24(47,0,"式",WHITE,BLACK);
			
			LCD_ShowChinese24x24(23,28,"亮",WHITE,BLACK);
			LCD_ShowChinese24x24(47,28,"度",WHITE,BLACK);
			
			LCD_ShowChinese24x24(23,57,"主",WHITE,BLACK);
			LCD_ShowChinese24x24(47,57,"题",WHITE,BLACK);
			LCD_ShowChinese24x24(71,57,"色",WHITE,BLACK);
			
			LCD_ShowChinese24x24(95,215,"退",WHITE,BLACK);
			LCD_ShowChinese24x24(119,215,"回",WHITE,BLACK);
			
			LCD_colon(71,0);
			LCD_colon(71,28);
			LCD_colon(95,57);
			
			buf_mod8_cursor=255;
			buf_Atmosphere_mode=255;
			buf_Atmosphere_brightness=255;
			buf_Atmosphere_color=255;
		}
		if(buf_mod8_cursor != mod8_cursor)
		{
			buf_mod8_cursor = mod8_cursor;
			switch(buf_mod8_cursor)
			{
				case 1:LCD_ShowChinese24x24(0,0,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,28,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,215,"白",WHITE,BLACK);LCD_underline(77,23,BLACK);LCD_underline(101,23,BLACK);LCD_underline(125,23,BLACK);LCD_underline(149,23,BLACK);LCD_underline(89,23,BLACK);LCD_underline(113,23,BLACK);LCD_underline(137,23,BLACK);LCD_underline(161,23,BLACK);break;
				case 2:LCD_ShowChinese24x24(0,0,"实",WHITE,BLACK);LCD_underline(77,23,WHITE);LCD_underline(101,23,WHITE);LCD_underline(125,23,WHITE);LCD_underline(149,23,WHITE);LCD_underline(89,23,WHITE);LCD_underline(113,23,WHITE);LCD_underline(137,23,WHITE);LCD_underline(161,23,WHITE);break;
				case 3:LCD_ShowChinese24x24(0,28,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,0,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,57,"白",WHITE,BLACK);LCD_underline(77,52,BLACK);LCD_underline(101,52,BLACK);LCD_underline(89,52,BLACK);break;
				case 4:LCD_ShowChinese24x24(0,28,"实",WHITE,BLACK);LCD_underline(77,52,WHITE);LCD_underline(101,52,WHITE);LCD_underline(89,52,WHITE);break;
				case 5:LCD_ShowChinese24x24(0,57,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,28,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,215,"白",WHITE,BLACK);LCD_underline(101,81,BLACK);LCD_underline(125,81,BLACK);LCD_underline(113,81,BLACK);LCD_underline(137,81,BLACK);break;
				case 6:LCD_ShowChinese24x24(0,57,"实",WHITE,BLACK);LCD_underline(101,81,WHITE);LCD_underline(125,81,WHITE);LCD_underline(113,81,WHITE);LCD_underline(137,81,WHITE);break;
				case 7:LCD_ShowChinese24x24(0,215,"空",WHITE,BLACK);LCD_ShowChinese24x24(0,0,"白",WHITE,BLACK);LCD_ShowChinese24x24(0,57,"白",WHITE,BLACK);break;
			}
		}
		if(buf_Atmosphere_mode != Atmosphere_mode)//模式变化刷新
		{
			buf_Atmosphere_mode = Atmosphere_mode;
			switch(buf_Atmosphere_mode)
			{
				case 1:LCD_ShowChinese24x24(77,0,"单",WHITE,BLACK);LCD_ShowChinese24x24(101,0,"色",WHITE,BLACK);LCD_ShowChinese24x24(125,0,"常",WHITE,BLACK);LCD_ShowChinese24x24(149,0,"亮",WHITE,BLACK);break;
				case 2:LCD_ShowChinese24x24(77,0,"六",WHITE,BLACK);LCD_ShowChinese24x24(101,0,"色",WHITE,BLACK);LCD_ShowChinese24x24(125,0,"常",WHITE,BLACK);LCD_ShowChinese24x24(149,0,"亮",WHITE,BLACK);break;
				case 3:LCD_ShowChinese24x24(77,0,"单",WHITE,BLACK);LCD_ShowChinese24x24(101,0,"色",WHITE,BLACK);LCD_ShowChinese24x24(125,0,"呼",WHITE,BLACK);LCD_ShowChinese24x24(149,0,"吸",WHITE,BLACK);break;
				case 4:LCD_ShowChinese24x24(77,0,"六",WHITE,BLACK);LCD_ShowChinese24x24(101,0,"色",WHITE,BLACK);LCD_ShowChinese24x24(125,0,"呼",WHITE,BLACK);LCD_ShowChinese24x24(149,0,"吸",WHITE,BLACK);break;
				case 5:LCD_ShowChinese24x24(77,0,"六",WHITE,BLACK);LCD_ShowChinese24x24(101,0,"色",WHITE,BLACK);LCD_ShowChinese24x24(125,0,"流",WHITE,BLACK);LCD_ShowChinese24x24(149,0,"水",WHITE,BLACK);break;
				case 6:LCD_ShowChinese24x24(77,0,"白",WHITE,BLACK);LCD_ShowChinese24x24(101,0,"白",WHITE,BLACK);LCD_ShowChinese24x24(125,0,"白",WHITE,BLACK);LCD_ShowChinese24x24(149,0,"白",WHITE,BLACK);LCD_ShowChinese24x24(89,0,"关",WHITE,BLACK);LCD_ShowChinese24x24(137,0,"闭",WHITE,BLACK);break;
			}
		}
		
		if(buf_Atmosphere_brightness != Atmosphere_brightness)//亮度变化刷新
		{
			buf_Atmosphere_brightness = Atmosphere_brightness;
			
			LCD_Fill(77,28,113,52,BLACK);
			if(buf_Atmosphere_brightness<10)
			{
				LCD_Num2412(89,28,buf_Atmosphere_brightness,WHITE,BLACK);
			}
			else if(buf_Atmosphere_brightness<100)
			{
				LCD_Num2412(83,28,buf_Atmosphere_brightness/10,WHITE,BLACK);
				LCD_Num2412(95,28,buf_Atmosphere_brightness%10,WHITE,BLACK);
			}
			else
			{
				LCD_Num2412(77,28,1,WHITE,BLACK);
				LCD_Num2412(89,28,0,WHITE,BLACK);
				LCD_Num2412(101,28,0,WHITE,BLACK);
			}
		}
		
		if(buf_Atmosphere_color != Atmosphere_color)//主题色变化刷新
		{
			buf_Atmosphere_color = Atmosphere_color;
			
			switch(buf_Atmosphere_color)
			{
				case 1:LCD_ShowChinese24x24(101,57,"红",WHITE,BLACK);LCD_ShowChinese24x24(125,57,"色",WHITE,BLACK);break;
				case 2:LCD_ShowChinese24x24(101,57,"紫",WHITE,BLACK);LCD_ShowChinese24x24(125,57,"色",WHITE,BLACK);break;
				case 3:LCD_ShowChinese24x24(101,57,"蓝",WHITE,BLACK);LCD_ShowChinese24x24(125,57,"色",WHITE,BLACK);break;
				case 4:LCD_ShowChinese24x24(101,57,"青",WHITE,BLACK);LCD_ShowChinese24x24(125,57,"色",WHITE,BLACK);break;
				case 5:LCD_ShowChinese24x24(101,57,"绿",WHITE,BLACK);LCD_ShowChinese24x24(125,57,"色",WHITE,BLACK);break;
				case 6:LCD_ShowChinese24x24(101,57,"黄",WHITE,BLACK);LCD_ShowChinese24x24(125,57,"色",WHITE,BLACK);break;
			}	
		}
		
	}
}

void working()//控制函数
{
	/* ---- 演示/番茄钟运行时让出 LED 与状态行 ---- */
	if(!ExtDemo_IsRunning() && !ExtPomo_IsRunning())
	{
		/* ---- LED status lamp: 0 standby / 1 charging / 2 low battery ---- */
		if(ClockLink_HomeState == 5)                       LedFx_SetMode(LED_FX_MODE_CHARGING);
		else if(ClockLink_PetBat > 0 && ClockLink_PetBat <= 30) LedFx_SetMode(LED_FX_MODE_LOWBAT);
		else                                               LedFx_SetMode(LED_FX_MODE_STANDBY);

		/* ---- 宠物电量 / 在线状态同步到 5 寸屏 ---- */
		Screen_SetBattery(ClockLink_PetBat, (ClockLink_HomeState == 5) ? 1 : 0);
		Screen_SetPetOnline(ClockLink_PetOnline);
	}

	time++;
	if(mod != 1)//非配网阶段
	{
		
		if(mod == 4)
		{
			wether_time++;
			
			if(wether_time == 3000)
			{
				mod = 3;
				wether_time = 0;
			}
		}
		//以下为正常工作时间隔一段时间做的事情
		if(time%5 == 0) //每100毫秒从rtc校准一下时间
		{
			if(calendar.w_year != 2015)//排除首次未校准时间
				RTC_Get();//从RTC获取时间
			else
				mcu_get_system_time();//从服务器校准系统时间
		}
		
		if(time%50 == 0)//每秒获取下环境温湿度 并上传服务器
		{
			sht30_data_process();//获取当前温湿度
			Screen_SetEnv(SHT30_temperature, SHT30_humidity);//同步到5寸串口屏
			
			if(time%250 == 0)//5秒上传一次温湿度
			{
				mcu_dp_value_update(DPID_WENDU,SHT30_temperature);//上传温度信息至涂鸦
				mcu_dp_value_update(DPID_SHIDU,SHT30_humidity);//上传湿度信息至涂鸦	
			}
		}
		
		if(time%250 == 0)//每5秒检测一下网络状态
		{
			mcu_get_wifi_connect_status();//获取涂鸦模组联网状态	
			
			if(Ty_mod != 4)//如果未连着网就进入配网模式
			{
				mod = 1;//进入配网模式
				time=0;
			}
		}
		
		if(time%180000 == 0)//每小时校准一下时间
		{
			mcu_get_system_time();//从服务器校准系统时间
		}
		
		if((time%30000 == 0) || ((time%4500 == 0) && (Ty_conditionNum == 0)))//每十分钟获取一次天气状况 或没有获取到天气时  一分半获取一次
		{
			request_weather_serve();//主动获取天气
		}

	}
	else//配网模式
	{
		if(time%5 == 0)//每一百毫秒获取一次网络状态
			mcu_get_wifi_connect_status();//获取涂鸦模组联网状态		

		if((time%1500 == 0) && (Ty_mod != 4))//30秒没连上网络复位涂鸦模组
			mcu_reset_wifi();//复位涂鸦模组
		
		if(Ty_mod == 4)
		{
			mod = 3;//进入正常工作模式
			time=0;
		}
	}
}
u8 num_set(u8 num)//将1-9-0顺序改为0-1-9
{
	switch(num)
	{
		case 0:return 8;
		case 1:return 9;
		case 2:return 0;
		case 3:return 1;
		case 4:return 2;
		case 5:return 3;
		case 6:return 4;
		case 7:return 5;
		case 8:return 6;
		case 9:return 7;
		default : return 0;
	}
}

void HGG_Scan(u8 hgg_hour,u8 hgg_min,u8 hgg_sec)//辉光管显示时间
{
	u32 dat_H,dat_L;
	u16 guan1,guan2,guan3,guan4,guan5,guan6;
	
	guan1=1<<num_set(hgg_hour/10);
	guan2=1<<num_set(hgg_hour%10);
	guan3=1<<num_set(hgg_min/10);
	guan4=1<<num_set(hgg_min%10);
	guan5=1<<num_set(hgg_sec/10);
	guan6=1<<num_set(hgg_sec%10);
	
	dat_L=guan3;
	dat_L<<=10;
	dat_L|=guan2;
	dat_L<<=10;
	dat_L|=guan1;
	dat_L<<=4;
	
	dat_H=guan6;
	dat_H<<=10;
	dat_H|=guan5;
	dat_H<<=10;
	dat_H|=guan4;
	dat_H<<=2;
	dat_H|=guan3>>8;
	
	HV57708_SendData(dat_H,dat_L);
	HV57708_OutputData();
}

void HGG_Display()//辉光管刷新
{
	static u8 buf_hgg_hour,buf_hgg_min,buf_hgg_sec;
	static u8 bj_Protection=0;
	
	u8 Protection_num;
	
	
	if(bj_Protection==0)//正常非保护状态
	{
		if((buf_hgg_hour!=calendar.hour || buf_hgg_min!=calendar.min || buf_hgg_sec!=calendar.sec))
		{
			buf_hgg_hour=calendar.hour;
			buf_hgg_min=calendar.min;
			buf_hgg_sec=calendar.sec;
			
			if(bj_display==1)
				Protection_count++;
			if(Protection_count >= Protection_interval_sec)
			{
				Protection_count=0;
				bj_Protection=1;
			}
			
			if(duan_num==1)
			{
				if(temporary_time || (buf_hgg_hour*60+buf_hgg_min>=duan1_on_hour*60+duan1_on_min && buf_hgg_hour*60+buf_hgg_min<=duan1_off_hour*60+duan1_off_min))
				{
					if(temporary_time)
						temporary_time--;
					Colon_on;
					bj_display=1;
					HGG_Scan(buf_hgg_hour,buf_hgg_min,buf_hgg_sec);
				}
				else if(bj_display!=2)
					bj_display=0;
			}
			else if(duan_num==2)
			{
				if(temporary_time || (buf_hgg_hour*60+buf_hgg_min>=duan1_on_hour*60+duan1_on_min && buf_hgg_hour*60+buf_hgg_min<=duan1_off_hour*60+duan1_off_min))
				{
					if(temporary_time)
						temporary_time--;
					Colon_on;
					bj_display=1;
					HGG_Scan(buf_hgg_hour,buf_hgg_min,buf_hgg_sec);
				}
				else if(temporary_time || (buf_hgg_hour*60+buf_hgg_min>=duan2_on_hour*60+duan2_on_min && buf_hgg_hour*60+buf_hgg_min<=duan2_off_hour*60+duan2_off_min))
				{
					if(temporary_time)
						temporary_time--;
					Colon_on;
					bj_display=1;
					HGG_Scan(buf_hgg_hour,buf_hgg_min,buf_hgg_sec);
				}
				else if(bj_display!=2)
					bj_display=0;
			}
			else if(duan_num==3)
			{
				if(temporary_time || (buf_hgg_hour*60+buf_hgg_min>=duan1_on_hour*60+duan1_on_min && buf_hgg_hour*60+buf_hgg_min<=duan1_off_hour*60+duan1_off_min))
				{
					if(temporary_time)
						temporary_time--;
					Colon_on;
					bj_display=1;
					HGG_Scan(buf_hgg_hour,buf_hgg_min,buf_hgg_sec);
				}
				else if(temporary_time || (buf_hgg_hour*60+buf_hgg_min>=duan2_on_hour*60+duan2_on_min && buf_hgg_hour*60+buf_hgg_min<=duan2_off_hour*60+duan2_off_min))
				{
					if(temporary_time)
						temporary_time--;
					Colon_on;
					bj_display=1;
					HGG_Scan(buf_hgg_hour,buf_hgg_min,buf_hgg_sec);
				}
				else if(temporary_time || (buf_hgg_hour*60+buf_hgg_min>=duan3_on_hour*60+duan3_on_min && buf_hgg_hour*60+buf_hgg_min<=duan3_off_hour*60+duan3_off_min))
				{
					if(temporary_time)
						temporary_time--;
					Colon_on;
					bj_display=1;
					HGG_Scan(buf_hgg_hour,buf_hgg_min,buf_hgg_sec);
				}
				else if(bj_display!=2)
					bj_display=0;
			}
		}
		if(bj_display == 0)
		{
			bj_Protection=0;
			bj_display=2;
			Colon_off;
			HV57708_SendData(0,0);
			HV57708_OutputData();
			REG_LED_ALL_BLACK();
		}
		
	}
	else 
	{
		Protection_num=(bj_Protection%10)*10+(bj_Protection%10);
		HGG_Scan(Protection_num,Protection_num,Protection_num);
		bj_Protection++;	
		if(bj_Protection==Protection_duration_num)
			bj_Protection=0;
	}
}
void color_choice(u8 color,u8 brightness,u8 *r,u8 *g,u8 *b)//根据亮度配置相应颜色RGB值
{
	switch(color)
	{
		case 1:*r=brightness;*g=0;*b=0;break;
		case 2:*r=brightness;*g=0;*b=brightness;break;
		case 3:*r=0;*g=0;*b=brightness;break;
		case 4:*r=0;*g=brightness;*b=brightness;break;
		case 5:*r=0;*g=brightness;*b=0;break;
		case 6:*r=brightness;*g=brightness;*b=0;break;
	}
}
void Atmosphere_Scan()//氛围灯刷新
{
	static u8 Atmosphere_time=0;
	static u8 Atmosphere_time_trend=0;
	static u8 Cycle_color=7;
	static u8 buf_Atmosphere_mode=255;
	static u8 buf_Atmosphere_brightness=255;
	static u8 buf_Atmosphere_color=255;
	u8 r,g,b;
	
	if(bj_display==1 && LedFx_IsActive()==0)
	{
		switch(Atmosphere_mode)
		{
			case 1://单色常亮
				if(buf_Atmosphere_mode!=Atmosphere_mode || buf_Atmosphere_brightness!=Atmosphere_brightness || buf_Atmosphere_color!=Atmosphere_color)
				{
					buf_Atmosphere_mode=Atmosphere_mode;
					buf_Atmosphere_brightness=Atmosphere_brightness;
					buf_Atmosphere_color=Atmosphere_color;
					
					color_choice(buf_Atmosphere_color,buf_Atmosphere_brightness,&r,&g,&b);
					REG_LED_Write_cooking(0,r,g,b);
					REG_LED_Write_cooking(1,r,g,b);
					REG_LED_Write_cooking(2,r,g,b);
					REG_LED_Write_cooking(3,r,g,b);
					REG_LED_Write_cooking(4,r,g,b);
					REG_LED_Write_cooking(5,r,g,b);
					
					REG_LED_Refresh();
				}
				break;
			case 2://多色常亮
				if(buf_Atmosphere_mode!=Atmosphere_mode || buf_Atmosphere_brightness!=Atmosphere_brightness || buf_Atmosphere_color!=Atmosphere_color)
				{
					buf_Atmosphere_mode=Atmosphere_mode;
					buf_Atmosphere_brightness=Atmosphere_brightness;
					buf_Atmosphere_color=Atmosphere_color;
					
					color_choice(1,Atmosphere_brightness,&r,&g,&b);
					REG_LED_Write_cooking(0,r,g,b);
					color_choice(2,Atmosphere_brightness,&r,&g,&b);
					REG_LED_Write_cooking(1,r,g,b);
					color_choice(3,Atmosphere_brightness,&r,&g,&b);
					REG_LED_Write_cooking(2,r,g,b);
					color_choice(4,Atmosphere_brightness,&r,&g,&b);
					REG_LED_Write_cooking(3,r,g,b);
					color_choice(5,Atmosphere_brightness,&r,&g,&b);
					REG_LED_Write_cooking(4,r,g,b);
					color_choice(6,Atmosphere_brightness,&r,&g,&b);
					REG_LED_Write_cooking(5,r,g,b);
					
					REG_LED_Refresh();
				}
				break;
			case 3://单色呼吸
				if(Atmosphere_time_trend)
				{
					Atmosphere_time--;
					if(Atmosphere_time==0)
						Atmosphere_time_trend=0;
				}
				else 
				{
					Atmosphere_time++;
					if(Atmosphere_time==100)
						Atmosphere_time_trend=1;
				}
				
				color_choice(Atmosphere_color,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
				REG_LED_Write_cooking(0,r,g,b);
				REG_LED_Write_cooking(1,r,g,b);
				REG_LED_Write_cooking(2,r,g,b);
				REG_LED_Write_cooking(3,r,g,b);
				REG_LED_Write_cooking(4,r,g,b);
				REG_LED_Write_cooking(5,r,g,b);
				
				REG_LED_Refresh();
				break;
			case 4://六色呼吸
			if(Atmosphere_time_trend)
			{
				Atmosphere_time--;
				if(Atmosphere_time==0)
					Atmosphere_time_trend=0;
			}
			else 
			{
				Atmosphere_time++;
				if(Atmosphere_time==100)
					Atmosphere_time_trend=1;
			}
			
			color_choice(1,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(0,r,g,b);
			color_choice(2,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(1,r,g,b);
			color_choice(3,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(2,r,g,b);
			color_choice(4,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(3,r,g,b);
			color_choice(5,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(4,r,g,b);
			color_choice(6,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(5,r,g,b);
			
			REG_LED_Refresh();
			break;
			
			case 5://六色流水
			if(Atmosphere_time_trend)
			{
				Atmosphere_time--;
				if(Atmosphere_time==0)
				{
					Atmosphere_time_trend=0;
					Cycle_color++;
					if(Cycle_color==13)
						Cycle_color=7;
				}
			}
			else 
			{
				Atmosphere_time++;
				if(Atmosphere_time==100)
					Atmosphere_time_trend=1;
			}
			
			color_choice((Cycle_color-1)%6+1,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(0,r,g,b);
			color_choice((Cycle_color-2)%6+1,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(1,r,g,b);
			color_choice((Cycle_color-3)%6+1,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(2,r,g,b);
			color_choice((Cycle_color-4)%6+1,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(3,r,g,b);
			color_choice((Cycle_color-5)%6+1,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(4,r,g,b);
			color_choice((Cycle_color-6)%6+1,Atmosphere_time*Atmosphere_brightness/100,&r,&g,&b);
			REG_LED_Write_cooking(5,r,g,b);
			
			REG_LED_Refresh();
			break;
			
			case 6://关闭
			if(buf_Atmosphere_mode!=Atmosphere_mode || buf_Atmosphere_brightness!=Atmosphere_brightness || buf_Atmosphere_color!=Atmosphere_color)
				{
					buf_Atmosphere_mode=Atmosphere_mode;
					buf_Atmosphere_brightness=Atmosphere_brightness;
					buf_Atmosphere_color=Atmosphere_color;
					
					REG_LED_ALL_BLACK();
				}
			break;
		}
	}
	else
	{
		buf_Atmosphere_mode=255;
		buf_Atmosphere_brightness=255;
		buf_Atmosphere_color=255;
	}
}
