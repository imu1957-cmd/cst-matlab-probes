#include "link_proto.h"

/* CRC8：poly 0x07，init 0x00（接收端重算一遍比对） */
uint8_t Lp_Crc8(const uint8_t *data, uint16_t len)
{
	uint8_t crc = 0x00;
	uint16_t i;
	uint8_t b;

	for(i = 0; i < len; i++)
	{
		crc ^= data[i];
		for(b = 0; b < 8; b++)
		{
			if(crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x07);
			else           crc = (uint8_t)(crc << 1);
		}
	}
	return crc;
}

void Lp_Init(LpParser *p)
{
	p->state = 0;
	p->type = p->len = p->seq = p->idx = 0;
	p->err_crc = p->err_len = 0;
}

/* 组帧：buf 至少 6 + LP_MAX_PAYLOAD 字节，返回整帧长度 */
uint16_t Lp_Build(uint8_t *buf, uint8_t type, uint8_t seq,
                  const uint8_t *payload, uint8_t len)
{
	uint8_t  body[3 + LP_MAX_PAYLOAD];
	uint16_t n = 0;
	uint8_t  i;

	if(len > LP_MAX_PAYLOAD) len = LP_MAX_PAYLOAD;

	body[0] = type;
	body[1] = len;
	body[2] = seq;
	for(i = 0; i < len; i++) body[3 + i] = payload[i];

	buf[n++] = LP_SOF0;
	buf[n++] = LP_SOF1;
	for(i = 0; i < (uint8_t)(3 + len); i++) buf[n++] = body[i];
	buf[n++] = Lp_Crc8(body, (uint16_t)(3 + len));
	return n;
}

/* 逐字节喂入。返回 1 表示 out 里有一帧完整且校验通过的帧 */
uint8_t Lp_Push(LpParser *p, uint8_t byte, LpFrame *out)
{
	uint8_t  body[3 + LP_MAX_PAYLOAD];
	uint8_t  i;

	switch(p->state)
	{
	case 0:                                   /* 等 AA */
		if(byte == LP_SOF0) p->state = 1;
		break;

	case 1:                                   /* 等 55 */
		if(byte == LP_SOF1)      p->state = 2;
		else if(byte == LP_SOF0) p->state = 1;   /* 连续 AA，重新等 55 */
		else                     p->state = 0;
		break;

	case 2:                                   /* TYPE */
		p->type  = byte;
		p->state = 3;
		break;

	case 3:                                   /* LEN */
		if(byte > LP_MAX_PAYLOAD)
		{
			p->err_len++;
			p->state = (byte == LP_SOF0) ? 1 : 0;
			break;
		}
		p->len   = byte;
		p->idx   = 0;
		p->state = 4;
		break;

	case 4:                                   /* SEQ */
		p->seq   = byte;
		p->state = (p->len == 0) ? 6 : 5;
		break;

	case 5:                                   /* PAYLOAD */
		p->payload[p->idx++] = byte;
		if(p->idx >= p->len) p->state = 6;
		break;

	case 6:                                   /* CRC */
		body[0] = p->type;
		body[1] = p->len;
		body[2] = p->seq;
		for(i = 0; i < p->len; i++) body[3 + i] = p->payload[i];

		if(Lp_Crc8(body, (uint16_t)(3 + p->len)) == byte)
		{
			out->type = p->type;
			out->len  = p->len;
			out->seq  = p->seq;
			for(i = 0; i < p->len; i++) out->payload[i] = p->payload[i];
			p->state = 0;
			return 1;
		}
		p->err_crc++;
		p->state = 0;
		break;

	default:
		p->state = 0;
		break;
	}
	return 0;
}

uint8_t Lp_InFrame(const LpParser *p)
{
	return (p->state != 0) ? 1 : 0;
}
