/* ======================================================================
 *  ext_linkstat.c  链路质量统计（时钟 <-> 宠物 自定义帧链路）
 *  目标：STM32F103 标准外设库 3.5.0，Keil armcc5（C89），GBK 源文件
 *
 *  统计口径（全部为累计值，溢出回绕不处理）：
 *    rx      = 收完的帧总数（好帧 + 错帧）
 *    err     = 错帧总数（CRC 错 / 长度格式错）
 *    retry   = 重发次数
 *    timeout = 等待应答超时次数
 *    rate    = 最近 100 帧窗口内的错帧率（百分比 x100，0..10000）
 *    last    = 最近一帧是否好帧（OK / ERR）
 *
 *  窗口实现：
 *    100 bit 位图（4 个 uint32 只用低 100 bit），1 = 错帧，0 = 好帧，环形覆盖；
 *    s_win_err 随覆盖同步增减，因此 ErrRate 为 O(1)，不需要扫描位图。
 *
 *  1s / 1min 窗口：
 *    维护"最近 1 秒 / 最近 1 分钟"的收帧数与错帧数，每秒 / 每分钟清零，
 *    供现场用 Keil Watch 窗口观察突发错误；不参与 [LK] 报告格式。
 *
 *  ISR 安全：
 *    OnFrameRx() 在 USART3 中断里调用（写），Report()/ErrRate()/StatusLine()
 *    在主循环里调用（读）。跨上下文状态用 volatile；Cortex-M3 上 32 位量单次
 *    读写是原子的，ErrRate 同时读"窗口错帧/窗口总帧"时可能被中断打断，最坏
 *    相差 1 帧，对展示无影响。
 * ====================================================================== */

#include <stdio.h>
#include "ext_linkstat.h"

/* ---------------- 累计计数器 ---------------- */
static volatile uint32_t s_rx_total;        /* 收到的帧总数（含错帧） */
static volatile uint32_t s_err_total;       /* 错帧总数 */
static volatile uint32_t s_retry_total;     /* 重发次数 */
static volatile uint32_t s_timeout_total;   /* 等待应答超时次数 */

/* ---------------- 最近 100 帧窗口：位图 1=错帧 ---------------- */
static volatile uint32_t s_win[4];          /* 100 bit：word0..2 各 32 bit，word3 用低 4 bit */
static volatile uint32_t s_win_cnt;         /* 窗口内已有帧数 0..100 */
static volatile uint32_t s_win_err;         /* 窗口内错帧数 0..100 */
static volatile uint8_t  s_win_idx;         /* 下一个写入位置 0..99 */

/* ---------------- 1s / 1min 窗口（调试观察用） ---------------- */
static volatile uint32_t s_rx_1s;           /* 本秒内收到的帧数 */
static volatile uint32_t s_err_1s;          /* 本秒内错帧数 */
static volatile uint32_t s_rx_1min;         /* 本分钟内收到的帧数 */
static volatile uint32_t s_err_1min;        /* 本分钟内错帧数 */

/* ---------------- 超时 / 状态 ---------------- */
static volatile uint8_t  s_gap_sec;         /* 距上次好帧的秒数，0xFF 饱和 */
static volatile uint8_t  s_down;            /* 1 = 已判断链 */
static volatile uint8_t  s_last_ok;         /* 最近一帧是否好帧：1=OK */
static volatile uint8_t  s_tick;            /* 20ms 计数 0..49 = 1 秒 */
static volatile uint8_t  s_sec;             /* 秒计数 0..59 = 1 分钟 */

static char s_line[16];                     /* StatusLine 缓冲，最长 "LK OK e100" 10 字符 */

/* 把 1 bit 压入窗口（is_err=1 错帧）；窗口满时覆盖最旧一帧 */
static void LinkStat_WinPush(uint8_t is_err)
{
    uint32_t w;
    uint32_t mask;

    w    = (uint32_t)(s_win_idx >> 5);
    mask = (uint32_t)1UL << (s_win_idx & 31u);

    if(s_win_cnt >= (uint32_t)LINKSTAT_WIN)
    {
        if(s_win[w] & mask) s_win_err--;    /* 被覆盖的旧帧是错帧 */
    }
    else
    {
        s_win_cnt++;
    }

    if(is_err)
    {
        s_win[w] |= mask;
        s_win_err++;
    }
    else
    {
        s_win[w] &= ~mask;
    }

    s_win_idx++;
    if(s_win_idx >= (uint8_t)LINKSTAT_WIN) s_win_idx = 0;
}

void LinkStat_Init(void)
{
    uint8_t i;

    s_rx_total      = 0;
    s_err_total     = 0;
    s_retry_total   = 0;
    s_timeout_total = 0;

    for(i = 0; i < 4u; i++) s_win[i] = 0;
    s_win_cnt = 0;
    s_win_err = 0;
    s_win_idx = 0;

    s_rx_1s    = 0;
    s_err_1s   = 0;
    s_rx_1min  = 0;
    s_err_1min = 0;

    s_gap_sec = 0;
    s_down    = 0;              /* 上电 5 秒内还没判断链，收到好帧就一直 OK */
    s_last_ok = 1;              /* 尚未收到错帧时按 OK 报 */
    s_tick    = 0;
    s_sec     = 0;
    s_line[0] = 0;
}

/* 20ms 节拍：每 1 秒结算一次 1s 窗口 / 断链计时，每 60 秒结算一次 1min 窗口 */
void LinkStat_Tick20ms(void)
{
    s_tick++;
    if(s_tick < 50u) return;    /* 50 x 20ms = 1s */
    s_tick = 0;

    /* 1s 窗口：进入新的一秒，本秒计数清零（可在 Keil Watch 里看上一秒的值） */
    s_rx_1s  = 0;
    s_err_1s = 0;

    /* 断链计时：只有收到好帧才会在 OnFrameRx() 里清零 */
    if(s_gap_sec < 0xFFu) s_gap_sec++;
    s_down = (s_gap_sec > (uint8_t)LINKSTAT_DOWN_SEC) ? 1 : 0;

    /* 1min 窗口 */
    s_sec++;
    if(s_sec >= 60u)
    {
        s_sec = 0;
        s_rx_1min  = 0;
        s_err_1min = 0;
    }
}

/* 每收完一帧调用一次：ok=1 好帧，ok=0 CRC 错 / 长度格式错 */
void LinkStat_OnFrameRx(uint8_t ok)
{
    s_rx_total++;
    s_rx_1s++;
    s_rx_1min++;

    if(ok)
    {
        s_last_ok = 1;
        s_gap_sec = 0;          /* 收到好帧，断链计时与断链标志立即清零 */
        s_down    = 0;
        LinkStat_WinPush(0);
    }
    else
    {
        s_last_ok = 0;
        s_err_total++;
        s_err_1s++;
        s_err_1min++;
        LinkStat_WinPush(1);
    }
}

void LinkStat_OnRetry(void)
{
    s_retry_total++;
}

void LinkStat_OnTimeout(void)
{
    s_timeout_total++;
}

/* 错帧率 = 窗口错帧 / 窗口总帧 x 10000（即百分比 x100），整数除法，无收包返回 0 */
uint16_t LinkStat_ErrRate_x100(void)
{
    uint32_t cnt;
    uint32_t err;

    cnt = s_win_cnt;
    if(cnt == 0u) return 0;
    err = s_win_err;
    return (uint16_t)((err * 10000UL) / cnt);
}

/* 一行统计： [LK] rx=1234 err=2 retry=3 timeout=1 rate=0.16% last=OK */
void LinkStat_Report(void)
{
    uint16_t rate;

    rate = LinkStat_ErrRate_x100();
    printf("[LK] rx=%lu err=%lu retry=%lu timeout=%lu rate=%lu.%02lu%% last=%s\r\n",
           (unsigned long)s_rx_total,
           (unsigned long)s_err_total,
           (unsigned long)s_retry_total,
           (unsigned long)s_timeout_total,
           (unsigned long)(rate / 100u),
           (unsigned long)(rate % 100u),
           s_last_ok ? "OK" : "ERR");
}

/* 上屏状态：断链返回 "LK DOWN"，否则 "LK OK eN"（N = 窗口内错帧数，最多 3 位） */
const char *LinkStat_StatusLine(void)
{
    if(s_down) return "LK DOWN";

    sprintf(s_line, "LK OK e%u", (unsigned)s_win_err);
    return s_line;
}
