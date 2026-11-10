#include "led_fx.h"
#include "ws2812b_init.h"

/* ------------------------------------------------------------------ *
 * LedFx : status light effects for the 6 ws2812b RGB LEDs of the clock.
 *
 *   mode 0 standby    : all white, slow breathing
 *   mode 1 charging   : one green dot running along the strip
 *   mode 2 low battery: all red, breathing
 *   mode 3 off        : all black
 *
 * Everything runs on the 100 ms TIM3 tick, so one animation step is
 * 100 ms.  The frame buffer REG_LED_cooking[] and the 800 kHz bit
 * banging functions come from CLOCK/WS2812/ws2812b_init.c, they are
 * not duplicated here.
 *
 * Note for the application layer: while LedFx is active (mode != 3)
 * the old atmosphere effect Atmosphere_Scan() must not refresh the
 * same LEDs, otherwise both writers fight over the frame buffer.
 * ------------------------------------------------------------------ */

#define LED_FX_LED_NUM        6u    /* number of ws2812b on the strip    */

#define LED_FX_BREATH_HALF    40u   /* standby : 4.0 s up + 4.0 s down   */
#define LED_FX_LOWBAT_HALF    30u   /* low bat : 3.0 s up + 3.0 s down   */

#define LED_FX_STANDBY_PEAK   80u   /* peak brightness, white breathing  */
#define LED_FX_LOWBAT_PEAK    100u  /* peak brightness, red breathing    */

#define LED_FX_FLOW_STEP      2u    /* green dot moves 1 LED every 200 ms*/
#define LED_FX_FLOW_HEAD      255u  /* brightness of the moving dot      */
#define LED_FX_FLOW_TAIL1     72u   /* first  fading pixel behind it     */
#define LED_FX_FLOW_TAIL2     18u   /* second fading pixel behind it     */

/* 240 is the least common multiple of 2*40, 2*30 and 6*2, so every
 * animation loops seamlessly and the tick counter never overflows.  */
#define LED_FX_PHASE_MOD      240u

static u8  s_mode  = LED_FX_MODE_STANDBY;  /* current effect mode         */
static u8  s_ready = 0;                    /* 1 after LedFx_Init()        */
static u16 s_phase = 0;                    /* animation counter, 100 ms   */

/* Write one colour into the frame buffer, without latching it out. */
static void LedFx_Fill(u8 r, u8 g, u8 b)
{
    u8 i;

    for (i = 0; i < LED_FX_LED_NUM; i++)
    {
        REG_LED_Write_cooking(i, r, g, b);
    }
}

/* Triangle 0..255..0 over 2*half ticks, shaped with a square law so
 * the fade in / fade out looks like a real breath instead of a ramp.  */
static u8 LedFx_BreathLevel(u16 phase, u16 half)
{
    u16 t;
    u16 up;
    u16 lin;

    t = phase % (2u * half);
    if (t < half)
    {
        up = t;
    }
    else
    {
        up = (2u * half) - t;
    }

    lin = (up * 255u) / half;                 /* 0 .. 255 linear */
    return (u8)(((u32)lin * lin) / 255u);     /* gamma 2.0       */
}

/* Scale a 0..255 level with a 0..255 peak value. */
static u8 LedFx_Scale(u8 level, u8 peak)
{
    return (u8)(((u16)level * peak) / 255u);
}

/* Build the frame for the current mode and latch it to the LEDs. */
static void LedFx_Render(void)
{
    u8 i;
    u8 d;
    u8 lvl;
    u8 pos;

    switch (s_mode)
    {
        case LED_FX_MODE_STANDBY:
            lvl = LedFx_BreathLevel(s_phase, LED_FX_BREATH_HALF);
            lvl = LedFx_Scale(lvl, LED_FX_STANDBY_PEAK);
            LedFx_Fill(lvl, lvl, lvl);            /* white */
            REG_LED_Refresh();
            break;

        case LED_FX_MODE_CHARGING:
            /* head runs 0 -> 5 -> 0, with two dimmer pixels behind it */
            pos = (u8)((s_phase / LED_FX_FLOW_STEP) % LED_FX_LED_NUM);
            for (i = 0; i < LED_FX_LED_NUM; i++)
            {
                d = (u8)((i + LED_FX_LED_NUM - pos) % LED_FX_LED_NUM);
                if (d == 0u)
                {
                    REG_LED_Write_cooking(i, 0u, LED_FX_FLOW_HEAD,  0u);
                }
                else if (d == 1u)
                {
                    REG_LED_Write_cooking(i, 0u, LED_FX_FLOW_TAIL1, 0u);
                }
                else if (d == 2u)
                {
                    REG_LED_Write_cooking(i, 0u, LED_FX_FLOW_TAIL2, 0u);
                }
                else
                {
                    REG_LED_Write_cooking(i, 0u, 0u, 0u);
                }
            }
            REG_LED_Refresh();
            break;

        case LED_FX_MODE_LOWBAT:
            lvl = LedFx_BreathLevel(s_phase, LED_FX_LOWBAT_HALF);
            lvl = LedFx_Scale(lvl, LED_FX_LOWBAT_PEAK);
            LedFx_Fill(lvl, 0u, 0u);              /* red */
            REG_LED_Refresh();
            break;

        case LED_FX_MODE_OFF:
        default:
            LedFx_Fill(0u, 0u, 0u);
            REG_LED_Refresh();
            break;
    }
}

/* Initialise the LED engine.  Call once after My_Init(), because the
 * driver needs delay_init() (delay_us) and the GPIO clocks.          */
void LedFx_Init(void)
{
    REG_LED_Init();               /* PA7 push-pull output + reset pulse */

    s_mode  = LED_FX_MODE_STANDBY;
    s_phase = 0u;
    s_ready = 1u;

    LedFx_Fill(0u, 0u, 0u);       /* start from black */
    REG_LED_Refresh();
}

/* 100 ms animation step, called from the TIM3 interrupt. */
void LedFx_Tick100ms(void)
{
    if (s_ready == 0u)
    {
        return;
    }

    if (s_mode == LED_FX_MODE_OFF)
    {
        return;                   /* nothing to animate while dark */
    }

    s_phase = (u16)((s_phase + 1u) % LED_FX_PHASE_MOD);
    LedFx_Render();
}

/* Switch to another effect.  The new frame is pushed out immediately
 * instead of waiting for the next 100 ms tick.                        */
void LedFx_SetMode(u8 mode)
{
    if (mode > LED_FX_MODE_OFF)
    {
        mode = LED_FX_MODE_OFF;
    }

    s_mode  = mode;
    s_phase = 0u;

    if (s_ready != 0u)
    {
        LedFx_Render();
    }
}

u8 LedFx_GetMode(void)
{
    return s_mode;
}

u8 LedFx_IsActive(void)
{
    return (u8)((s_ready != 0u) && (s_mode != LED_FX_MODE_OFF));
}
