#include "stm32f10x.h"
#include "Voice_TTS.h"

/* ======================================================================
 *  SYN6288 帧格式
 *   [0xFD][0x00][长度H][长度L][0x01 合成播放][0x01 GB2312][文本...][XOR]
 *   长度 = 文本长度 + 3（命令 1 + 编码 1 + 校验 1）
 *   校验 = 从「命令」起到「文本」末，全部字节异或
 * ====================================================================== */

#define TTS_MAX_BYTES   100
#define TTS_HEAD        0xFD
#define TTS_CMD_PLAY    0x01
#define TTS_ENC_GB2312  0x01

static uint8_t  s_busy = 0;
static uint32_t s_lock_ms = 0;

static void TTS_PutByte(uint8_t b)
{
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {}
	USART_SendData(USART1, b);
}

void TTS_Init(void)
{
	s_busy = 0;
	s_lock_ms = 0;
}

uint8_t TTS_Speak(const char *gb2312_text)
{
	uint8_t  i, n = 0, x = 0;
	uint16_t len;

	if(gb2312_text == 0) return 0;
	if(s_busy) return 0;                       /* 正在播，丢弃本次，不排队 */

	while(gb2312_text[n] && n < TTS_MAX_BYTES) n++;
	if(n == 0) return 0;

	len = (uint16_t)(n + 3);

	TTS_PutByte(TTS_HEAD);
	TTS_PutByte(0x00);
	TTS_PutByte((uint8_t)(len >> 8));
	TTS_PutByte((uint8_t)(len & 0xFF));

	TTS_PutByte(TTS_CMD_PLAY);  x ^= TTS_CMD_PLAY;
	TTS_PutByte(TTS_ENC_GB2312);x ^= TTS_ENC_GB2312;

	for(i = 0; i < n; i++)
	{
		TTS_PutByte((uint8_t)gb2312_text[i]);
		x ^= (uint8_t)gb2312_text[i];
	}
	TTS_PutByte(x);                            /* 异或校验 */

	/* 估算播放时长：中文（高位为 1 的 GB2312）约 250ms/字，ASCII 约 90ms/字符 */
	s_lock_ms = 300;
	for(i = 0; i < n; i++)
		s_lock_ms += (gb2312_text[i] & 0x80) ? 250 : 90;

	s_busy = 1;
	return 1;
}

void TTS_Tick20ms(void)
{
	if(!s_busy) return;
	if(s_lock_ms > 20) s_lock_ms -= 20;
	else
	{
		s_lock_ms = 0;
		s_busy = 0;
	}
}

uint8_t TTS_IsBusy(void)
{
	return s_busy;
}

void TTS_Stop(void)
{
	s_busy = 0;
	s_lock_ms = 0;
}
