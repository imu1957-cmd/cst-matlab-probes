#ifndef __VOICE_TTS_H
#define __VOICE_TTS_H

#include "stm32f10x.h"

/* ======================================================================
 *  SYN6288 中文语音合成（播报备忘录）
 *
 *  接法（与 CI-03T 共用 USART1 的 TX，单向）：
 *     STM32 PA9 (USART1_TX) ──┬── CI-03T 的 RXD（原有，经 R7 100Ω）
 *                             └── SYN6288 的 RXD
 *     5V  ── SYN6288 VCC（并 100µF + 100nF）
 *     GND ── SYN6288 GND
 *     SYN6288 TXD 悬空不接（只发不读，避免和 CI-03T 抢 PA10）
 *
 *  波特率：9600（和 CI-03T 一致，Voice_Init 里已配好 USART1）
 *
 *  ⚠ 编码限制：SYN6288 只认 GB2312/GBK，不认 UTF-8。
 *    所以传进来的字符串必须是 GB2312 字节流；
 *    中文备忘录需要时钟端先转码（或先只用「数字 + 英文」的备忘）。
 * ====================================================================== */

void TTS_Init(void);

/* 播报一段 GB2312 文本（长度 ≤ 100 字节）。
 * 正在播报时新请求会被丢弃，返回 0；接受返回 1。 */
uint8_t TTS_Speak(const char *gb2312_text);

/* 20ms 调一次，维护「播放锁」超时 */
void TTS_Tick20ms(void);

uint8_t TTS_IsBusy(void);

/* 清掉播放锁（不保证模块停声，停声指令待实测后补） */
void TTS_Stop(void);

#endif
