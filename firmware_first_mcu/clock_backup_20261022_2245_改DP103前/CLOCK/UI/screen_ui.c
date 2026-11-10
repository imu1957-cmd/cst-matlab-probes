#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>
#include "screen_ui.h"
#include "rtc.h"
#include "clock_link.h"

/* ======================================================================
 *  5 寸串口屏 UI（陶晶驰 TJC8048X550_011）
 *
 *  - 所有指令：字符串 + 0xFF 0xFF 0xFF
 *  - 触摸事件：0x65 page ctrl event 0xFF 0xFF 0xFF
 *  - 只发 ASCII：中文标签写在 TJC 工程里（固件发 UTF-8 会乱码）
 *
 *  页面与对象名（要和 TJC 工程一致）：
 *    page0 主页   t0 时间 t1 日期 t2 星期 t3 天气 n0 温度 n1 湿度
 *                 t4 电量文本 j0 电量条 t5 状态
 *    page1 备忘录 bt0..bt7 t6 正文
 *    page2 设置   n2 阈值 n3 亮度 n4 温度偏移
 *    page3 日志   t7
 * ====================================================================== */

#define PG_MAIN   0
#define PG_MEMO   1
#define PG_SET    2
#define PG_LOG    3

/* 备忘录缓存：8 条 × 40 字节 */
char Screen_Memo[8][40];

static uint8_t s_inited = 0;
static uint8_t s_hh = 0xFF, s_mm = 0xFF, s_t2 = 0xFF;
static int32_t s_date = -1;
static int16_t s_temp_x10 = 0x7FFF;
static uint16_t s_hum = 0xFFFF;
static uint8_t s_bat = 0xFF, s_chg = 0xFF;
static char s_status[24] = {0};
static char s_weather[20] = {0};

static volatile uint8_t s_touch_ready = 0;
static uint8_t s_t_page, s_t_ctrl, s_t_event;

/* ---------------- 底层 ---------------- */
static void SndCmd(const char *cmd)
{
	printf("%s", cmd);
	USART_SendData(USART1, 0xFF);
	while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {}
	USART_SendData(USART1, 0xFF);
	while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {}
	USART_SendData(USART1, 0xFF);
	while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {}
}

static void SetTxt(const char *obj, const char *val)
{
	char cmd[96];
	sprintf(cmd, "%s.txt=\"%s\"", obj, val);
	SndCmd(cmd);
}

static void SetVal(const char *obj, int32_t v)
{
	char cmd[48];
	sprintf(cmd, "%s.val=%ld", obj, (long)v);
	SndCmd(cmd);
}

void Screen_ShowPage(uint8_t page)
{
	char cmd[24];
	sprintf(cmd, "page %d", (int)page);
	SndCmd(cmd);
}

/* ---------------- 初始化 ---------------- */
void Screen_Init(void)
{
	SndCmd("bkcmd=0");          /* 关掉调试回包，省带宽 */
	Screen_ClearMemo();
	Screen_ShowPage(PG_MAIN);
	s_inited = 1;
	Screen_SetStatus("BOOT");
}

/* ---------------- 触摸 ---------------- */
void Screen_RxByte(uint8_t b)
{
	static uint8_t st = 0;
	static uint8_t pg, ct, ev;

	switch(st)
	{
	case 0: if(b == 0x65) st = 1; break;
	case 1: pg = b; st = 2; break;
	case 2: ct = b; st = 3; break;
	case 3: ev = b; st = 4; break;
	case 4: st = (b == 0xFF) ? 5 : 0; break;
	case 5: st = (b == 0xFF) ? 6 : 0; break;
	case 6:
		if(b == 0xFF)
		{
			s_t_page = pg; s_t_ctrl = ct; s_t_event = ev;
			s_touch_ready = 1;
		}
		st = 0;
		break;
	default: st = 0; break;
	}
}

void Screen_OnTouch(uint8_t page, uint8_t ctrl, uint8_t event)
{
	if(event != 1) return;              /* 只认按下 */

	if(page == PG_MEMO)
	{
		/* bt0..bt7 控件号约定 1..8：点一下就把这条发给宠物念 */
		if(ctrl >= 1 && ctrl <= 8)
		{
			const char *txt = Screen_Memo[ctrl - 1];
			if(txt[0]) ClockLink_SendMemo(txt);
		}
	}
	else if(page == PG_MAIN)
	{
		Screen_ShowPage(PG_MEMO);       /* 主页点一下进备忘录 */
	}
}

void Screen_Poll(void)
{
	if(!s_touch_ready) return;
	s_touch_ready = 0;
	Screen_OnTouch(s_t_page, s_t_ctrl, s_t_event);
}

/* ---------------- 数据更新 ---------------- */
void Screen_SetEnv(int16_t temp_x10, uint16_t hum)
{
	if(temp_x10 != s_temp_x10)
	{
		s_temp_x10 = temp_x10;
		SetVal("n0", (int32_t)(temp_x10 / 10));      /* 屏上只显整数度 */
	}
	if(hum != s_hum)
	{
		s_hum = hum;
		SetVal("n1", (int32_t)hum);
	}
}

void Screen_SetWeather(int16_t temp, const char *cond)
{
	if(cond && cond[0] && strcmp(cond, s_weather) != 0)
	{
		strncpy(s_weather, cond, sizeof(s_weather) - 1);
		s_weather[sizeof(s_weather) - 1] = 0;
		SetTxt("t3", s_weather);
	}
	(void)temp;
}

void Screen_SetBattery(uint8_t pct, uint8_t charging)
{
	char buf[8];
	if(pct == s_bat && charging == s_chg) return;
	s_bat = pct; s_chg = charging;

	sprintf(buf, "%d%%", (int)pct);
	SetTxt("t4", buf);
	SetVal("j0", (int32_t)pct);
	SetTxt("t5", charging ? "CHG" : (ClockLink_PetOnline ? "ON" : "OFF"));
}

void Screen_SetStatus(const char *text)
{
	if(!text || strcmp(text, s_status) == 0) return;
	strncpy(s_status, text, sizeof(s_status) - 1);
	s_status[sizeof(s_status) - 1] = 0;
	SetTxt("t5", s_status);
}

void Screen_SetPetOnline(uint8_t online)
{
	if(!s_chg) SetTxt("t5", online ? "ON" : "OFF");
}

/* ---------------- 200ms 刷新主页 ---------------- */
void Screen_Tick100ms(void)
{
	static uint8_t div = 0;
	char buf[24];
	int32_t d;

	if(!s_inited) return;
	if(++div < 2) return;
	div = 0;

	if(calendar.hour != s_hh || calendar.min != s_mm)
	{
		s_hh = calendar.hour; s_mm = calendar.min;
		sprintf(buf, "%02d:%02d", (int)s_hh, (int)s_mm);
		SetTxt("t0", buf);
	}

	d = (int32_t)calendar.w_year * 10000 + (int32_t)calendar.w_month * 100 + calendar.w_date;
	if(d != s_date)
	{
		s_date = d;
		sprintf(buf, "%04d-%02d-%02d", (int)calendar.w_year, (int)calendar.w_month, (int)calendar.w_date);
		SetTxt("t1", buf);
	}

	if(calendar.week != s_t2)
	{
		static const char *wkn[7] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
		uint8_t w = (uint8_t)(calendar.week % 7);
		s_t2 = calendar.week;
		SetTxt("t2", (char *)wkn[w]);      /* ASCII 星期，中文标签交给屏的静态控件 */
	}
}

/* ---------------- 备忘录 ---------------- */
void Screen_ClearMemo(void)
{
	uint8_t i;
	char obj[8];
	for(i = 0; i < 8; i++)
	{
		Screen_Memo[i][0] = 0;
		sprintf(obj, "bt%d", (int)i);
		SetTxt(obj, "");
	}
}

void Screen_AddMemo(uint8_t idx, const char *text)
{
	char obj[8];
	if(idx > 7 || !text) return;
	strncpy(Screen_Memo[idx], text, 39);
	Screen_Memo[idx][39] = 0;
	sprintf(obj, "bt%d", (int)idx);
	SetTxt(obj, Screen_Memo[idx]);
}
