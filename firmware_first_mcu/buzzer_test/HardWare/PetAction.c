#include "stm32f10x.h"                  // Device header
#include "Servo.h"
#include "Delay.h"
#include "BlueTooth.h"
#include "Variable.h"
#include "Timer.h"
#include "Guard.h"

	/*舵机位置
	    1        2
	
	

	
	    3        4
	*/
	
	//舵机充当腿，括号里的值为0时表示的是腿向前进的方向甩，90度是站立

void Action_SwingTail(void)//摇尾巴
{
	while(WeiBa_Bit)
	{
		Delay_ms(60);
		if(WeiBa_Dir) /*若方向为正向*/
		{
			for(uint8_t i=WeiBa_Value;i<150;i++)
			{
			 WeiBa_Value=i;
			 WServo_Angle(i);
			 Delay_ms(SwingDelay);
			 if(WeiBa_Bit!=1)break;	
			 if(Action_Mode!=9) break;
			}
			WeiBa_Dir=0;
			if(Action_Mode!=9) break;
		}
		else         /*若方向为负方向*/
		{
			for(uint8_t i=WeiBa_Value;i>30;i--)
			{
			  WeiBa_Value=i;
			  WServo_Angle(i);
			  Delay_ms(SwingDelay);
			  if(WeiBa_Bit!=1)break;
			  if(Action_Mode!=9) break;
			}
			WeiBa_Dir=1;
			if(Action_Mode!=9) break;
		}
		Delay_ms(60);	
	}
	
}


void Action_relaxed_getdowm(void)
{
	const int16_t pose[4]={20,20,160,160};
	Servo_SetAnglesLimited(pose,80);/*限流: 前两路先动,80ms后再动后两路*/
	
	/*摇尾巴判断*/
	Action_Mode=9;
}

void Action_upright(void)//站立
{
	const int16_t pose[4]={90,90,90,90};
	Servo_SetAnglesLimited(pose,80);/*限流: 前两路先动,80ms后再动后两路*/
	
	/*摇尾巴判断*/
	Action_Mode=9;
}

void Action_upright2(void)//站立
{
	Servo_Angle3(90);
	Servo_Angle4(90);
	Delay_ms(80);
	Servo_Angle1(90);
	Servo_Angle2(90);
	
	/*摇尾巴判断*/
	Action_Mode=9;
}

void Action_getdowm(void)//趴下
{
	const int16_t pose[4]={20,20,20,20};
	Servo_SetAnglesLimited(pose,80);/*限流: 前两路先动,80ms后再动后两路*/
	
	/*摇尾巴判断*/
	Action_Mode=9;
}

void Action_sit(void)//坐下
{
	const int16_t pose[4]={90,90,20,20};
	Servo_SetAnglesLimited(pose,80);/*限流: 前两路先动,80ms后再动后两路*/
	
	/*摇尾巴判断*/
	Action_Mode=9;
}



void Action_advance(void)//前进
{

	while(Action_Mode==4)
	{
		PAnumbers=Chongfunumber;
			while((PAnumbers || Sustainedmove)&& Action_Mode==4)
			{
				Servo_Angle2(45);	
				Servo_Angle3(45);
				Delay_ms(SpeedDelay);
				if(Action_Mode!=4)break;
				Servo_Angle1(135);	
				Servo_Angle4(135);
				Delay_ms(SpeedDelay);
				if(Action_Mode!=4)break;
				Servo_Angle2(90);	
				Servo_Angle3(90);
				Delay_ms(SpeedDelay);
				if(Action_Mode!=4)break;
				Servo_Angle1(90);	
				Servo_Angle4(90);
				Delay_ms(SpeedDelay);
				if(Action_Mode!=4)break;
		
				Servo_Angle1(45);	
				Servo_Angle4(45);
				Delay_ms(SpeedDelay);
				if(Action_Mode!=4)break;
				Servo_Angle2(135);	
				Servo_Angle3(135);
				Delay_ms(SpeedDelay);
				if(Action_Mode!=4)break;
				Servo_Angle1(90);	
				Servo_Angle4(90);
				Delay_ms(SpeedDelay);
				if(Action_Mode!=4)break;
				Servo_Angle2(90);	
				Servo_Angle3(90);
				Delay_ms(SpeedDelay);
				if(Action_Mode!=4)break;
				
				PAnumbers--;
			}
			if(Sustainedmove!=1 && Action_Mode==4)
				Action_Mode=2;
	}
}

void Action_back(void)//后退
{
	while(Action_Mode==5)
	{
		PAnumbers=Chongfunumber;
		while((PAnumbers || Sustainedmove) && Action_Mode==5 )
		{
			Servo_Angle2(135);	
			Servo_Angle3(135);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=5)break;
			Servo_Angle1(45);	
			Servo_Angle4(45);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=5)break;
			Servo_Angle2(90);	
			Servo_Angle3(90);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=5)break;
			Servo_Angle1(90);	
			Servo_Angle4(90);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=5)break;
			
			Servo_Angle1(135);	
			Servo_Angle4(135);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=5)break;
			Servo_Angle2(45);	
			Servo_Angle3(45);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=5)break;
			Servo_Angle1(90);	
			Servo_Angle4(90);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=5)break;
			Servo_Angle2(90);	
			Servo_Angle3(90);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=5)break;
			
			PAnumbers--;
		}
		if(Sustainedmove!=1 && Action_Mode==5)
		Action_Mode=2;

		
	}
}

void Action_Lrotation(void)//向左旋转
{
	while(Action_Mode==6)
	{
		PAnumbers=Chongfunumber;
		PAnumbers=PAnumbers+Chongfunumber;
		while((PAnumbers || Sustainedmove) && Action_Mode==6)
		{
			Servo_Angle2(45);
			Servo_Angle3(135);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=6)break;
			Servo_Angle1(45);
			Servo_Angle4(135);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=6)break;
			Servo_Angle2(90);
			Servo_Angle3(90);	
			Delay_ms(SpeedDelay);
			if(Action_Mode!=6)break;
			Servo_Angle1(90);
			Servo_Angle4(90);	
			Delay_ms(SpeedDelay);
			if(Action_Mode!=6)break;
			
			PAnumbers--;
		}
		if(Sustainedmove!=1 && Action_Mode==6)
		Action_Mode=2;
	}

}


void Action_Rrotation(void)//向右旋转
{
	while(Action_Mode==7)
	{
		PAnumbers=Chongfunumber;
		PAnumbers=PAnumbers+Chongfunumber;
		while((PAnumbers || Sustainedmove)  && Action_Mode==7)
		{
			Servo_Angle1(45);
			Servo_Angle4(135);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=7)break;
			Servo_Angle2(45);
			Servo_Angle3(135);
			Delay_ms(SpeedDelay);
			if(Action_Mode!=7)break;
			Servo_Angle1(90);
			Servo_Angle4(90);	
			Delay_ms(SpeedDelay);
			if(Action_Mode!=7)break;
			Servo_Angle2(90);
			Servo_Angle3(90);	
			Delay_ms(SpeedDelay);
			if(Action_Mode!=7)break;
			
			PAnumbers--;
		}
		if(Sustainedmove!=1 && Action_Mode==7)
		Action_Mode=2;
	}

}

void Action_Swing(void)//摇摆
{
	uint16_t SwingNumber=SwingRepeatnumber;
	uint16_t half=(uint16_t)(SwingDelay/2);/*一拍拆两半: 前半拍动前两路, 后半拍动后两路, 整拍时长不变*/
	const int16_t pose[4]={30,30,30,30};

	/*进入摇摆的首个大角度跳变先分两级走, 避免四路同时起步*/
	Servo_SetAnglesLimited(pose,80);

	while(SwingNumber && Action_Mode==8)
	{
		for(uint8_t i=30;i<150;i++)
		{
			Servo_Angle1(i);/*前半拍: 只给前两路新目标*/
			Servo_Angle2(i);
			Delay_ms(half);
			Servo_Angle3(i);/*后半拍: 再给后两路新目标*/
			Servo_Angle4(i);
			Delay_ms((uint16_t)(SwingDelay-half));
			if(Action_Mode!=8)break;
		}
		if(Action_Mode!=8)break;
		for(uint8_t i=150;i>30;i--)
		{
			Servo_Angle1(i);
			Servo_Angle2(i);
			Delay_ms(half);
			Servo_Angle3(i);
			Servo_Angle4(i);
			Delay_ms((uint16_t)(SwingDelay-half));
			if(Action_Mode!=8)break;
		}
		if(Action_Mode!=8)break;
		
		SwingNumber--;
	}
		for(uint8_t i=30;i<90;i++)
		{
			Servo_Angle1(i);
			Servo_Angle2(i);
			Delay_ms(half);
			Servo_Angle3(i);
			Servo_Angle4(i);
			Delay_ms((uint16_t)(SwingDelay-half));
			if(Action_Mode!=8)break;
		}
		if(Action_Mode==8)
		Action_Mode=2;
}




void Action_JumpU(void)//向前跳
{
	if(TiaoTurn==0)
	{
		Servo_Angle1(140);
		Servo_Angle4(35);
		Delay_ms(SpeedDelay);
		
		Servo_Angle2(140);
		Servo_Angle3(35);
		Delay_ms(SpeedDelay+80);
		
		Action_Mode=2;
		TiaoTurn=1;
	}
	else
	{
		Servo_Angle2(140);
		Servo_Angle3(35);
		Delay_ms(SpeedDelay);
		
		Servo_Angle1(140);
		Servo_Angle4(35);
		Delay_ms(SpeedDelay+80);
		
		Action_Mode=2;
		TiaoTurn=0;
	}
}

void Action_JumpD(void)//向后跳
{
	if(TiaoTurn2==0){
		Servo_Angle4(35);
		Servo_Angle1(140);
		Delay_ms(SpeedDelay);
	
		Servo_Angle3(35);
		Servo_Angle2(140);
		Delay_ms(SpeedDelay);
	
		Action_Mode=12;
		TiaoTurn2=1;
	}
	else
	{
		Servo_Angle3(35);
		Servo_Angle2(140);
		Delay_ms(SpeedDelay);
	
		Servo_Angle4(35);
		Servo_Angle1(140);
		Delay_ms(SpeedDelay);
	
		Action_Mode=12;
		TiaoTurn2=0;
	}
}

void Action_Hello(void)
{
	uint16_t HelloNumber=HelloRepeatnumber;
	
	Servo_Angle3(20);
	Servo_Angle4(45);
	Delay_ms(80);
	Servo_Angle1(90);
	while(HelloNumber && Action_Mode==13)
	{
		if(Action_Mode!=13)break;
		for(int i=0;i<=45;i++)
		{
			if(Action_Mode!=13)break;
			Servo_Angle2(i);
			Delay_ms(SwingDelay);
		}
		for(int i=45;i>0;i--)
		{
			if(Action_Mode!=13)break;
			Servo_Angle2(i);
			Delay_ms(SwingDelay);
		}
		if(Action_Mode!=13)break;
		
		HelloNumber--;
	}
	if(Action_Mode==13)
	Action_Mode=2;
}

void Action_stretch(void)//伸懒腰
{
	Servo_Angle3(90);
	Servo_Angle4(90);
	Delay_ms(80);
	for(int i=90;i>10;i--)
	{
		Servo_Angle1(i);
		Servo_Angle2(i);
		if(Action_Mode!=14)break;
		Delay_ms(15);
	}
	for(int i=10;i<90;i++)
	{
		Servo_Angle1(i);
		Servo_Angle2(i);
		if(Action_Mode!=14)break;
		Delay_ms(15);
	}
	for(int i=90;i<170;i++)
	{
		Servo_Angle3(i);
		Servo_Angle4(i);
		if(Action_Mode!=14)break;
		Delay_ms(15);
	}
	
	for(int i=170;i>90;i--)
	{
		Servo_Angle3(i);
		Servo_Angle4(i);
		if(Action_Mode!=14)break;
		Delay_ms(15);
	}
	if(Action_Mode==14)
	Action_Mode=15;
}

void Action_Lstretch(void)//后腿拉伸
{
	int breakvalue=1;
	int temp=3;
	const int16_t pose[4]={90,90,90,90};/*回到站立的限流目标角*/
	while(breakvalue)
	{
		Servo_Angle1(90);
		Servo_Angle2(20);
		Delay_ms(60);
		Servo_Angle4(110);
		for(int i=90;i<180;i++)
		{
			if(Action_Mode!=15)break;
			Servo_Angle3(i);
			Delay_ms(6);
		}
		while(temp && Action_Mode==15)
		{
			for(int i=180;i>150;i--)
			{
				if(Action_Mode!=15)break;
				Servo_Angle3(i);
				Delay_ms(15);
			}
			temp--;
		}
		if(Action_Mode!=15)break;
		Delay_ms(100);
		Servo_SetAnglesLimited(pose,80);/*限流: 前两路先动,80ms后再动后两路*/
		if(Action_Mode!=15)break;
		Delay_ms(100);
		if(Action_Mode!=15)break;
		
		temp=3;
		
		Servo_Angle2(90);
		Servo_Angle1(20);
		if(Action_Mode!=15)break;
		Delay_ms(60);
		Servo_Angle3(110);
		for(int i=90;i<180;i++)
		{
			if(Action_Mode!=15)break;
			Servo_Angle4(i);
			Delay_ms(6);
		}
		while(temp && Action_Mode==15)
		{
			for(int i=180;i>150;i--)
			{
				if(Action_Mode!=15)break;
				Servo_Angle4(i);
				Delay_ms(15);
			}
			temp--;
		}
		if(Action_Mode==15)
		Action_Mode=2;
		
		
		breakvalue=0;
	}
}


/*执行动作*/
void PetAction_Perform(void)
{
		if(Action_Mode==0){Action_relaxed_getdowm();}//放松趴下
		else if(Action_Mode==1){Action_sit();}//坐下
		else if(Action_Mode==2){Action_upright();}//站立
		else if(Action_Mode==3){Action_getdowm();}//趴下
		else if(Action_Mode==4){Action_advance();}//前进
		else if(Action_Mode==5){Action_back();}//后退
		else if(Action_Mode==6){Action_Lrotation();}//左转
		else if(Action_Mode==7){Action_Rrotation();}//右转
		else if(Action_Mode==8){Action_Swing();}//摇摆
		else if(Action_Mode==9){Action_SwingTail();}//摇尾巴
		else if(Action_Mode==10){Action_JumpU();}//前跳
		else if(Action_Mode==11){Action_JumpD();}//后跳
		else if(Action_Mode==12){Action_upright2();}//站立方式2
		else if(Action_Mode==13){Action_Hello();}//打招呼
		else if(Action_Mode==14){Action_stretch();}//伸懒腰
		else if(Action_Mode==15){Action_Lstretch();}//后腿拉伸
}

/*动作判断*/
//Serial_type:串口通信类型：0为语音，1为蓝牙
void PetAction_Judgmengt(uint8_t Serial_type)  
{
	if(Serial_type==0)
	{
		Sustainedmove=0;//持续运动关闭
		switch(RxData_Voice)
		{
			case 0x29://放松的趴下
				Face_Mode=0;
				Action_Mode=0;
				break;
			case 0x30://蹲下
				Face_Mode=1;
				Action_Mode=1;
				break;
			case 0x31://直立
				Face_Mode=5;
				Action_Mode=2;
				break;
			case 0x32://趴下
				Face_Mode=1;
				Action_Mode=3;
				break;
			case 0x33://前进
				if(Guard_BlockForward()){Action_Mode=9;break;}//前方12cm内有障碍/桌沿->站定
				Face_Mode=2;
				Action_Mode=4;
				break;
			case 0x34://前进
				Face_Mode=2;
				Action_Mode=5;
				break;
			case 0x35://左转
				Face_Mode=2;
				Action_Mode=6;
				break;
			case 0x36://右转
				Face_Mode=2;
				Action_Mode=7;
				break;
			case 0x37://摇摆
				Face_Mode=4;
				Action_Mode=8;
				break;
			case 0x38://减少移动延迟，增加移动速度
				if(SpeedDelay==120){Face_Mode=3;}
				if(SpeedDelay>100)
				SpeedDelay-=20;	
				else{Face_Mode=2;SpeedDelay=200;}
				break;	
			case 0x39://减少摇摆延迟，增加摇摆速度
				if(SwingDelay==4){Face_Mode=3;}
				if(SwingDelay>3)
				SwingDelay--;	
				else{Face_Mode=4;SwingDelay=9;}
			break;
			case 0x40://摇尾巴
				WeiBa_Bit^=1;
				Face_Mode=1;
				break;
			case 0x41://向前跳
				Face_Mode=2;
				Action_Mode=10;
				break;
			case 0x42://向后跳
				Face_Mode=2;
				Action_Mode=11;
				break;
			case 0x43://打招呼
				Face_Mode=6;
				Action_Mode=13;
				break;
			case 0x44://开启灯光
				AllLed=1;
				break;
			case 0x45://关闭灯光
				AllLed=0;
				break;
			case 0x46://开启呼吸灯
				BreatheLed=1;
				break;	
			case 0x47://关闭呼吸灯
				BreatheLed=0;
				break;	
			case 0x48://伸懒腰
				Face_Mode=6;
				Action_Mode=14;
				break;	
			case 0x49://拉伸
				Face_Mode=6;
				Action_Mode=15;
				break;	
			case 0x50://显示电量
				Battery_Bit^=1;
				break;	
		}	
	}
	else
	{
		Sustainedmove=1;//持续运动开启
		switch(RxData_Blue)
		{
			case 0x29://放松的趴下
				Face_Mode=0;
				Action_Mode=0;
				break;
			case 0x30://蹲下
				Face_Mode=1;
				Action_Mode=1;
				break;
			case 0x31://直立
				Face_Mode=5;
				Action_Mode=2;
				break;
			case 0x32://趴下
				Face_Mode=1;
				Action_Mode=3;
				break;
			case 0x33://前进
				if(Guard_BlockForward()){Action_Mode=9;break;}//前方12cm内有障碍/桌沿->站定
				Face_Mode=2;
				Action_Mode=4;
				break;
			case 0x34://前进
				Face_Mode=2;
				Action_Mode=5;
				break;
			case 0x35://左转
				Face_Mode=2;
				Action_Mode=6;
				break;
			case 0x36://右转
				Face_Mode=2;
				Action_Mode=7;
				break;
			case 0x37://摇摆
				Face_Mode=4;
				Action_Mode=8;
				break;
			case 0x38://减少移动延迟，增加移动速度
				if(SpeedDelay==120){Face_Mode=3;}
				if(SpeedDelay>100)
				SpeedDelay-=20;	
				else{Face_Mode=2;SpeedDelay=200;}
				break;	
			case 0x39://减少摇摆延迟，增加摇摆速度
				if(SwingDelay==4){Face_Mode=3;}
				if(SwingDelay>3)
				SwingDelay--;	
				else{Face_Mode=4;SwingDelay=9;}
			break;
			case 0x40://摇尾巴
				WeiBa_Bit^=1;
				Face_Mode=1;
				break;
			case 0x41://向前跳
				Face_Mode=2;
				Action_Mode=10;
				break;
			case 0x42://向后跳
				Face_Mode=2;
				Action_Mode=11;
				break;
			case 0x43://打招呼
				Face_Mode=6;
				Action_Mode=13;
				break;
			case 0x44://开启灯光
				AllLed=1;
				break;
			case 0x45://关闭灯光
				AllLed=0;
				break;
			case 0x46://开启呼吸灯
				BreatheLed=1;
				break;	
			case 0x47://关闭呼吸灯
				BreatheLed=0;
				break;	
			case 0x48://伸懒腰
				Face_Mode=6;
				Action_Mode=14;
				break;	
			case 0x49://拉伸
				Face_Mode=6;
				Action_Mode=15;
				break;	
			case 0x50://显示电量
				Battery_Bit^=1;
				break;	
		}
	}	
}
