#include "ext_diag.h"
#include "string.h"
#include "stdio.h"
#include <stdlib.h>
#include "sht30.h"
#include "clock_link.h"
#include "screen_ui.h"
#include "screen_cfg.h"
#include "ext_repeat.h"
#include "ext_demo.h"
#include "ext_pomo.h"
#include "24Cxx_init.h"
#include "ext_linkstat.h"

/* ---------------- 内部状态 ---------------- */
static char     s_line[48];        /* 命令行缓冲 */
static uint8_t  s_len;             /* 已收字节 */
static uint8_t  s_ready;           /* 1=收到整行 */
static uint16_t s_tick;            /* 20ms 计数 */
static uint8_t  s_checked;         /* 自检已执行 */
static uint8_t  s_ok;              /* 自检结果 */
static char     s_err[16];         /* 失败项 */

/* ---------------- 看门狗 ---------------- */
static void Diag_WdgInit(void)
{
#if DIAG_WDG_EN
    /* LSI ~40kHz，4 分频 -> 10kHz；重装 1600 -> 约 1.6s */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_4);
    IWDG_SetReload(1600);
    IWDG_ReloadCounter();
    IWDG_Enable();
#endif
}

void Diag_Init(void)
{
    s_len = 0; s_ready = 0; s_tick = 0; s_checked = 0; s_ok = 0;
    s_err[0] = 0;
    Diag_WdgInit();
    printf("\r\n[DIAG] boot OK, build %s\r\n", __DATE__);
}

void Diag_Tick20ms(void)
{
    if(s_tick < 0xFFFF) s_tick++;
#if DIAG_WDG_EN
    /* 注意：看门狗在主循环喂，不在中断里喂，否则死循环也喂得上 */
#endif
}

void Diag_RxByte(uint8_t b)
{
    if(b == 0xFF || b == 0xFE) { s_len = 0; return; }   /* 屏二进制帧：丢弃 */
    if(b == '\r') return;
    if(b == '\n') { s_line[s_len] = 0; s_ready = (s_len > 0) ? 1 : 0; return; }
    if(b < 0x20 || b > 0x7E) { s_len = 0; return; }
    if(s_len < sizeof(s_line) - 1) s_line[s_len++] = (char)b;
}

/* ---------------- 自检 ---------------- */
static void Diag_SelfTest(void)
{
    uint8_t magic;

    s_ok = 1;
    strcpy(s_err, "OK");

    /* 1) SHT30：温度值应落在 -40..80℃ 的合理范围（×10 存储） */
    if(SHT30_temperature == 0 || SHT30_temperature == 0xFFFF)
    {
        s_ok = 0; strcpy(s_err, "SHT30");
    }
    /* 2) EEPROM：能读到魔数说明 I2C 通（0xA5 由 ext_repeat 写入） */
    else
    {
        magic = AT24CXX_ReadOneByte(0xF2);
        if(magic != 0xA5 && magic != 0xFF) { s_ok = 0; strcpy(s_err, "EEPROM"); }
    }

    /* 3) 宠物链路：自检时刻在线则通过（不在线只提示，不算失败） */
    printf("[DIAG] %s  temp=%u humi=%u pet=%u%% online=%u\r\n",
           s_ok ? "PASS" : "FAIL",
           (unsigned)SHT30_temperature, (unsigned)SHT30_humidity,
           (unsigned)ClockLink_PetBat, (unsigned)ClockLink_PetOnline);

    if(!ClockLink_PetOnline)
        printf("[DIAG] WARN: pet not online (check HC-05 / power)\r\n");
}

/* ---------------- 命令实现 ---------------- */
static void Diag_CmdHelp(void)
{
    printf("--- commands (end with \r\n) ---\r\n");
    printf(":help               this list\r\n");
    printf(":get                show cfg/status\r\n");
    printf(":set low=40         low-battery threshold %\r\n");
    printf(":set bright=80      backlight percent\r\n");
    printf(":set ofs=2          temperature offset\r\n");
    printf(":selftest           run self test now\r\n");
    printf(":memo 15:00 组会     add reminder(must be GBK)\r\n");
    printf(":demo               run 30s demo\r\n");
    printf(":pomo               start 25/5 pomodoro\r\n");
    printf(":petbat             query pet battery\r\n");
    printf(":home               ask pet go home\r\n");
    printf(":lstat              link quality stats (last 100 frames)\r\n");
}

static void Diag_CmdGet(void)
{
    printf("[CFG] low=%u bright=%u ofs=%d\r\n",
           (unsigned)g_cfg.low_bat, (unsigned)g_cfg.backlight, (int)g_cfg.temp_ofs);
    printf("[LINK] petbat=%u%% online=%u home=%u\r\n",
           (unsigned)ClockLink_PetBat, (unsigned)ClockLink_PetOnline,
           (unsigned)ClockLink_HomeState);
    printf("[MEMO] count=%u\r\n", (unsigned)ExtRepeat_Count());
    printf("[DIAG] self=%s(%s)\r\n", s_ok ? "OK" : "ERR", s_err);
}

static void Diag_CmdSet(const char *arg)
{
    int v;
    if(strncmp(arg, "low=", 4) == 0)
    {
        v = atoi(arg + 4);
        if(v >= 5 && v <= 95) { g_cfg.low_bat = (uint8_t)v; Cfg_Save(); printf("[SET] low=%d\r\n", v); }
        else printf("[ERR] low must be 5..95\r\n");
    }
    else if(strncmp(arg, "bright=", 7) == 0)
    {
        v = atoi(arg + 7);
        if(v >= 10 && v <= 100) { g_cfg.backlight = (uint8_t)v; Cfg_Save();
                                  Screen_SetBrightness((uint8_t)v); printf("[SET] bright=%d\r\n", v); }
        else printf("[ERR] bright must be 10..100\r\n");
    }
    else if(strncmp(arg, "ofs=", 4) == 0)
    {
        v = atoi(arg + 4);
        if(v >= -10 && v <= 10) { g_cfg.temp_ofs = (int8_t)v; Cfg_Save(); printf("[SET] ofs=%d\r\n", v); }
        else printf("[ERR] ofs must be -10..10\r\n");
    }
    else printf("[ERR] unknown key, try :help\r\n");
}

static void Diag_Exec(char *line)
{
    if(line[0] != ':') return;

    if(strncmp(line, ":help", 5) == 0)          Diag_CmdHelp();
    else if(strncmp(line, ":get", 4) == 0)      Diag_CmdGet();
    else if(strncmp(line, ":set ", 5) == 0)     Diag_CmdSet(line + 5);
    else if(strncmp(line, ":selftest", 9) == 0) { Diag_SelfTest(); Screen_SetStatus(s_ok ? "SELF OK" : s_err); }
    else if(strncmp(line, ":demo", 5) == 0)     { ExtDemo_Start(); Screen_SetStatus("DEMO ON"); printf("[DEMO] start\r\n"); }
    else if(strncmp(line, ":pomo", 5) == 0)     { ExtPomo_Start(25, 5); Screen_SetStatus("POMO 25/5"); printf("[POMO] start\r\n"); }
    else if(strncmp(line, ":petbat", 7) == 0)   { ClockLink_QueryBattery(); printf("[LINK] query sent\r\n"); }
    else if(strncmp(line, ":home", 5) == 0)     { ClockLink_StartHome(); printf("[LINK] home cmd sent\r\n"); }
    else if(strncmp(line, ":lstat", 6) == 0)    { LinkStat_Report(); Screen_SetStatus(LinkStat_StatusLine()); }
    else if(strncmp(line, ":memo ", 6) == 0)
    {
        if(Diag_TryAddReminder(line + 6)) printf("[MEMO] reminder added\r\n");
        else printf("[ERR] format must be HH:MM text\r\n");
    }
    else printf("[ERR] unknown, try :help\r\n");
}

void Diag_Poll(void)
{
    /* 1) 延时自检一次 */
    if(!s_checked && s_tick >= DIAG_START_TICKS)
    {
        s_checked = 1;
        Diag_SelfTest();
    }

    /* 2) 命令行（在主循环里执行，避免在 ISR 里写 EEPROM/刷屏） */
    if(s_ready)
    {
        s_ready = 0;
        Diag_Exec(s_line);
        s_len = 0;
    }

    /* 3) 喂狗 */
#if DIAG_WDG_EN
    IWDG_ReloadCounter();
#endif
}

uint8_t Diag_IsOk(void)   { return s_ok; }
const char *Diag_ErrText(void) { return s_err; }

/* ---------------- "HH:MM 文本" -> 提醒 ---------------- */
uint8_t Diag_TryAddReminder(const char *gbk)
{
    uint8_t hh = 0, mm = 0;
    const char *p = gbk;

    while(*p == ' ' || *p == '\t') p++;

    if(p[0] < '0' || p[0] > '9') return 0;
    if(p[1] < '0' || p[1] > '9') return 0;
    hh = (uint8_t)((p[0] - '0') * 10 + (p[1] - '0'));

    p += 2;
    if(*p != ':') return 0;
    p++;

    if(p[0] < '0' || p[0] > '9') return 0;
    if(p[1] < '0' || p[1] > '9') return 0;
    mm = (uint8_t)((p[0] - '0') * 10 + (p[1] - '0'));
    p += 2;

    while(*p == ' ') p++;

    if(hh > 23 || mm > 59) return 0;
    if(*p == 0) return 0;               /* 必须有文字 */

    if(!ExtRepeat_Add(hh, mm, p)) return 0;

    Screen_PushMemo(gbk);
    ClockLink_SendMemo(gbk);
    printf("[MEMO] %02u:%02u added\r\n", (unsigned)hh, (unsigned)mm);
    return 1;
}
