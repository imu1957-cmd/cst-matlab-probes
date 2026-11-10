#ifndef __LCD_H
#define __LCD_H		
#include "sys.h"


void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color);//指定区域填充颜色
void LCD_DrawPoint(u16 x,u16 y,u16 color);//在指定位置画一个点
void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2,u16 color);//在指定位置画一条线
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2,u16 color);//在指定位置画一个矩形
void Draw_Circle(u16 x0,u16 y0,u8 r,u16 color);//在指定位置画一个圆

void LCD_ShowPicture(u16 x,u16 y,u16 length,u16 width);//显示图片

void LCD_ShowChinese40x40(u16 x,u16 y,u8 *s,u16 fc,u16 bc);//显示单个40x40汉字
void LCD_ShowChinese32x32(u16 x,u16 y,u8 *s,u16 fc,u16 bc);//显示单个32x32汉字
void LCD_ShowChinese24x24(u16 x,u16 y,u8 *s,u16 fc,u16 bc);//显示单个24x24汉字
void LCD_Num2412(u16 x,u16 y,u8 num,u16 fc,u16 bc);
void LCD_Num3618(u16 x,u16 y,u8 num,u16 fc,u16 bc);
void LCD_Num4020(u16 x,u16 y,u8 num,u16 fc,u16 bc);
void LCD_Num4530(u16 x,u16 y,u8 num,u16 fc,u16 bc);
void LCD_Num5025(u16 x,u16 y,u8 num,u16 fc,u16 bc);
void LCD_Num6040(u16 x,u16 y,u8 num,u16 fc,u16 bc);
void LCD_Pic3636(u16 x,u16 y,u8 pic,u16 fc,u16 bc);
void LCD_Pic5050(u16 x,u16 y,u8 pic,u16 fc,u16 bc);
void LCD_Pic9696(u16 x,u16 y,u8 pic,u16 fc,u16 bc);


void LCD_colon(u16 x,u16 y);//4*24冒号
void LCD_underline(u16 x,u16 y,u16 color);//3*8下划线

u32 mypow(u8 m,u8 n);//求幂
void LCD_strip80(u16 x,u16 y,u8 num, u16 frame, u16 inside);
void LCD_strip100(u16 x,u16 y,u8 num, u16 frame, u16 inside);


//画笔颜色


#define WHITE         	 0xFFFF //白色
#define BLACK         	 0x0000 //黑色
#define BLUE           	 0x001F //蓝色  
#define RED           	 0xF800 //红色
#define GREEN         	 0x07E0 //绿色
#define CYAN          	 0x7FFF //青色
#define YELLOW        	 0xFFE0 //黄色
#define ORANGE					 0xFD20 //橙色
#define PURPLE					 0x8010 //紫色
#define BRRED 			     0XFC07 //棕红色
#define DARKBLUE      	 0X01CF	//深蓝色
#define LIGHTBLUE      	 0X7D7C	//浅蓝色  

//#define BRED             0XF81F //紫红色
//#define GRED 			       0XFFE0 //纯黄
//#define GBLUE			       0X07FF //吉蓝
//#define MAGENTA       	 0xF81F //洋红
//#define BROWN 			     0XBC40 //棕色
//#define GRAY  			     0X8430 //灰色
//#define GRAYBLUE       	 0X5458 //灰蓝色
//#define LIGHTGREEN     	 0X841F //浅绿色
//#define LGRAY 			     0XC618 //浅灰色(PANNEL),窗体背景色
//#define LGRAYBLUE        0XA651 //浅灰蓝色(中间层颜色)
//#define LBBLUE           0X2B12 //浅棕蓝色(选择条目的反色)

#endif
