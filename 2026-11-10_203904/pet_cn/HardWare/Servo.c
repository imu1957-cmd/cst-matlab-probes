#include "stm32f10x.h"                  // Device header
#include "Servo.h"
#include "Delay.h"

/* 每路舵机最近一次写入的目标角(单位:度), 供 Servo_SetAnglesLimited() 判断
   目标角是否变化; 0x0FFF 表示上电后尚未写入过(合法角度只有 0~180) */
static int16_t Servo_LastAngle[4] = {0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF};

void Servo_Init()
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);//开启TIM2时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);//开启GPIOA时钟

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF_PP;//复用推挽输出模式
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;//默认PA0是TIM2通道1的复用，PA1是TIM2通道2的复用所以开启这俩IO口...
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	
	TIM_InternalClockConfig(TIM2);//TIM2切换为内部定时器
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;//不分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;//向上计数
	TIM_TimeBaseInitStructure.TIM_Period=20000-1;
	TIM_TimeBaseInitStructure.TIM_Prescaler=72-1;
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter=0;
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStructure);
	
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode=TIM_OCMode_PWM1;//输出比较模式采用PWM1
	TIM_OCInitStructure.TIM_OCPolarity=TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState=TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse=0;//初始化CCR的值为0
	TIM_OC1Init(TIM2,&TIM_OCInitStructure);//TIM2复用通道1开启
	TIM_OC2Init(TIM2,&TIM_OCInitStructure);//TIM2复用通道2开启
	TIM_OC3Init(TIM2,&TIM_OCInitStructure);//TIM2复用通道3开启
	TIM_OC4Init(TIM2,&TIM_OCInitStructure);//TIM2复用通道4开启
	
	TIM_Cmd(TIM2,ENABLE);//使能TIM2
}


void PWM_SetCompare1(uint16_t Compare)
{
	TIM_SetCompare1(TIM2, Compare);//设置CCR1的值		
}

void PWM_SetCompare2(uint16_t Compare)
{
	TIM_SetCompare2(TIM2, Compare);//设置CCR2的值
}

void PWM_SetCompare3(uint16_t Compare)
{
	TIM_SetCompare3(TIM2, Compare);//设置CCR3的值
}

void PWM_SetCompare4(uint16_t Compare)
{
	TIM_SetCompare4(TIM2, Compare);//设置CCR4的值
}


void Servo_Angle1(float Angle)//左上
{
	Servo_LastAngle[0]=(int16_t)Angle;
	PWM_SetCompare1(Angle / 180 * 2000 + 500);			
}

void Servo_Angle2(float Angle)//右上
{
	Servo_LastAngle[1]=(int16_t)Angle;
	PWM_SetCompare2((180-Angle) / 180 * 2000 + 500);		
}

void Servo_Angle3(float Angle)//左下
{
	Servo_LastAngle[2]=(int16_t)Angle;
	PWM_SetCompare3(Angle / 180 * 2000 + 500);			
}

void Servo_Angle4(float Angle)//右下
{
	Servo_LastAngle[3]=(int16_t)Angle;
	PWM_SetCompare4((180-Angle) / 180 * 2000 + 500);			
}


/* 限流版四路置角, 防止 5V 被四路舵机同时启停拉低而复位:
   1) 只写目标角有变化的通道, 未变的通道完全跳过;
   2) 前两路(PA0/PA1)与前两路都写完后, 阻塞 step_ms 再写后两路(PA2/PA3),
      因此任一时刻最多只有两路舵机同时启动;
   3) 若只有后两路需要变化, 则不需要等待, 直接写在下一拍;
   4) 若两对都无需变化, 函数立即返回, 不产生任何延时 */
void Servo_SetAnglesLimited(const int16_t a[4], uint16_t step_ms)
{
	uint8_t front=0;
	uint8_t back=0;

	if(a == 0) return;

	if(a[0] != Servo_LastAngle[0]){Servo_Angle1((float)a[0]);front=1;}
	if(a[1] != Servo_LastAngle[1]){Servo_Angle2((float)a[1]);front=1;}

	if(a[2] != Servo_LastAngle[2]) back=1;
	if(a[3] != Servo_LastAngle[3]) back=1;

	/* 前后两对都要动时才需要间隔, 保证两对不会同时起步 */
	if(front && back && step_ms) Delay_ms(step_ms);

	if(a[2] != Servo_LastAngle[2]) Servo_Angle3((float)a[2]);
	if(a[3] != Servo_LastAngle[3]) Servo_Angle4((float)a[3]);
}
