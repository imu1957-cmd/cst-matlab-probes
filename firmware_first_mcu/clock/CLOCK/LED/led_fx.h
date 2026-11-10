#ifndef __LED_FX_H__
#define __LED_FX_H__

#include "sys.h"

/* ------------------------------------------------------------------ *
 * LedFx : status light effect engine for the 6 ws2812b RGB LEDs.
 *
 * mode 0 : standby    -> all white, slow breathing
 * mode 1 : charging   -> one green dot running along the strip
 * mode 2 : low bat    -> all red, breathing
 * mode 3 : off        -> all LEDs black
 *
 * The 800 kHz bit banging and the frame buffer are reused from the
 * existing driver  CLOCK/WS2812/ws2812b_init.c  (data pin = PA7).
 * Tick base is 100 ms, driven by the TIM3 interrupt.
 * ------------------------------------------------------------------ */

#define LED_FX_MODE_STANDBY    0u   /* idle       : white breathing       */
#define LED_FX_MODE_CHARGING   1u   /* charging   : green running water   */
#define LED_FX_MODE_LOWBAT     2u   /* low battery: red breathing         */
#define LED_FX_MODE_OFF        3u   /* switch all LEDs off                */

void LedFx_Init(void);        /* call once after My_Init()              */
void LedFx_Tick100ms(void);   /* call from TIM3_IRQHandler (100 ms)     */
void LedFx_SetMode(u8 mode);  /* switch effect, takes effect immediately*/
u8   LedFx_GetMode(void);     /* current effect mode                    */
u8   LedFx_IsActive(void);    /* 1 = LedFx owns the 6 LEDs (mode != 3)  */

#endif
