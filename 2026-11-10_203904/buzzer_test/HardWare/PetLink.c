#include "stm32f10x.h"
#include "link_proto.h"
#include "PetLink.h"
#include "Voice_TTS.h"
#include "Variable.h"
#include "PetAction.h"
#include "PowerDetection.h"
#include "home.h"

/* ======================================================================
 *  宠物端链路：USART3 (PB10=TX, PB11=RX, 115200, 8N1)
 *
 *  - 中断里只做一件事：PetLink_RxByte() 把字节喂进解析器
 *  - 主循环 PetLink_Poll() 负责解析、派发、回 ACK
 *  - 20ms 节拍 PetLink_Tick20ms() 负责心跳/电量上报
 *
 *  兼容原来「单字节 = 动作命令」的用法：
 *  如果收到的字节不在帧里，仍然走老的 PetAction_Judgmengt(1)
 * ====================================================================== */

static LpParser s_parser;
static LpFrame  s_frame;
static volatile uint8_t s_ready = 0;   /* 解析器收齐一帧，等主循环处理 */
static uint8_t  s_tx_seq = 1;
static uint16_t s_tick = 0;
static uint16_t s_last_bat = 0xFFFF;
static volatile uint8_t s_report = 0;  /* 20ms 节拍里置位，主循环里真的发 */

/* 备忘录：时钟下发后放这里，语音模块（SYN6288）以后从这里取 */
char    PetLink_Memo[66] = {0};
uint8_t PetLink_MemoLen = 0;
uint8_t PetLink_MemoNew = 0;

/* ---------------- 发送 ---------------- */
static void USART3_SendByte(uint8_t b)
{
	uint32_t guard = 0;
	while((USART3->SR & 0x80) == 0)          /* TXE */
	{
		if(++guard > 40000) return;           /* 防止卡死 */
	}
	USART3->DR = b;
}

static void PetLink_SendFrame(uint8_t type, uint8_t seq,
                              const uint8_t *payload, uint8_t len)
{
	uint8_t  buf[6 + LP_MAX_PAYLOAD];
	uint16_t n = Lp_Build(buf, type, seq, payload, len);
	uint16_t i;

	for(i = 0; i < n; i++) USART3_SendByte(buf[i]);
	while((USART3->SR & 0x40) == 0);          /* TC，确保发完 */
}

void PetLink_SendAck(uint8_t seq, uint8_t ok)
{
	uint8_t p = ok ? 0 : 1;
	PetLink_SendFrame(LP_T_ACK, seq, &p, 1);
}

void PetLink_SendBattery(uint16_t pct)
{
	uint8_t p[2];
	if(pct > 100) pct = 100;
	p[0] = (uint8_t)pct;
	p[1] = (uint8_t)(pct >> 8);
	PetLink_SendFrame(LP_T_BATTERY, s_tx_seq++, p, 2);
}

void PetLink_SendHomeState(uint8_t state, uint16_t pct)
{
	uint8_t p[3];
	p[0] = state;
	p[1] = (uint8_t)pct;
	p[2] = 0;
	PetLink_SendFrame(LP_T_HOME_STATE, s_tx_seq++, p, 3);
}

void PetLink_SendEvent(uint8_t code)
{
	PetLink_SendFrame(LP_T_ACK, s_tx_seq++, &code, 1);
}

/* ---------------- 接收 ---------------- */
void PetLink_Init(void)
{
	Lp_Init(&s_parser);
	s_ready = 0;
	s_tick = 0;
	s_last_bat = 0xFFFF;
}

uint8_t PetLink_RxByte(uint8_t b)
{
	LpFrame f;
	uint8_t in_frame = Lp_InFrame(&s_parser);

	if(Lp_Push(&s_parser, b, &f))
	{
		s_frame = f;
		s_ready = 1;
	}

	if(in_frame)        return 1;      /* 本来就在帧里，肯定不是单字节指令 */
	if(b == LP_SOF0)    return 1;      /* 帧头第一个字节，也不算指令 */
	return 0;                          /* 交给调用者按老的单字节命令处理 */
}

uint8_t PetLink_InFrame(void)
{
	return Lp_InFrame(&s_parser);
}

uint16_t PetLink_GetCrcErrors(void)
{
	return s_parser.err_crc;
}

/* 帧派发：主循环里跑，别放中断 */
void PetLink_Poll(void)
{
	uint8_t i;

	/* 定时上报电量（节拍由 20ms 定时器置位，这里才真正发，避免中断里长时间阻塞） */
	if(s_report)
	{
		s_report = 0;
		PetLink_SendBattery(CurBattery);
	}

	if(!s_ready) return;
	s_ready = 0;

	switch(s_frame.type)
	{
	case LP_T_MEMO:                       /* 备忘录：存起来 + 回 ACK */
		PetLink_MemoLen = (s_frame.len > 64) ? 64 : s_frame.len;
		for(i = 0; i < PetLink_MemoLen; i++) PetLink_Memo[i] = (char)s_frame.payload[i];
		PetLink_Memo[PetLink_MemoLen] = 0;
		PetLink_MemoNew = 1;
		TTS_Speak(PetLink_Memo);          /* 收到备忘就念（文本需为 GB2312） */
		Face_Mode = 6;                    /* 打招呼表情，表示"有新消息" */
		PetLink_SendAck(s_frame.seq, 1);
		break;

	case LP_T_CMD:                        /* 动作命令字 */
		if(s_frame.len >= 1)
		{
			RxData_Blue = s_frame.payload[0];
			PetAction_Judgmengt(1);
			PetLink_SendAck(s_frame.seq, 0);   /* 0 = 成功 */
		}
		else PetLink_SendAck(s_frame.seq, 1);
		break;

	case LP_T_BATTERY:                    /* 查询电量（len=0） */
		if(s_frame.len == 0) PetLink_SendBattery(CurBattery);
		else PetLink_SendAck(s_frame.seq, 1);
		break;

	case LP_T_HEARTBEAT:
		PetLink_SendAck(s_frame.seq, 1);
		break;

	case LP_T_HOME_START:
		Home_Start();
		PetLink_SendAck(s_frame.seq, 1);
		break;

	case LP_T_ACK:                        /* 收到对方应答，本端暂不处理 */
		break;

	default:
		PetLink_SendAck(s_frame.seq, 0);
		break;
	}
}

/* 20ms 一次：每 5 秒置一次上报标志（真正的发送在 Poll 里） */
void PetLink_Tick20ms(void)
{
	s_tick++;
	if(s_tick >= 250)                 /* 5 秒一次 */
	{
		s_tick = 0;
		if(s_last_bat == 0xFFFF || CurBattery + 2 < s_last_bat || CurBattery > s_last_bat + 2)
		{
			s_last_bat = CurBattery;
		}
		s_report = 1;                 /* 心跳式上报，保证时钟端随时有数 */
		PetLink_SendHomeState(Home_GetState(), (uint16_t)CurBattery);   /* 归航状态上报 */
	}
}
