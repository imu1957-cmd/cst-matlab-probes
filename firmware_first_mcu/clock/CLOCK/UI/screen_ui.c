#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>
#include "screen_ui.h"
#include "rtc.h"
#include "clock_link.h"
#include "ext_demo.h"
#include "screen_cfg.h"
#include "ext_pomo.h"
#include "ext_diag.h"

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

/* ---- 屏幕按钮（可选）：TJC 触摸事件回的是控件索引。--------- */
/* 在 USART HMI 里加好按钮后，把索引填进来；-1 = 关闭该功能。 */
/* ---- 按钮控件索引（按磁盘 .HMI 里的实际创建顺序，2026-10-25 核对）----
 * page0: 0 t0  1 t1  2 t2  3 t3  4 n0  5 n1  6 t4  7 t5  8 j0  9 t6
 *        10 btn_home  11 btn_memo  12 btn_set  13 btn_demo
 * page1: 0 u0  1 bt4  2 bt1  3 bt2  4 bt3  5 bt0  6 bt5  7 bt6  8 bt7
 *        9 btn_back  10 btn_clear   <-- btN 的索引是乱的，用 MEMO_BTN_MAP 映射
 * page2: 0 n2  1 n3  2 n4  3 s0  4 u1  5 u2  6 u3  7 u4
 *        8 btn_n2m  9 btn_n2p  10 btn_n3m  11 btn_n3p
 *        12 btn_n4m 13 btn_n4p  14 btn_back
 */
#define CTRL_BTN_HOME   10     /* 主页「叫宠物回充」 */
#define CTRL_BTN_MEMO   11     /* 主页「备忘录」 */
#define CTRL_BTN_SET    12     /* 主页「设置」 */
#define CTRL_BTN_DEMO   13     /* 主页「演示模式」 */

#define CTRL_MEMO_BACK   9     /* page1 返回 */
#define CTRL_MEMO_CLEAR  10    /* page1 清空 */

/* page3 日志页：0 t7 滚动文本  1 btn_back  2 u5 标题  3 btn_logclr */
#define CTRL_LOG_BACK   0
#define CTRL_LOG_CLEAR  2
/* page2 已删掉 s0 曲线（2026-10-25），后续控件索引整体 -1 */
#define CTRL_SET_N2M     7     /* page2 低电阈值 - */
#define CTRL_SET_N2P     8
#define CTRL_SET_N3M     9     /* 亮度 - */
#define CTRL_SET_N3P    10
#define CTRL_SET_N4M    11     /* 温度偏移 - */
#define CTRL_SET_N4P    12
#define CTRL_SET_BACK   13

/* page1 上 bt0..bt7 的控件索引（不是 1..8，实测乱序） */
static const uint8_t k_memo_ctrl[8] = { 5, 2, 3, 4, 1, 6, 7, 8 };

#define PG_MAIN   0
#define PG_MEMO   1
#define PG_SET    2

/* page4 番茄钟页控件索引（按屏工程实际顺序） */
#define PO_TITLE    0
#define PO_TIMER    1
#define PO_STATE    2
#define PO_START    3
#define PO_RESET    4
#define PO_BACK     5
#define PO_CURVE    6
#define PG_LOG    3

/* 备忘录缓存：8 条 × 40 字节 */
char Screen_Memo[8][40];

static uint8_t s_memo_cnt  = 0;   /* used slots (max 8) */
static uint8_t s_memo_next = 0;   /* next write index */
static uint8_t s_page_hold = 0;
static uint8_t s_cur_page = 0;   /* 当前页号 */   /* ticks before auto return to main */

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
	s_cur_page = page;
	sprintf(cmd, "page %d", (int)page);
	SndCmd(cmd);
}

void Screen_SetBrightness(uint8_t pct)
{
	char cmd[24];
	if(pct < 10) pct = 10;
	if(pct > 100) pct = 100;
	sprintf(cmd, "dim=%d", (int)pct);
	SndCmd(cmd);
	if(pct != g_cfg.backlight)
	{
		g_cfg.backlight = pct;
		Cfg_Save();
	}
}

/* ---------------- 初始化 ---------------- */
void Screen_Init(void)
{
	SndCmd("bkcmd=0");          /* 关掉调试回包，省带宽 */
	Cfg_Load();
	Screen_ClearMemo();
	Screen_ShowPage(PG_MAIN);
	s_inited = 1;
	Screen_SetBrightness(g_cfg.backlight);
	Screen_SetSettings();
	Screen_SetStatus("BOOT");
}

/* ---------------- 触摸 ---------------- */
void Screen_RxByte(uint8_t b)
{
	Diag_RxByte(b);   /* 调试命令行旁路 */
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


/* 临时提示：借主页状态行 t5 显示，3 秒后由 Tick 恢复真实状态 */
static uint8_t s_toast_hold = 0;
static void Screen_Toast(const char *msg)
{
	SetTxt("t5", msg);
	s_toast_hold = 30;      /* 30 x 100ms = 3s */
	s_status[0] = 0;        /* 让 Tick 重新推送真实状态 */
}

/* ---------------- 日志页（page3） ---------------- */
#define LOG_LINES   6
#define LOG_COLS    58
static char s_log[LOG_LINES][LOG_COLS];
static uint8_t s_log_n = 0;

/* 清空日志并刷新屏 */
void Screen_ClearLog(void)
{
	uint8_t i;
	for(i = 0; i < LOG_LINES; i++) s_log[i][0] = 0;
	s_log_n = 0;
	SetTxt("t7", "");
}

/* 追加一行日志（自动上滚），并刷新 t7 */
void Screen_Log(const char *line)
{
	uint8_t i;
	char buf[LOG_LINES * (LOG_COLS + 2) + 1];
	uint16_t n = 0;

	if(!line) return;
	if(s_log_n < LOG_LINES) { s_log_n++; }
	else
	{
		for(i = 1; i < LOG_LINES; i++)
		{
			strncpy(s_log[i - 1], s_log[i], LOG_COLS - 1);
			s_log[i - 1][LOG_COLS - 1] = 0;
		}
	}
	strncpy(s_log[s_log_n - 1], line, LOG_COLS - 1);
	s_log[s_log_n - 1][LOG_COLS - 1] = 0;

	/* 拼成多行文本：行间用 \r 换行（TJC 文本控件支持 0x0D） */
	for(i = 0; i < s_log_n; i++)
	{
		strncpy(&buf[n], s_log[i], LOG_COLS - 1);
		n += (uint16_t)strlen(&buf[n]);
		buf[n++] = '\r';
		buf[n] = 0;
	}
	SetTxt("t7", buf);
}
void Screen_OnTouch(uint8_t page, uint8_t ctrl, uint8_t event)
{
	if(event != 1) return;              /* 只认按下 */

	if(page == PG_MEMO)
	{
		uint8_t i;
		if(ctrl == CTRL_MEMO_BACK)  { Screen_ShowPage(PG_MAIN); return; }
		if(ctrl == CTRL_MEMO_CLEAR) { Screen_ClearMemo(); return; }
		/* bt0..bt7：索引乱序，用映射表查；点一下就把这条发给宠物念 */
		for(i = 0; i < 8; i++)
		{
			if(ctrl == k_memo_ctrl[i])
			{
				const char *txt = Screen_Memo[i];
				if(txt[0]) ClockLink_SendMemo(txt);
				return;
			}
		}
	}
	else if(page == PG_MAIN)
	{
		switch(ctrl)
		{
		case CTRL_BTN_HOME:
			ClockLink_StartHome();
			Screen_Toast("已通知宠物回充");
			return;
		case CTRL_BTN_MEMO:
			Screen_ShowPage(PG_MEMO);
			return;
		case CTRL_BTN_SET:
			Screen_SetSettings();
			Screen_ShowPage(PG_SET);
			return;
		case CTRL_BTN_DEMO:
			ExtDemo_Start();
			Screen_Toast("演示模式启动");
			return;
		default:
			return;     /* 主页其它区域不响应 */
		}
	}
	else if(page == PG_LOG)
	{
		if(ctrl == CTRL_LOG_BACK)  { Screen_ShowPage(PG_MAIN); return; }
		if(ctrl == CTRL_LOG_CLEAR) { Screen_ClearLog(); return; }
	}
	else if(page == PG_POMO)
	{
		if(ctrl == PO_START)
		{
			if(ExtPomo_IsRunning()) ExtPomo_Stop();
			else                    ExtPomo_Start(25, 5);
			return;
		}
		if(ctrl == PO_RESET)
		{
			ExtPomo_Stop();
			SetTxt("n0", "0:00");
			SetTxt("t1", "已停止");
			return;
		}
		if(ctrl == PO_BACK)
		{
			Screen_ShowPage(PG_MAIN);
			return;
		}
	}	else if(page == PG_SET)
	{
		switch(ctrl)
		{
		case CTRL_SET_N2P: if(g_cfg.low_bat + 5 <= 60) g_cfg.low_bat += 5; break;
		case CTRL_SET_N2M: if(g_cfg.low_bat > 30)      g_cfg.low_bat -= 5; break;
		case CTRL_SET_N3P: Screen_SetBrightness((uint8_t)(g_cfg.backlight + 10)); return;
		case CTRL_SET_N3M: Screen_SetBrightness((uint8_t)(g_cfg.backlight > 20 ? g_cfg.backlight - 10 : 10)); return;
		case CTRL_SET_N4P: if(g_cfg.temp_ofs + 1 <= 20)  g_cfg.temp_ofs += 1; break;
		case CTRL_SET_N4M: if(g_cfg.temp_ofs - 1 >= -20) g_cfg.temp_ofs -= 1; break;
		case CTRL_SET_BACK: Screen_ShowPage(PG_MAIN); return;
		default: return;
		}
		Cfg_Save();
		Screen_SetSettings();
		s_temp_x10 = 0x7FFF;            /* force redraw with new offset */
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
		temp_x10 += (int16_t)g_cfg.temp_ofs * 10;
		SetVal("n0", (int32_t)(temp_x10 / 10));      /* 屏上只显整数度 */
	}
	if(hum != s_hum)
	{
		s_hum = hum;
		SetVal("n1", (int32_t)(hum / 10));      /* 入参是 0.1%，屏上显整数 % */
	}
}

void Screen_SetWeather(int16_t temp, const char *cond)
{
	char buf[28];
	if(cond == 0 || cond[0] == 0) return;
	if(temp < -40 || temp > 80) snprintf(buf, sizeof(buf), "%s", cond);
	else                        snprintf(buf, sizeof(buf), "%s %d\xA1\xE6", cond, (int)temp);
	if(strcmp(buf, s_weather) == 0) return;
	strncpy(s_weather, buf, sizeof(s_weather) - 1);
	s_weather[sizeof(s_weather) - 1] = 0;
	SetTxt("t3", s_weather);
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


/* ---------------- page4：番茄钟 ---------------- */
uint8_t Screen_GetPage(void)
{
	return s_cur_page;
}

/* 显示倒计时：sec=剩余秒，resting=1 休息阶段 */
void Screen_PomoShow(uint16_t sec, uint8_t resting)
{
	static uint16_t last_sec = 0xFFFF;
	static uint8_t  last_rest = 0xFF;
	char buf[32];

	if(sec > 5999) sec = 5999;
	if(sec != last_sec)
	{
		last_sec = sec;
		sprintf(buf, "%u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
		SetTxt("n0", buf);          /* n0 为数字/文本控件，直接写字符串 */
	}
	if(resting != last_rest)
	{
		last_rest = resting;
		SetTxt("t1", resting ? "休息中" : "计时中");
	}
}

/* 曲线追加一个点：temp_x10=温度×10，hum=湿度% */
void Screen_CurveAdd(int16_t temp_x10, uint8_t hum)
{
	char cmd[40];
	sprintf(cmd, "add 0,%d", (int)(temp_x10 / 10));
	SndCmd(cmd);
	sprintf(cmd, "add 1,%d", (int)hum);
	SndCmd(cmd);
}
/* ---------------- 200ms 刷新主页 ---------------- */
void Screen_Tick100ms(void)
{
	static uint8_t div = 0;
	char buf[24];
	int32_t d;

	if(!s_inited) return;

	if(s_page_hold)
	{
		if(--s_page_hold == 0) Screen_ShowPage(PG_MAIN);
		if(s_toast_hold && --s_toast_hold == 0) s_status[0] = 0;   /* toast 结束 */
	}

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
	s_memo_cnt  = 0;
	s_memo_next = 0;
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

/* ---------------- memo ring ---------------- */
void Screen_RefreshMemo(void)
{
	uint8_t i;
	char obj[8];
	for(i = 0; i < 8; i++)
	{
		sprintf(obj, "bt%d", (int)i);
		SetTxt(obj, Screen_Memo[i]);
	}
}

void Screen_PushMemo(const char *text)
{
	if(!text || !text[0]) return;

	if(s_memo_cnt < 8) s_memo_cnt++;

	strncpy(Screen_Memo[s_memo_next], text, 39);
	Screen_Memo[s_memo_next][39] = 0;
	s_memo_next = (uint8_t)((s_memo_next + 1) & 7);

	Screen_RefreshMemo();
	Screen_ShowPage(PG_MEMO);
	s_page_hold = 15;               /* 15 x 200ms = 3s then back to main */
}

/* ---------------- settings page ---------------- */
void Screen_SetSettings(void)
{
	SetVal("n8", (int32_t)g_cfg.low_bat);
	SetVal("n9", (int32_t)g_cfg.backlight);
	SetVal("n10", (int32_t)g_cfg.temp_ofs);
}
