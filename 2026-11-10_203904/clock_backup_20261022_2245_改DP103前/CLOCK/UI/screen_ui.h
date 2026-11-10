#ifndef __SCREEN_UI_H_
#define __SCREEN_UI_H_

#include "stm32f10x.h"

/* ======================================================================
 *  5 寸串口屏（陶晶驰 TJC8048X550_011）UI 模块
 *
 *  链路：USART1 (PA9=TX, PA10=RX, 115200, 8N1)
 *  指令：字符串 + 0xFF 0xFF 0xFF 结尾
 *  触摸：0x65 page ctrl event 0xFF 0xFF 0xFF
 *
 *  4 页：
 *    page0 主页   t0 时间  t1 日期  t2 星期  t3 天气
 *                 n0 温度  n1 湿度  t4 电量文本  j0 电量条  t5 状态
 *    page1 备忘录 bt0..bt7 八条  t6 正文  x0 返回
 *    page2 设置   n2 低电阈值  n3 亮度  n4 温度偏移
 *    page3 日志   t7 多行文本
 * ====================================================================== */

void Screen_Init(void);
void Screen_RxByte(uint8_t b);        /* USART1 中断里喂进来 */
void Screen_Poll(void);               /* 主循环：处理触摸事件 */
void Screen_Tick100ms(void);          /* 100ms 节拍：刷新主页（只在变化时发） */

/* 数据更新（control.c / 其他模块调用） */
void Screen_SetEnv(int16_t temp_x10, uint16_t hum);        /* SHT30 */
void Screen_SetWeather(int16_t temp, const char *cond);    /* 涂鸦天气 */
void Screen_SetBattery(uint8_t pct, uint8_t charging);     /* 宠物电量 */
void Screen_SetStatus(const char *text);                   /* 状态行 */
void Screen_SetPetOnline(uint8_t online);
void Screen_ShowPage(uint8_t page);
void Screen_ClearMemo(void);
void Screen_AddMemo(uint8_t idx, const char *text);        /* idx 0..7 */

/* 触摸回调：page1 的 bt0..bt7 默认会通过 ClockLink 下发备忘录播报 */
void Screen_OnTouch(uint8_t page, uint8_t ctrl, uint8_t event);

#endif
