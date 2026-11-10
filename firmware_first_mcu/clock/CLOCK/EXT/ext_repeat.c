/* ======================================================================
 *  ext_repeat.c  扩展功能：定时备忘录（重复提醒）
 *  目标：STM32F103C8，标准外设库 3.5.0，Keil（GBK 源文件）
 *
 *  对外接口见 ext_repeat.h。本文件只依赖以下已有 API：
 *      rtc.h         calendar.hour / calendar.min / calendar.sec
 *                    calendar.w_year / calendar.w_month / calendar.w_date
 *      screen_ui.h   Screen_PushMemo() / Screen_SetStatus() / Screen_ShowPage()
 *      clock_link.h  ClockLink_SendMemo()
 *      24Cxx_init.h  AT24CXX_ReadOneByte() / AT24CXX_WriteOneByte() /
 *                    AT24CXX_Read()
 *
 *  说明 1：EEPROM 写入是阻塞的
 *      AT24CXX_WriteOneByte() 内部每写 1 个字节就 delay_ms(10)，写满 1 条
 *      记录最坏约 38*10ms = 380ms。所以本模块的划分是：
 *          - ExtRepeat_Tick20ms()（TIM2 的 20ms 中断上下文）里绝对不写 EEPROM；
 *          - 只有 ExtRepeat_Add / Remove / ClearAll 会写，必须在主循环上下文
 *            调用；写之前先读比较，值相同的字节直接跳过，常见情况只写几十字节。
 *
 *  说明 2：不要调用 AT24CXX_Check()
 *      它拿 0xFF 当标志字（24Cxx_init.c:108），而 control.c:126 用
 *      AT24CXX_ReadOneByte(255) == 66 判断"首次使用"，调用它会破坏这个标志。
 *      本模块只读写自己的 0x80..0xF4 段，不碰 0x00..0x12，也不碰 0xFF。
 * ====================================================================== */

#include "ext_repeat.h"
#include "rtc.h"
#include "screen_ui.h"
#include "clock_link.h"
#include "24Cxx_init.h"
#include <stdio.h>      /* sprintf，用法与 screen_ui.c 保持一致 */

/* ---------------- 模块内部状态（全部 static） ---------------- */

typedef struct
{
    uint8_t hour;                       /* 0..23                       */
    uint8_t min;                        /* 0..59                       */
    char    text[EXT_RPT_TEXT_LEN];     /* GBK，<= 35 字节 + 结尾 0    */
} ExtRptItem;

static ExtRptItem s_rpt[EXT_RPT_RAM_MAX];       /* RAM 里最多 8 条  */
static uint8_t    s_cnt = 0;                    /* 有效条数 0..8    */
static uint8_t    s_fired[EXT_RPT_RAM_MAX];     /* 当天是否已触发过 */
static uint8_t    s_inited = 0;                 /* Init 之后才工作  */

/* 跨天判定：日期变化 或 (hour*60+min) 回绕，任一成立就清 fired[] */
static uint32_t   s_day = 0xFFFFFFFFu;
static uint16_t   s_min_of_day = 0xFFFFu;

/* ---------------- 内部函数 ---------------- */

/* 第 i 条记录在 AT24C02 里的地址 */
static uint16_t ExtRepeat_RecAddr(uint8_t i)
{
    return (uint16_t)(EXT_RPT_OFFSET + (uint16_t)i * EXT_RPT_REC_LEN);
}

/* 掉电实际保存的条数：0..EXT_RPT_SAVE_MAX */
static uint8_t ExtRepeat_SaveCnt(void)
{
    return (s_cnt > (uint8_t)EXT_RPT_SAVE_MAX) ? (uint8_t)EXT_RPT_SAVE_MAX : s_cnt;
}

/* 校验和：只覆盖"真正写进 EEPROM 的那些字节"，载入时用同样顺序重算 */
static uint8_t ExtRepeat_Chk(void)
{
    uint8_t i, j, n, sum;

    n   = ExtRepeat_SaveCnt();
    sum = (uint8_t)(0x5Au ^ n);
    for (i = 0; i < n; i++)
    {
        sum = (uint8_t)(sum + s_rpt[i].hour);
        sum = (uint8_t)(sum ^ (uint8_t)(s_rpt[i].min + 0x11u));
        for (j = 0; j < (uint8_t)EXT_RPT_TEXT_LEN && s_rpt[i].text[j] != 0; j++)
        {
            sum = (uint8_t)(sum + (uint8_t)s_rpt[i].text[j]);
            sum = (uint8_t)(sum + j);
        }
    }
    return sum;
}

/* 写 1 个字节到 EEPROM：和现值相同就不写（省掉一次 10ms 的写周期） */
static void ExtRepeat_EeWrite(uint16_t addr, uint8_t val)
{
    if (AT24CXX_ReadOneByte(addr) != val)
    {
        AT24CXX_WriteOneByte(addr, val);
    }
}

/* 把前 3 条记录 + 条数 + 校验和写进 AT24C02，最后才写"有效标志"，
 * 这样中途掉电最多丢掉这次保存，不会留下半条坏数据被当成有效。 */
static void ExtRepeat_Save(void)
{
    uint8_t  i, j, n;
    uint16_t base;

    n = ExtRepeat_SaveCnt();

    for (i = 0; i < n; i++)
    {
        base = ExtRepeat_RecAddr(i);
        ExtRepeat_EeWrite(base, s_rpt[i].hour);
        ExtRepeat_EeWrite((uint16_t)(base + 1u), s_rpt[i].min);
        for (j = 0; j < (uint8_t)EXT_RPT_TEXT_LEN; j++)
        {
            ExtRepeat_EeWrite((uint16_t)(base + 2u + j), (uint8_t)s_rpt[i].text[j]);
            if (s_rpt[i].text[j] == 0) break;     /* 结尾 0 写上就够了 */
        }
    }

    ExtRepeat_EeWrite((uint16_t)EXT_RPT_CNT_ADDR, n);
    ExtRepeat_EeWrite((uint16_t)EXT_RPT_CHK_ADDR, ExtRepeat_Chk());
    ExtRepeat_EeWrite((uint16_t)EXT_RPT_MAGIC_ADDR, (uint8_t)EXT_RPT_MAGIC);
}

/* GBK 文本截断拷贝：最多 35 字节，双字节汉字不拆半，结尾补 0 */
static void ExtRepeat_CopyText(char *dst, const char *src)
{
    uint8_t n = 0;
    uint8_t c;

    while (n < (uint8_t)EXT_RPT_TEXT_MAX && src[n] != 0)
    {
        c = (uint8_t)src[n];
        if (c >= 0x81u)                     /* GBK 双字节首字节 */
        {
            if ((uint16_t)n + 2u > (uint16_t)EXT_RPT_TEXT_MAX) break;
            if (src[n + 1u] == 0) break;    /* 尾部只剩半个汉字 */
            dst[n]      = src[n];
            dst[n + 1u] = src[n + 1u];
            n = (uint8_t)(n + 2u);
        }
        else
        {
            dst[n] = (char)c;
            n++;
        }
    }
    dst[n] = 0;
}

/* 到点触发：屏 + 蓝牙 + 状态行 + 切页 */
static void ExtRepeat_Trigger(uint8_t idx)
{
    char st[20];

    Screen_PushMemo(s_rpt[idx].text);      /* 屏：push 进备忘录环（内部已拷贝） */
    ClockLink_SendMemo(s_rpt[idx].text);   /* 蓝牙：下发宠物播报（内部已拷贝） */
    sprintf(st, "提醒: %02d:%02d", (int)s_rpt[idx].hour, (int)s_rpt[idx].min);
    Screen_SetStatus(st);                  /* 主页状态行 t5 */
    Screen_ShowPage(1);                    /* 切到备忘录页 */
}

/* ---------------- 对外接口 ---------------- */

/* 上电恢复：读 AT24C02 里保存的前 3 条。必须在 AT24CXX_Init() 之后调用 */
void ExtRepeat_Init(void)
{
    uint8_t  i, j, n, ok;
    uint16_t base;
    uint8_t  hour, minute;

    s_cnt        = 0;
    s_inited     = 0;
    s_day        = 0xFFFFFFFFu;
    s_min_of_day = 0xFFFFu;

    for (i = 0; i < (uint8_t)EXT_RPT_RAM_MAX; i++)
    {
        s_rpt[i].hour    = 0;
        s_rpt[i].min     = 0;
        s_rpt[i].text[0] = 0;
        s_fired[i]       = 0;
    }
    s_inited = 1;

    if (AT24CXX_ReadOneByte((uint16_t)EXT_RPT_MAGIC_ADDR) != (uint8_t)EXT_RPT_MAGIC)
    {
        return;                             /* 从没保存过：0 条 */
    }

    n = AT24CXX_ReadOneByte((uint16_t)EXT_RPT_CNT_ADDR);
    if (n > (uint8_t)EXT_RPT_SAVE_MAX) return;   /* 条数非法：0 条 */

    ok = 1;
    for (i = 0; i < n && ok; i++)
    {
        base   = ExtRepeat_RecAddr(i);
        hour   = AT24CXX_ReadOneByte(base);
        minute = AT24CXX_ReadOneByte((uint16_t)(base + 1u));
        if (hour > 23 || minute > 59)
        {
            ok = 0;
            break;
        }
        s_rpt[i].hour = hour;
        s_rpt[i].min  = minute;

        AT24CXX_Read((uint16_t)(base + 2u), (u8 *)s_rpt[i].text, (uint16_t)EXT_RPT_TEXT_LEN);
        s_rpt[i].text[EXT_RPT_TEXT_LEN - 1u] = 0;

        /* 36 字节里必须找得到结尾 0，否则认为数据坏了 */
        ok = 0;
        for (j = 0; j < (uint8_t)(EXT_RPT_TEXT_LEN - 1u); j++)
        {
            if (s_rpt[i].text[j] == 0)
            {
                ok = 1;
                break;
            }
        }
    }

    if (!ok) return;

    s_cnt = n;
    if (AT24CXX_ReadOneByte((uint16_t)EXT_RPT_CHK_ADDR) != ExtRepeat_Chk())
    {
        s_cnt = 0;      /* 校验错当作没有数据，避免拿坏数据乱提醒 */
    }
}

/* 20ms 节拍：sec == 0 且 hour/min 命中、当天没响过才触发。
 * 不写 EEPROM，可在 TIM2 中断里调用。 */
void ExtRepeat_Tick20ms(void)
{
    uint16_t mod;
    uint32_t day;
    uint8_t  i;

    if (!s_inited) return;
    if (calendar.w_year == 2015 || calendar.w_year == 0) return;   /* RTC 未校准 */
    if (calendar.hour > 23 || calendar.min > 59) return;

    mod = (uint16_t)((uint16_t)calendar.hour * 60u + (uint16_t)calendar.min);
    day = (uint32_t)calendar.w_year * 10000u
        + (uint32_t)calendar.w_month * 100u
        + (uint32_t)calendar.w_date;

    if (day != s_day || mod < s_min_of_day)
    {
        for (i = 0; i < (uint8_t)EXT_RPT_RAM_MAX; i++) s_fired[i] = 0;
    }
    s_day        = day;
    s_min_of_day = mod;

    if (calendar.sec != 0) return;          /* 只在整分那一秒触发 */
    if (s_cnt == 0) return;

    for (i = 0; i < s_cnt; i++)
    {
        if (s_fired[i]) continue;
        if (s_rpt[i].hour != calendar.hour) continue;
        if (s_rpt[i].min  != calendar.min)  continue;
        s_fired[i] = 1;                     /* 先置位，免得同一秒重复触发 */
        ExtRepeat_Trigger(i);
    }
}

/* 新增一条：返回 1 成功，0 失败（满 8 条 / 参数非法）。会写 EEPROM */
uint8_t ExtRepeat_Add(uint8_t hour, uint8_t min, const char *gbk_text)
{
    uint8_t slot;

    if (!s_inited || gbk_text == 0 || gbk_text[0] == 0) return 0;
    if (hour > 23 || min > 59) return 0;
    if (s_cnt >= (uint8_t)EXT_RPT_RAM_MAX) return 0;

    slot = s_cnt;
    s_rpt[slot].hour = hour;
    s_rpt[slot].min  = min;
    ExtRepeat_CopyText(s_rpt[slot].text, gbk_text);

    /* 如果正好加在当前这一分钟内，就别立刻再响一次 */
    if (hour == calendar.hour && min == calendar.min && calendar.sec == 0)
    {
        s_fired[slot] = 1;
    }
    else
    {
        s_fired[slot] = 0;
    }

    s_cnt++;
    ExtRepeat_Save();       /* 只保前 3 条，第 4..8 条掉电丢失 */
    return 1;
}

/* 删除第 idx 条（0..Count-1）：返回 1 成功，0 失败。会写 EEPROM */
uint8_t ExtRepeat_Remove(uint8_t idx)
{
    uint8_t i;

    if (!s_inited || idx >= s_cnt) return 0;

    for (i = idx; (uint8_t)(i + 1u) < s_cnt; i++)
    {
        s_rpt[i].hour = s_rpt[i + 1u].hour;
        s_rpt[i].min  = s_rpt[i + 1u].min;
        ExtRepeat_CopyText(s_rpt[i].text, s_rpt[i + 1u].text);
        s_fired[i]    = s_fired[i + 1u];
    }

    i = (uint8_t)(s_cnt - 1u);
    s_rpt[i].hour    = 0;
    s_rpt[i].min     = 0;
    s_rpt[i].text[0] = 0;
    s_fired[i]       = 0;
    s_cnt--;

    ExtRepeat_Save();
    return 1;
}

/* 当前条数 0..EXT_RPT_RAM_MAX */
uint8_t ExtRepeat_Count(void)
{
    return s_cnt;
}

/* 读第 idx 条：越界时 hour/min 给 0、text 给空串 */
void ExtRepeat_Get(uint8_t idx, uint8_t *hour, uint8_t *min, const char **text)
{
    if (hour != 0) *hour = 0;
    if (min  != 0) *min  = 0;
    if (text != 0) *text = "";

    if (!s_inited || idx >= s_cnt) return;

    if (hour != 0) *hour = s_rpt[idx].hour;
    if (min  != 0) *min  = s_rpt[idx].min;
    if (text != 0) *text = s_rpt[idx].text;
}

/* 清空全部（RAM 8 条 + 掉电归档）：返回后 Count() == 0。会写 EEPROM */
void ExtRepeat_ClearAll(void)
{
    uint8_t i;

    for (i = 0; i < (uint8_t)EXT_RPT_RAM_MAX; i++)
    {
        s_rpt[i].hour    = 0;
        s_rpt[i].min     = 0;
        s_rpt[i].text[0] = 0;
        s_fired[i]       = 0;
    }
    s_cnt = 0;

    if (s_inited) ExtRepeat_Save();
}
