#include "stm32f10x.h"
#include "home.h"
#include "Voice_TTS.h"
#include "Variable.h"
#include "PowerDetection.h"
#include "Servo.h"

/* ======================================================================
 *  低电量自动归航：6 状态机
 *
 *  IDLE ──电池<=45%──> LOWBAT ──1.5s──> SCAN ──找到信标──> HOMING
 *    ^                                        │              │
 *    │                                   3s 没扫到            │ 对准
 *    │                                        v              v
 *    │                                     FAULT <──超时── ALIGN ──触点──> CHARGING
 *    └────────────────充满/手动──────────────────────────────────┘
 *
 *  设计要点（答辩可讲）：
 *   1. 阈值带 10% 回差，避免在临界点反复进出；
 *   2. 采样前必须停转（电机启动会让电池端电压瞬降 0.2V 左右）；
 *   3. 连续 5 次同结论才动作，单次抖动不算；
 *   4. 每段都有超时，最后统一 30s 总超时，失败进 FAULT 不做无限重试。
 * ====================================================================== */

/* ---------------- 可配置项 ---------------- */
#define HOME_OFFLINE_TEST   1     /* 1=离线仿真：不接红外/触点也能跑完整个状态机 */
#define HOME_LOG_ENABLE     1     /* 1=USART1 115200 打印（离线测试请拔掉语音模块） */

#define HOME_BAT_LOW        45    /* 触发阈值 % */
#define HOME_BAT_EXIT       55    /* 退出回差 % */
#define HOME_TICK_MS        20

#define HOME_DOCK_BACKWARD  1     /* 1=尾部对接：车体倒退朝信标，转向符号取反 */
                                  /* 0=头部对接：车头朝信标前进 */    /* 调用周期 */

#define IR_L_PORT   GPIOB
#define IR_L_PIN    GPIO_Pin_13
#define IR_R_PORT   GPIOB
#define IR_R_PIN    GPIO_Pin_14
#define DOCK_PORT   GPIOB
#define DOCK_PIN    GPIO_Pin_15

/* ---------------- 时间（tick = 20ms） ---------------- */
#define MS2TICK(ms)         ((ms)/HOME_TICK_MS)
#define T_LOWBAT_HOLD       MS2TICK(1500)   /* 低电确认后停一下再出发 */
#define T_SCAN_WINDOW       MS2TICK(3000)   /* 最多转 3 秒 */
#define T_SCAN_STEP         MS2TICK(600)    /* 每 0.6 秒换一个角度 */
#define T_HOMING_MAX        MS2TICK(20000)
#define T_ALIGN_MAX         MS2TICK(8000)
#define T_TOTAL_MAX         MS2TICK(30000)
#define T_CONFIRM           MS2TICK(500)    /* 双通道/触点持续 0.5s 才算数 */
#define T_LOST_TOLERANCE    MS2TICK(300)    /* 短时丢信号保持直行 */

/* ================= 静态变量 ================= */

/* ---- SYN6288 语音播报文本（GB2312 字节，home.c 本身是 UTF-8，不能直接写中文）---- */
static const char TTS_LOWBAT[] = {0xB5,0xE7,0xC1,0xBF,0xB2,0xBB,0xD7,0xE3,0xA3,0xAC,0xD7,0xBC,0xB1,0xB8,0xBB,0xD8,0xBC,0xD2,0x00};   /* "电量不足，准备回家" */
static const char TTS_CHARGING[] = {0xBF,0xAA,0xCA,0xBC,0xB3,0xE4,0xB5,0xE7,0x00};   /* "开始充电" */
static const char TTS_FULL[] = {0xB3,0xE4,0xB5,0xE7,0xCD,0xEA,0xB3,0xC9,0x00};   /* "充电完成" */
static Home_State_t s_state = HOME_IDLE;
static uint32_t s_ticks = 0;          /* 总 tick，用于日志时间戳 */
static uint32_t s_state_ticks = 0;    /* 进入当前状态后的 tick */
static uint32_t s_total_ticks = 0;    /* 本次返航已用 tick */
static uint16_t s_bat = 78;           /* 离线仿真的电量 */
static uint8_t  s_ir_l, s_ir_r, s_dock;
static uint16_t s_confirm = 0;
static uint8_t  s_retry = 0;
static int16_t  s_v = 0, s_w = 0;     /* 当前输出的速度指令 */

/* ================= 调试日志 ================= */
#if HOME_LOG_ENABLE
static void Home_LogInit(void)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef usart;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

	gpio.GPIO_Pin   = GPIO_Pin_9;          /* USART1_TX = PA9 */
	gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio);

	usart.USART_BaudRate            = 115200;
	usart.USART_WordLength          = USART_WordLength_8b;
	usart.USART_StopBits            = USART_StopBits_1;
	usart.USART_Parity              = USART_Parity_No;
	usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart.USART_Mode                = USART_Mode_Tx;
	USART_Init(USART1, &usart);
	USART_Cmd(USART1, ENABLE);
}

static void Home_PutChar(char c)
{
	while((USART1->SR & 0x80) == 0);       /* TXE */
	USART1->DR = (uint8_t)c;
}

static void Home_PutStr(const char *s)
{
	while(*s) Home_PutChar(*s++);
}

static void Home_PutNum(uint32_t n)
{
	char b[11]; int i = 0;
	if(n == 0){ Home_PutChar('0'); return; }
	while(n && i < 10){ b[i++] = (char)('0' + n % 10); n /= 10; }
	while(i) Home_PutChar(b[--i]);
}

/* [HOME] 12.34s  IDLE->LOW_BAT  bat=45 */
static void Home_LogState(Home_State_t from)
{
	uint32_t ms = s_ticks * HOME_TICK_MS;
	Home_PutStr("[HOME] ");
	Home_PutNum(ms / 1000); Home_PutChar('.'); Home_PutNum((ms % 1000) / 10); Home_PutStr("s  ");
	Home_PutStr(Home_StateName(from));
	Home_PutStr(" -> ");
	Home_PutStr(Home_StateName(s_state));
	Home_PutStr("  bat=");
	Home_PutNum(s_bat);
	Home_PutStr("  ir=");
	Home_PutChar(s_ir_l ? 'L' : '-');
	Home_PutChar(s_ir_r ? 'R' : '-');
	Home_PutStr("  v=");
	Home_PutNum((uint32_t)(s_v < 0 ? -s_v : s_v));
	Home_PutChar(s_v < 0 ? '-' : '+');
	Home_PutStr("  w=");
	Home_PutNum((uint32_t)(s_w < 0 ? -s_w : s_w));
	Home_PutChar(s_w < 0 ? '-' : '+');
	Home_PutChar('\r'); Home_PutChar('\n');
}
#else
static void Home_LogInit(void){}
static void Home_LogState(Home_State_t from){ (void)from; }
#endif

/* ================= 输入 ================= */
static void Home_GpioInit(void)
{
	GPIO_InitTypeDef gpio;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	gpio.GPIO_Mode  = GPIO_Mode_IPU;       /* 红外接收头：平时高，收到 38k 为低 */
	gpio.GPIO_Pin   = IR_L_PIN | IR_R_PIN;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);

	gpio.GPIO_Mode  = GPIO_Mode_IPU;       /* 触点到位开关/微动：闭合拉到 GND */
	gpio.GPIO_Pin   = DOCK_PIN;
	GPIO_Init(GPIOB, &gpio);
}

/* 采样一次。真机上这里换成「边沿计数 + 20ms 清零」，
   现在先用轮询电平，够跑通流程。 */
static void Home_SampleInputs(void)
{
#if HOME_OFFLINE_TEST
	/* 离线仿真：按状态造数据，让状态机能一路走完 */
	switch(s_state)
	{
		case HOME_SCAN:                     /* 每 0.6s 扫到一个方向 */
			s_ir_l = ((s_state_ticks / T_SCAN_STEP) % 2 == 1);
			s_ir_r = !s_ir_l;
			break;
		case HOME_HOMING:                   /* 两边都收到 = 正对着信标 */
			s_ir_l = 1; s_ir_r = 1;
			break;
		case HOME_ALIGN:                    /* 1.5s 后“顶到触点” */
			s_ir_l = 1; s_ir_r = 1;
			s_dock = (s_state_ticks > MS2TICK(1500)) ? 1 : 0;
			break;
		default:
			s_ir_l = 0; s_ir_r = 0; s_dock = 0;
			break;
	}
#else
	/* 红外是低有效，取反成「收到 = 1」 */
	s_ir_l = (GPIO_ReadInputDataBit(IR_L_PORT, IR_L_PIN) == Bit_RESET) ? 1 : 0;
	s_ir_r = (GPIO_ReadInputDataBit(IR_R_PORT, IR_R_PIN) == Bit_RESET) ? 1 : 0;
	s_dock = (GPIO_ReadInputDataBit(DOCK_PORT, DOCK_PIN) == Bit_RESET) ? 1 : 0;
#endif
}

/* 电量：真机直接用 CurBattery；离线用仿真值 */
static uint16_t Home_Battery(void)
{
#if HOME_OFFLINE_TEST
	return s_bat;
#else
	return CurBattery;
#endif
}

static uint8_t Home_Charging(void)
{
#if HOME_OFFLINE_TEST
	return (s_state == HOME_CHARGING) ? 1 : 0;
#else
	return Charge_Detect();
#endif
}

/* ================= 输出 ================= */
/* ================= 四足步态（非阻塞，20ms 一步） =================
 * 相位序列直接照搬 PetAction.c 里的 Action_advance/back/Lrotation/Rrotation，
 * 但不用 Delay_ms，改由 Home_Tick 每 20ms 推进一步，绝不阻塞状态机。
 * 腿 = Servo_Angle1/4（一对角线）与 2/3（另一对）；
 * 尾巴交给 PetAction 的 Action_SwingTail（Action_Mode=9），互不抢定时器。 */

#define A_SKIP   0xFF

#define GAIT_STOP   0
#define GAIT_FWD    1
#define GAIT_BACK   2
#define GAIT_LEFT   3
#define GAIT_RIGHT  4

typedef struct { uint8_t a1, a2, a3, a4; } GaitStep;

static const GaitStep gait_fwd[8] = {          /* 前进：同 Action_advance */
	{ A_SKIP,  45,  45, A_SKIP },
	{ 135,  A_SKIP, A_SKIP, 135 },
	{ A_SKIP,  90,  90, A_SKIP },
	{  90,  A_SKIP, A_SKIP,  90 },
	{  45,  A_SKIP, A_SKIP,  45 },
	{ A_SKIP, 135, 135, A_SKIP },
	{  90,  A_SKIP, A_SKIP,  90 },
	{ A_SKIP,  90,  90, A_SKIP },
};

static const GaitStep gait_back[8] = {         /* 后退：同 Action_back */
	{ A_SKIP, 135, 135, A_SKIP },
	{  45,  A_SKIP, A_SKIP,  45 },
	{ A_SKIP,  90,  90, A_SKIP },
	{  90,  A_SKIP, A_SKIP,  90 },
	{ 135,  A_SKIP, A_SKIP, 135 },
	{ A_SKIP,  45,  45, A_SKIP },
	{  90,  A_SKIP, A_SKIP,  90 },
	{ A_SKIP,  90,  90, A_SKIP },
};

static const GaitStep gait_left[4] = {         /* 左转：同 Action_Lrotation */
	{ A_SKIP,  45, 135, A_SKIP },
	{  45,  A_SKIP, A_SKIP, 135 },
	{ A_SKIP,  90,  90, A_SKIP },
	{  90,  A_SKIP, A_SKIP,  90 },
};

static const GaitStep gait_right[4] = {        /* 右转：同 Action_Rrotation */
	{  45,  A_SKIP, A_SKIP, 135 },
	{ A_SKIP,  45, 135, A_SKIP },
	{  90,  A_SKIP, A_SKIP,  90 },
	{ A_SKIP,  90,  90, A_SKIP },
};

static uint8_t  s_gait    = GAIT_STOP;
static uint8_t  s_gidx    = 0;
static uint8_t  s_gcount  = 0;
static uint16_t s_gacc    = 0;
static uint16_t s_gperiod = 10;      /* 每相位 tick 数，10 tick = 200ms */

static void Gait_WriteStep(const GaitStep *st)
{
	if(st->a1 != A_SKIP) Servo_Angle1(st->a1);
	if(st->a2 != A_SKIP) Servo_Angle2(st->a2);
	if(st->a3 != A_SKIP) Servo_Angle3(st->a3);
	if(st->a4 != A_SKIP) Servo_Angle4(st->a4);
}

static void Gait_Stand(void)
{
	int16_t pose[4];
	pose[0] = 90; pose[1] = 90; pose[2] = 90; pose[3] = 90;
	Servo_SetAnglesLimited(pose, 80);   /* 4 legs -> 2+2 with 80ms gap */
}

static void Home_GaitTick(void)
{
	const GaitStep *tab;

	if(s_gait == GAIT_STOP) return;
	if(++s_gacc < s_gperiod) return;
	s_gacc = 0;

	switch(s_gait)
	{
		case GAIT_FWD:   tab = gait_fwd;   break;
		case GAIT_BACK:  tab = gait_back;  break;
		case GAIT_LEFT:  tab = gait_left;  break;
		case GAIT_RIGHT: tab = gait_right; break;
		default:         return;
	}

	Gait_WriteStep(&tab[s_gidx]);
	if(++s_gidx >= s_gcount) s_gidx = 0;
}

void Home_ApplyMotion(int16_t v, int16_t w)
{
	uint8_t dir;
	int16_t sp;
	uint16_t per;

	s_v = v; s_w = w;

#if HOME_DOCK_BACKWARD
	v = (int16_t)(-v);
	w = (int16_t)(-w);
#endif

	if(w <= -20)      dir = GAIT_LEFT;
	else if(w >= 20)  dir = GAIT_RIGHT;
	else if(v >  10)  dir = GAIT_FWD;
	else if(v < -10)  dir = GAIT_BACK;
	else              dir = GAIT_STOP;

	if(dir != s_gait)
	{
		s_gait = dir;
		s_gidx = 0;
		s_gacc = 0;

		if(dir == GAIT_STOP)
		{
			Gait_Stand();
			Sustainedmove = 0;
			PAnumbers     = 0;
		}
		else
		{
			s_gcount = (dir == GAIT_FWD || dir == GAIT_BACK) ? 8 : 4;
			/* 让主循环停在「摇尾巴」态，避免 PetAction 去抢四条腿 */
			Action_Mode   = 9;
			Sustainedmove = 1;
			PAnumbers     = 0;
		}
	}

	if(dir == GAIT_STOP) return;

	/* 速度 -> 相位周期：>=55 快(150ms) / >=35 中(200ms) / 其余慢(280ms) */
	sp = (v < 0) ? (int16_t)(-v) : v;
	if(w < -20 || w > 20)
	{
		sp = (w < 0) ? (int16_t)(-w) : w;
	}
	if(sp >= 55)      per = 150;
	else if(sp >= 35) per = 200;
	else              per = 280;

	s_gperiod = (uint16_t)(per / HOME_TICK_MS);
	if(s_gperiod < 1) s_gperiod = 1;
}

static void Home_Goto(Home_State_t s)
{
	Home_State_t from = s_state;
	s_state = s;
	s_state_ticks = 0;
	s_confirm = 0;
	if(s == HOME_LOWBAT) s_total_ticks = 0;
	if(s == HOME_IDLE || s == HOME_FAULT) Home_ApplyMotion(0, 0);

	/* ---- 关键节点语音播报（只在状态切换时播一次）---- */
	if(s == HOME_LOWBAT && from != HOME_LOWBAT)
	{
		TTS_Speak(TTS_LOWBAT);
	}
	else if(s == HOME_CHARGING && from != HOME_CHARGING)
	{
		TTS_Speak(TTS_CHARGING);
	}
	else if(s == HOME_IDLE && from == HOME_CHARGING)
	{
		TTS_Speak(TTS_FULL);
	}

	Home_LogState(from);
}

/* ================= 对外接口 ================= */
void Home_Init(void)
{
	Home_GpioInit();
	Home_LogInit();
	s_state = HOME_IDLE;
	s_ticks = 0; s_state_ticks = 0; s_total_ticks = 0;
	s_confirm = 0; s_retry = 0;
	Home_ApplyMotion(0, 0);
#if HOME_LOG_ENABLE
	Home_PutStr("\r\n[HOME] ready  bat_low=");
	Home_PutNum(HOME_BAT_LOW);
	Home_PutStr("%  mode=");
#if HOME_OFFLINE_TEST
	Home_PutStr("OFFLINE_TEST");
#else
	Home_PutStr("REAL");
#endif
	Home_PutChar('\r'); Home_PutChar('\n');
#endif
}

void Home_Start(void)
{
	if(s_state == HOME_IDLE || s_state == HOME_FAULT) Home_Goto(HOME_LOWBAT);
}

void Home_Abort(void)
{
	Home_Goto(HOME_IDLE);
}

void Home_SimBattery(uint16_t pct)
{
	if(pct > 100) pct = 100;
	s_bat = pct;
}

Home_State_t Home_GetState(void){ return s_state; }

const char* Home_StateName(Home_State_t s)
{
	switch(s)
	{
		case HOME_IDLE:     return "IDLE";
		case HOME_LOWBAT:   return "LOW_BAT";
		case HOME_SCAN:     return "SCAN";
		case HOME_HOMING:   return "HOMING";
		case HOME_ALIGN:    return "ALIGN";
		case HOME_CHARGING: return "CHARGING";
		case HOME_FAULT:    return "FAULT";
		default:            return "?";
	}
}

/* ================= 状态机主体（20ms 一次） ================= */
void Home_Tick(void)
{
	uint16_t bat;

	s_ticks++;
	s_state_ticks++;
	if(s_state != HOME_IDLE && s_state != HOME_FAULT) s_total_ticks++;

	Home_SampleInputs();
	bat = Home_Battery();

	Home_GaitTick();

	/* 总超时：除了 IDLE/CHARGING/FAULT，其他状态共用一个 30s 预算 */
	if(s_total_ticks > T_TOTAL_MAX &&
	   s_state != HOME_IDLE && s_state != HOME_CHARGING && s_state != HOME_FAULT)
	{
		Home_Goto(HOME_FAULT);
		return;
	}

	switch(s_state)
	{
	case HOME_IDLE:
#if HOME_OFFLINE_TEST
		/* 离线仿真：电量每 2 秒掉 1%，降到阈值自动触发返航 */
		if(s_state_ticks % 100 == 0 && s_bat > HOME_BAT_LOW) s_bat--;
#endif
		if(bat <= HOME_BAT_LOW)
		{
			Home_ApplyMotion(0, 0);        /* 先停稳，再采样 */
			Home_Goto(HOME_LOWBAT);
		}
		break;

	case HOME_LOWBAT:
		/* 停 1.5s 让电压回稳，同时给一个「我要回家」的表情 */
		Face_Mode = 1;
		if(s_state_ticks >= T_LOWBAT_HOLD) Home_Goto(HOME_SCAN);
		break;

	case HOME_SCAN:
		/* 原地慢转找信标：左收到往左转，右收到往右转 */
		if(s_ir_l && !s_ir_r)      Home_ApplyMotion(0, -40);
		else if(s_ir_r && !s_ir_l) Home_ApplyMotion(0,  40);
		else                       Home_ApplyMotion(0,  25);   /* 都没收到，慢慢转 */
		if(s_ir_l && s_ir_r)
		{
			if(++s_confirm >= T_CONFIRM) Home_Goto(HOME_HOMING);
		}
		else s_confirm = 0;

		if(s_state_ticks >= T_SCAN_WINDOW) Home_Goto(HOME_FAULT);   /* 3s 没扫到 */
		break;

	case HOME_HOMING:
		/* 双通道都收到 = 正对，直行；单边收到 = 往那边修 */
		if(s_ir_l && s_ir_r)      Home_ApplyMotion(60, 0);
		else if(s_ir_l)           Home_ApplyMotion(40, -35);
		else if(s_ir_r)           Home_ApplyMotion(40,  35);
		else
		{
			/* 短时丢信号：保持直行冲一下（V 槽会兜住） */
			if(s_state_ticks < T_HOMING_MAX) Home_ApplyMotion(40, 0);
		}

		/* 进槽条件：双通道稳定 0.5s，或者检测到触点 */
		if((s_ir_l && s_ir_r) || s_dock)
		{
			if(++s_confirm >= T_CONFIRM) Home_Goto(HOME_ALIGN);
		}
		else s_confirm = 0;

		if(s_state_ticks >= T_HOMING_MAX)          /* 20s 还没靠近 */
		{
			if(++s_retry <= 2) Home_Goto(HOME_SCAN);   /* 重试 2 次再回 SCAN */
			else Home_Goto(HOME_FAULT);
		}
		break;

	case HOME_ALIGN:
		/* 低速直顶，靠 V 槽被动导正，触点接触即停 */
		Home_ApplyMotion(35, 0);
		if(s_dock)
		{
			if(++s_confirm >= T_CONFIRM)
			{
				Home_ApplyMotion(0, 0);        /* 先断电停住 */
				Face_Mode = 3;                 /* 开心 */
				Home_Goto(HOME_CHARGING);
			}
		}
		else s_confirm = 0;

		if(s_state_ticks >= T_ALIGN_MAX)   /* 8s 没顶到，退出来重试 */
		{
			Home_ApplyMotion(-30, 0);      /* 退 0.4s */
			if(s_state_ticks >= T_ALIGN_MAX + MS2TICK(400))
			{
				Home_ApplyMotion(0, 0);
				if(++s_retry <= 2) Home_Goto(HOME_SCAN);
				else Home_Goto(HOME_FAULT);
			}
		}
		break;

	case HOME_CHARGING:
		/* 充电中：屏上显示 Chg（Face_Config 里读 Charge_Detect），这里只等满 */
		Home_ApplyMotion(0, 0);
#if HOME_OFFLINE_TEST
		if(s_state_ticks % 50 == 0 && s_bat < 100) s_bat++;   /* 仿真：1s 涨 1% */
#endif
		if(bat >= 100 || !Home_Charging())
		{
			if(bat >= HOME_BAT_EXIT) Home_Goto(HOME_IDLE);     /* 充满，回待机 */
		}
		break;

	case HOME_FAULT:
		/* 停住等人：屏上表情 + 等语音/蓝牙复位，或者电池回升 */
		Home_ApplyMotion(0, 0);
		Face_Mode = 1;
		if(bat >= HOME_BAT_EXIT) Home_Goto(HOME_IDLE);
		break;

	default:
		Home_Goto(HOME_IDLE);
		break;
	}
}
