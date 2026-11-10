#include "stm32f10x.h"
#include "link_proto.h"
#include "clock_link.h"

/* ======================================================================
 *  时钟端链路：USART3 (PB10=TX, PB11=RX, 115200, 8N1)
 *
 *  - 中断只收字节，解析在主循环 ClockLink_Poll()
 *  - 20ms 节拍里做重发与超时：一帧 300ms 没 ACK 就重发，最多 3 次
 *  - 宠物端 5 秒上报一次电量；10 秒收不到任何帧就判离线
 *
 *  注意：control.c 里 My_Init() 会把 PB10/PB11 配成按键输入，
 *        所以必须在 My_Init() 之后再调 ClockLink_Init()。
 * ====================================================================== */

static LpParser s_parser;
static LpFrame  s_frame;
static volatile uint8_t s_ready = 0;
static uint8_t  s_seq = 1;

/* 待重发的帧 */
static uint8_t  s_pend_buf[6 + LP_MAX_PAYLOAD];
static uint16_t s_pend_len = 0;
static uint8_t  s_pend_seq = 0;
static uint8_t  s_pend_try = 0;
static uint16_t s_pend_wait = 0;

static uint16_t s_tick = 0;
static uint16_t s_online = 0;

uint8_t ClockLink_PetBat = 0;
uint8_t ClockLink_PetOnline = 0;
uint8_t ClockLink_HomeState = 0xFF;

/* ---------------- 底层收发 ---------------- */
static void USART3_SendByte(uint8_t b)
{
	uint32_t guard = 0;
	while((USART3->SR & 0x80) == 0) { if(++guard > 40000) return; }
	USART3->DR = b;
}

static void ClockLink_RawSend(const uint8_t *buf, uint16_t len)
{
	uint16_t i;
	for(i = 0; i < len; i++) USART3_SendByte(buf[i]);
	while((USART3->SR & 0x40) == 0);
}

/* 发一帧，并登记为「等 ACK」 */
static void ClockLink_Send(uint8_t type, const uint8_t *payload, uint8_t len)
{
	uint16_t n;

	s_pend_seq = s_seq++;
	n = Lp_Build(s_pend_buf, type, s_pend_seq, payload, len);
	s_pend_len = n;
	s_pend_try = 0;
	s_pend_wait = 0;
	ClockLink_RawSend(s_pend_buf, s_pend_len);
}

/* ---------------- 初始化 ---------------- */
void ClockLink_Init(void)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef usart;
	NVIC_InitTypeDef nvic;

	Lp_Init(&s_parser);
	s_ready = 0;
	s_tick = 0;
	s_online = 0;
	s_pend_len = 0;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	/* PB10 = USART3_TX */
	gpio.GPIO_Pin   = GPIO_Pin_10;
	gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);

	/* PB11 = USART3_RX */
	gpio.GPIO_Pin  = GPIO_Pin_11;
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &gpio);

	usart.USART_BaudRate            = 115200;
	usart.USART_WordLength          = USART_WordLength_8b;
	usart.USART_StopBits            = USART_StopBits_1;
	usart.USART_Parity              = USART_Parity_No;
	usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(USART3, &usart);

	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

	/* 抢占优先级 2、响应 2：比涂鸦(0)和定时器(1)都低，不抢关键节拍 */
	nvic.NVIC_IRQChannel                   = USART3_IRQn;
	nvic.NVIC_IRQChannelPreemptionPriority = 2;
	nvic.NVIC_IRQChannelSubPriority        = 2;
	nvic.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init(&nvic);

	USART_Cmd(USART3, ENABLE);
}

/* ---------------- 接收中断 ---------------- */
void USART3_IRQHandler(void)
{
	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
	{
		LpFrame f;
		uint8_t b = (uint8_t)USART_ReceiveData(USART3);
		if(Lp_Push(&s_parser, b, &f))
		{
			s_frame = f;
			s_ready = 1;
		}
		USART_ClearITPendingBit(USART3, USART_IT_RXNE);
	}
}

/* ---------------- 派发 ---------------- */
void ClockLink_Poll(void)
{
	if(!s_ready) return;
	s_ready = 0;

	switch(s_frame.type)
	{
	case LP_T_BATTERY:                         /* 宠物上报电量 */
		if(s_frame.len >= 1) ClockLink_PetBat = s_frame.payload[0];
		ClockLink_PetOnline = 1;
		s_online = 0;
		break;

	case LP_T_ACK:                             /* 应答：配对序号，清待重发 */
		if(s_frame.seq == s_pend_seq) s_pend_len = 0;
		break;

	case LP_T_HOME_STATE:                      /* 归航状态 */
		if(s_frame.len >= 1) ClockLink_HomeState = s_frame.payload[0];
		ClockLink_PetOnline = 1;
		s_online = 0;
		break;

	case LP_T_HEARTBEAT:
		ClockLink_PetOnline = 1;
		s_online = 0;
		break;

	default:
		break;
	}
}

/* ---------------- 20ms 节拍：重发 + 离线判定 ---------------- */
void ClockLink_Tick20ms(void)
{
	s_tick++;

	/* 待重发：300ms 没 ACK 重发，最多 3 次 */
	if(s_pend_len)
	{
		if(++s_pend_wait >= 15)                /* 15 * 20ms = 300ms */
		{
			s_pend_wait = 0;
			if(++s_pend_try > 3) s_pend_len = 0;   /* 放弃，不再死等 */
			else ClockLink_RawSend(s_pend_buf, s_pend_len);
		}
	}

	/* 离线判定：10 秒（500 tick）没收到任何帧 */
	if(++s_online >= 500)
	{
		s_online = 500;
		ClockLink_PetOnline = 0;
	}
}

/* ---------------- 对外发送接口 ---------------- */
void ClockLink_SendMemo(const char *text)
{
	uint8_t p[LP_MAX_PAYLOAD];
	uint8_t n = 0;
	while(text[n] && n < 64) { p[n] = (uint8_t)text[n]; n++; }
	ClockLink_Send(LP_T_MEMO, p, n);
}

void ClockLink_SendCmd(uint8_t cmd)
{
	ClockLink_Send(LP_T_CMD, &cmd, 1);
}

void ClockLink_QueryBattery(void)
{
	ClockLink_Send(LP_T_BATTERY, 0, 0);
}

void ClockLink_StartHome(void)
{
	ClockLink_Send(LP_T_HOME_START, 0, 0);
}
