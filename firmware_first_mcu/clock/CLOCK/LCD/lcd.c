#include "lcd.h"
#include "lcd_init.h"
#include "lcdfont.h"
#include "delay.h"
#include "pic.h"


/******************************************************************************
      函数说明：在指定区域填充颜色
      入口数据：xsta,ysta   起始坐标
                xend,yend   终止坐标
								color       要填充的颜色
      返回值：  无
******************************************************************************/
void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color)
{          
	u16 i,j; 
	LCD_Address_Set(xsta,ysta,xend-1,yend-1);//设置显示范围
	for(i=ysta;i<yend;i++)
	{													   	 	
		for(j=xsta;j<xend;j++)
		{
			LCD_WR_DATA(color);
		}
	} 					  	    
}

/******************************************************************************
      函数说明：在指定位置画点
      入口数据：x,y 画点坐标
                color 点的颜色
      返回值：  无
******************************************************************************/
void LCD_DrawPoint(u16 x,u16 y,u16 color)
{
	LCD_Address_Set(x,y,x,y);//设置光标位置 
	LCD_WR_DATA(color);
} 


/******************************************************************************
      函数说明：画线
      入口数据：x1,y1   起始坐标
                x2,y2   终止坐标
                color   线的颜色
      返回值：  无
******************************************************************************/
void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2,u16 color)
{
	u16 t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	delta_x=x2-x1; //计算坐标增量 
	delta_y=y2-y1;
	uRow=x1;//画线起点坐标
	uCol=y1;
	if(delta_x>0)incx=1; //设置单步方向 
	else if (delta_x==0)incx=0;//垂直线 
	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if (delta_y==0)incy=0;//水平线 
	else {incy=-1;delta_y=-delta_y;}
	if(delta_x>delta_y)distance=delta_x; //选取基本增量坐标轴 
	else distance=delta_y;
	for(t=0;t<distance+1;t++)
	{
		LCD_DrawPoint(uRow,uCol,color);//画点
		xerr+=delta_x;
		yerr+=delta_y;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}
}


/******************************************************************************
      函数说明：画矩形
      入口数据：x1,y1   起始坐标
                x2,y2   终止坐标
                color   矩形的颜色
      返回值：  无
******************************************************************************/
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2,u16 color)
{
	LCD_DrawLine(x1,y1,x2,y1,color);
	LCD_DrawLine(x1,y1,x1,y2,color);
	LCD_DrawLine(x1,y2,x2,y2,color);
	LCD_DrawLine(x2,y1,x2,y2,color);
}


/******************************************************************************
      函数说明：画圆
      入口数据：x0,y0   圆心坐标
                r       半径
                color   圆的颜色
      返回值：  无
******************************************************************************/
void Draw_Circle(u16 x0,u16 y0,u8 r,u16 color)
{
	int a,b;
	a=0;b=r;	  
	while(a<=b)
	{
          
		LCD_DrawPoint(x0+b,y0-a,color);             //1           
		LCD_DrawPoint(x0+a,y0-b,color);             //2            
		LCD_DrawPoint(x0-a,y0-b,color);             //3  		
		LCD_DrawPoint(x0-b,y0-a,color);             //4
		LCD_DrawPoint(x0-b,y0+a,color);             //5	
		LCD_DrawPoint(x0-a,y0+b,color);             //6    		
		LCD_DrawPoint(x0+a,y0+b,color);             //7 
		LCD_DrawPoint(x0+b,y0+a,color);             //8 
		a++;
		if((a*a+b*b)>(r*r))//判断要画的点是否过远
		{
			b--;
		}
	}
}
/******************************************************************************
      函数说明：显示单个40x40汉字
      入口数据：x,y显示坐标
                *s 要显示的汉字
                fc 字的颜色
                bc 字的背景色
      返回值：  无
******************************************************************************/
void LCD_ShowChinese40x40(u16 x,u16 y,u8 *s,u16 fc,u16 bc)
{
	u8 i,j,m=0;
	u16 k;
	u16 HZnum;//汉字数目
	HZnum=sizeof(tfont40)/sizeof(typFNT_GB40);	//统计汉字数目
	for(k=0;k<HZnum;k++) 
	{
		if ((tfont40[k].Index[0]==*(s))&&(tfont40[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+39,y+39);
			for(i=0;i<200;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(tfont40[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
					else LCD_WR_DATA(bc);
					m++;
					if(m%40==0)
					{
						m=0;
						break;
					}
				}
			}
		}				  	
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}

void LCD_ShowChinese32x32(u16 x,u16 y,u8 *s,u16 fc,u16 bc)
{
	u8 i,j,m=0;
	u16 k;
	u16 HZnum;//汉字数目
	HZnum=sizeof(tfont32)/sizeof(typFNT_GB32);	//统计汉字数目
	for(k=0;k<HZnum;k++) 
	{
		if ((tfont32[k].Index[0]==*(s))&&(tfont32[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+31,y+31);
			for(i=0;i<128;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(tfont32[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
					else LCD_WR_DATA(bc);
					m++;
					if(m%32==0)
					{
						m=0;
						break;
					}
				}
			}
		}				  	
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}


void LCD_ShowChinese24x24(u16 x,u16 y,u8 *s,u16 fc,u16 bc)
{
	u8 i,j,m=0;
	u16 k;
	u16 HZnum;//汉字数目
	HZnum=sizeof(tfont24)/sizeof(typFNT_GB24);	//统计汉字数目
	for(k=0;k<HZnum;k++) 
	{
		if ((tfont24[k].Index[0]==*(s))&&(tfont24[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+23,y+23);
			for(i=0;i<72;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(tfont24[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
					else LCD_WR_DATA(bc);
					m++;
					if(m%24==0)
					{
						m=0;
						break;
					}
				}
			}
		}				  	
		continue;  //查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
	}
}


/******************************************************************************
      函数说明：显示单个字符
      入口数据：x,y显示坐标
                num 要显示的字符
                fc 字的颜色
                bc 字的背景色
                sizey 字号
                mode:  0非叠加模式  1叠加模式
      返回值：  无
******************************************************************************/
void LCD_ShowChar(u16 x,u16 y,u8 num,u16 fc,u16 bc,u8 sizey)
{
	u8 temp,sizex,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	sizex=sizey/2;
	TypefaceNum=(sizex/8+((sizex%8)?1:0))*sizey;
	num=num-' ';    //得到偏移后的值
	LCD_Address_Set(x,y,x+sizex-1,y+sizey-1);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
//		if(sizey==12)temp=ascii_1206[num][i];		       //调用6x12字体
//		else if(sizey==16)temp=ascii_1608[num][i];		 //调用8x16字体
//		else if(sizey==24)temp=ascii_2412[num][i];		 //调用12x24字体
//		else if(sizey==32)temp=ascii_3216[num][i];		 //调用16x32字体
//		else return;
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(fc);
			else LCD_WR_DATA(bc);
			m++;
			if(m%sizex==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}
void LCD_Num3618(u16 x,u16 y,u8 num,u16 fc,u16 bc)
{
	u8 temp,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	TypefaceNum=108;
	LCD_Address_Set(x,y,x+17,y+35);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		temp=num_3618[num][i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(fc);
			else LCD_WR_DATA(bc);
			m++;
			if(m%18==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}

void LCD_Num2412(u16 x,u16 y,u8 num,u16 fc,u16 bc)
{
	u8 temp,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	TypefaceNum=48;
	LCD_Address_Set(x,y,x+11,y+23);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		temp=num_2412[num][i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(fc);
			else LCD_WR_DATA(bc);
			m++;
			if(m%12==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}

void LCD_Num4020(u16 x,u16 y,u8 num,u16 fc,u16 bc)
{
	u8 temp,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	TypefaceNum=120;
	LCD_Address_Set(x,y,x+19,y+39);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		temp=num_4020[num][i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(fc);
			else LCD_WR_DATA(bc);
			m++;
			if(m%20==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}

void LCD_Num5025(u16 x,u16 y,u8 num,u16 fc,u16 bc)
{
	u8 temp,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	TypefaceNum=200;
	LCD_Address_Set(x,y,x+24,y+49);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		temp=num_5025[num][i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(fc);
			else LCD_WR_DATA(bc);
			m++;
			if(m%25==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}

void LCD_Num4530(u16 x,u16 y,u8 num,u16 fc,u16 bc)
{
	u8 temp,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	TypefaceNum=180;
	LCD_Address_Set(x,y,x+29,y+44);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		temp=num_4530[num][i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(bc);//由于取模问题 弄反了 这里背景色和字色对调了
			else LCD_WR_DATA(fc);
			m++;
			if(m%30==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}

void LCD_Num6040(u16 x,u16 y,u8 num,u16 fc,u16 bc)
{
	u8 temp,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	TypefaceNum=300;
	LCD_Address_Set(x,y,x+39,y+59);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		temp=num_6040[num][i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(bc);//由于取模问题 弄反了 这里背景色和字色对调了
			else LCD_WR_DATA(fc);
			m++;
			if(m%40==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}
void LCD_Pic3636(u16 x,u16 y,u8 pic,u16 fc,u16 bc)
{
	u8 temp,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	TypefaceNum=180;
	LCD_Address_Set(x,y,x+35,y+35);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		temp=pic_3636[pic][i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(fc);//由于取模问题 弄反了 这里背景色和字色对调了
			else LCD_WR_DATA(bc);
			m++;
			if(m%36==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}
void LCD_Pic5050(u16 x,u16 y,u8 pic,u16 fc,u16 bc)
{
	u8 temp,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	TypefaceNum=350;
	LCD_Address_Set(x,y,x+49,y+49);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		temp=pic_5050[pic][i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(fc);//由于取模问题 弄反了 这里背景色和字色对调了
			else LCD_WR_DATA(bc);
			m++;
			if(m%50==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}
void LCD_Pic9696(u16 x,u16 y,u8 pic,u16 fc,u16 bc)
{
	u8 temp,t,m=0;
	u16 i,TypefaceNum;//一个字符所占字节大小
	TypefaceNum=1152;
	LCD_Address_Set(x,y,x+95,y+95);  //设置光标位置 
	for(i=0;i<TypefaceNum;i++)
	{ 
		temp=pic_9696[pic][i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(fc);//由于取模问题 弄反了 这里背景色和字色对调了
			else LCD_WR_DATA(bc);
			m++;
			if(m%96==0)
			{
				m=0;
				break;
			}
		}
	}   	 	  
}


/******************************************************************************
      函数说明：幂函数
      入口数据：m底数，n指数
      返回值：  无
******************************************************************************/
u32 mypow(u8 m,u8 n)
{
	u32 result=1;	 
	while(n--)result*=m;
	return result;
}

/******************************************************************************
      函数说明：显示图片
      入口数据：x,y起点坐标
                length 图片长度
                width  图片宽度
                pic[]  图片数组    
      返回值：  无
******************************************************************************/
void LCD_ShowPicture(u16 x,u16 y,u16 length,u16 width)
{
	u16 i,j;
	u32 k=0;
	LCD_Address_Set(x,y,x+length-1,y+width-1);
	for(i=0;i<length;i++)
	{
		for(j=0;j<width;j++)
		{
			LCD_WR_DATA8(gImage_1[k*2]);
			LCD_WR_DATA8(gImage_1[k*2+1]);
			k++;
		}
	}			
}

const u8 Semicircle_lift[17]={0xE0,0x10,0x08,0x04,0x02,0x02,0x02,0x01,0x01,0x01,0x02,0x02,0x02,0x04,0x08,0x10,0xE0};
const u8 Semicircle_right[17]={0x07,0x08,0x10,0x20,0x40,0x40,0x40,0x80,0x80,0x80,0x40,0x40,0x40,0x20,0x10,0x08,0x07};
void LCD_strip80(u16 x,u16 y,u8 num, u16 frame, u16 inside)
{
	u8 temp,i,t,m=0;

	//左边半圆
	LCD_Address_Set(x,y-8,x+7,y+8);  //设置光标位置 
	for(i=0;i<17;i++)
	{ 
		temp=Semicircle_lift[i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(frame);
			else LCD_WR_DATA(BLACK);
			m++;
			if(m%8==0)
			{
				m=0;
				break;
			}
		}
	} 
	//左边半圆
	LCD_Address_Set(x+74,y-8,x+81,y+8);  //设置光标位置 
	for(i=0;i<17;i++)
	{ 
		temp=Semicircle_right[i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(frame);
			else LCD_WR_DATA(BLACK);
			m++;
			if(m%8==0)
			{
				m=0;
				break;
			}
		}
	} 
	//边框线
	LCD_DrawLine(x+8,y-8,x+74,y-8,frame);
	LCD_DrawLine(x+8,y+8,x+74,y+8,frame);

	
	if(num>=0)	LCD_DrawLine(x+1,y-1,x+1,y+2,inside);
	else LCD_DrawLine(x+1,y-1,x+1,y+2,BLACK);
	if(num>=1)	LCD_DrawLine(x+2,y-4,x+2,y+5,inside);
	else LCD_DrawLine(x+2,y-4,x+2,y+5,BLACK);
	if(num>=2)	LCD_DrawLine(x+3,y-5,x+3,y+6,inside);
	else LCD_DrawLine(x+3,y-5,x+3,y+6,BLACK);
	if(num>=3)	LCD_DrawLine(x+4,y-6,x+4,y+7,inside);
	else LCD_DrawLine(x+4,y-6,x+4,y+7,BLACK);
	
	if(num>=4 && num<76)
		LCD_Fill(x+5,y-7,x+num,y+8,inside);
	else if(num>=76)
		LCD_Fill(x+5,y-7,x+77,y+8,inside);
	if(num<76 && num>=4)
		LCD_Fill(x+num+1,y-7,x+77,y+8,BLACK);
	else if(num<=4)
		LCD_Fill(x+5,y-7,x+77,y+8,BLACK);
	
	
	if(num>=77)	LCD_DrawLine(x+77,y-6,x+77,y+7,inside);
	else LCD_DrawLine(x+77,y-6,x+77,y+7,BLACK);
	if(num>=78)	LCD_DrawLine(x+78,y-5,x+78,y+6,inside);
	else LCD_DrawLine(x+78,y-5,x+78,y+6,BLACK);
	if(num>=79)	LCD_DrawLine(x+79,y-4,x+79,y+5,inside);
	else LCD_DrawLine(x+79,y-4,x+79,y+5,BLACK);
	if(num>=80)	LCD_DrawLine(x+80,y-1,x+80,y+2,inside);
	else LCD_DrawLine(x+80,y-1,x+80,y+2,BLACK);
}


void LCD_strip100(u16 x,u16 y,u8 num, u16 frame, u16 inside)
{
	u8 temp,i,t,m=0;

	//左边半圆
	LCD_Address_Set(x,y-8,x+7,y+8);  //设置光标位置 
	for(i=0;i<17;i++)
	{ 
		temp=Semicircle_lift[i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(frame);
			else LCD_WR_DATA(BLACK);
			m++;
			if(m%8==0)
			{
				m=0;
				break;
			}
		}
	} 
	//左边半圆
	LCD_Address_Set(x+94,y-8,x+101,y+8);  //设置光标位置 
	for(i=0;i<17;i++)
	{ 
		temp=Semicircle_right[i];
		for(t=0;t<8;t++)
		{
			if(temp&(0x01<<t))LCD_WR_DATA(frame);
			else LCD_WR_DATA(BLACK);
			m++;
			if(m%8==0)
			{
				m=0;
				break;
			}
		}
	} 
	//边框线
	LCD_DrawLine(x+8,y-8,x+94,y-8,frame);
	LCD_DrawLine(x+8,y+8,x+94,y+8,frame);

	
	if(num>=0)	LCD_DrawLine(x+1,y-1,x+1,y+2,inside);
	else LCD_DrawLine(x+1,y-1,x+1,y+2,BLACK);
	if(num>=1)	LCD_DrawLine(x+2,y-4,x+2,y+5,inside);
	else LCD_DrawLine(x+2,y-4,x+2,y+5,BLACK);
	if(num>=2)	LCD_DrawLine(x+3,y-5,x+3,y+6,inside);
	else LCD_DrawLine(x+3,y-5,x+3,y+6,BLACK);
	if(num>=3)	LCD_DrawLine(x+4,y-6,x+4,y+7,inside);
	else LCD_DrawLine(x+4,y-6,x+4,y+7,BLACK);
	
	if(num>=4 && num<96)
		LCD_Fill(x+5,y-7,x+num,y+8,inside);
	else if(num>=96)
		LCD_Fill(x+5,y-7,x+97,y+8,inside);
	if(num<96 && num>=4)
		LCD_Fill(x+num+1,y-7,x+97,y+8,BLACK);
	else if(num<=4)
		LCD_Fill(x+5,y-7,x+97,y+8,BLACK);
	
	
	if(num>=77)	LCD_DrawLine(x+77,y-6,x+77,y+7,inside);
	else LCD_DrawLine(x+77,y-6,x+77,y+7,BLACK);
	if(num>=98)	LCD_DrawLine(x+98,y-5,x+98,y+6,inside);
	else LCD_DrawLine(x+98,y-5,x+98,y+6,BLACK);
	if(num>=99)	LCD_DrawLine(x+99,y-4,x+99,y+5,inside);
	else LCD_DrawLine(x+99,y-4,x+99,y+5,BLACK);
	if(num>=100)	LCD_DrawLine(x+100,y-1,x+100,y+2,inside);
	else LCD_DrawLine(x+100,y-1,x+100,y+2,BLACK);
}


void LCD_colon(u16 x,u16 y)
{
	LCD_Fill(x+1,y+5,x+5,y+9,WHITE);
	LCD_Fill(x+1,y+17,x+5,y+21,WHITE);
}

void LCD_underline(u16 x,u16 y,u16 color)
{
	LCD_Fill(x,y,x+12,y+3,color);

}
