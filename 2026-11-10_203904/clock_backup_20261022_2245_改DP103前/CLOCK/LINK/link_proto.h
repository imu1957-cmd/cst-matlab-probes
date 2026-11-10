#ifndef __LINK_PROTO_H_
#define __LINK_PROTO_H_

#include "stm32f10x.h"

/* ======================================================================
 *  两端（时钟 <-> 宠物）串口帧协议
 *
 *  帧格式： AA 55 | TYPE | LEN | SEQ | PAYLOAD[LEN] | CRC8
 *    - AA 55  帧头
 *    - TYPE   1 字节，见下面的 LP_T_xxx
 *    - LEN    负载长度 0..LP_MAX_PAYLOAD
 *    - SEQ    序号，应答里原样带回，用来配对
 *    - CRC8   poly=0x07, init=0x00，覆盖 TYPE..PAYLOAD
 *
 *  三条工程约束：
 *    1. 帧头错位只丢一个字节重新找，不整帧丢；
 *    2. 发出去 3 帧（约 300ms）没应答就重发，最多 3 次；
 *    3. 校验错只计数不重传，避免两端互相拖死。
 * ====================================================================== */

#define LP_SOF0          0xAA
#define LP_SOF1          0x55
#define LP_MAX_PAYLOAD   96

/* 帧类型 */
#define LP_T_WEATHER     0x01   /* 时钟 -> 宠物：天气（预留） */
#define LP_T_MEMO        0x02   /* 时钟 -> 宠物：备忘录文本（ASCII/GBK，<=64 字节） */
#define LP_T_BATTERY     0x03   /* 双向：电量上报/查询 */
#define LP_T_CMD         0x04   /* 时钟 -> 宠物：动作命令字 0x29..0x50 */
#define LP_T_ACK         0x05   /* 应答：payload[0]=0 成功，非 0 失败 */
#define LP_T_HEARTBEAT   0x06   /* 心跳 */
#define LP_T_HOME_START  0x07   /* 时钟 -> 宠物：开始低电返航 */
#define LP_T_HOME_STATE  0x08   /* 宠物 -> 时钟：归航状态上报 */

typedef struct
{
	uint8_t type;
	uint8_t len;
	uint8_t seq;
	uint8_t payload[LP_MAX_PAYLOAD];
} LpFrame;

/* 接收解析器（逐字节状态机，可以放在中断里喂） */
typedef struct
{
	uint8_t  state;     /* 0=等AA 1=等55 2=TYPE 3=LEN 4=SEQ 5=PAYLOAD 6=CRC */
	uint8_t  type;
	uint8_t  len;
	uint8_t  seq;
	uint8_t  idx;
	uint8_t  crc;
	uint8_t  payload[LP_MAX_PAYLOAD];
	uint16_t err_crc;   /* 校验错计数 */
	uint16_t err_len;   /* 长度非法计数 */
} LpParser;

void     Lp_Init(LpParser *p);
uint8_t  Lp_Push(LpParser *p, uint8_t byte, LpFrame *out);   /* 返回 1 = 收齐一帧 */
uint8_t  Lp_InFrame(const LpParser *p);
uint8_t  Lp_Crc8(const uint8_t *data, uint16_t len);
uint16_t Lp_Build(uint8_t *buf, uint8_t type, uint8_t seq,
                  const uint8_t *payload, uint8_t len);      /* 返回整帧长度 */

#endif
