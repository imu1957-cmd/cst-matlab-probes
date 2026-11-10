#include "stm32f10x.h"
#include "Delay.h"

/* =====================================================================
 *  Buzzer melody test  (PASSIVE buzzer / PWM on PA0 = TIM2_CH1)
 *
 *  PA0 -> buzzer(+)  [series 100 ohm recommended]
 *  GND -> buzzer(-)
 *
 *  NOTE: an ACTIVE buzzer module (3-pin MH-FMD low-level trigger)
 *        cannot play melody -- it beeps at a fixed tone only.
 *        If your module is active: the sweep test below will sound
 *        like ONE constant pitch. Then use a passive buzzer instead.
 * ===================================================================== */

/* ---------------- note frequency table (Hz) ---------------- */
#define N_0     0
#define N_C4    262
#define N_D4    294
#define N_E4    330
#define N_F4    349
#define N_G4    392
#define N_A4    440
#define N_B4    494
#define N_C5    523
#define N_D5    587
#define N_E5    659
#define N_F5    698
#define N_G5    784
#define N_A5    880

static void Buzzer_PwmInit(void)
{
    GPIO_InitTypeDef  gpio;
    TIM_TimeBaseInitTypeDef tb;
    TIM_OCInitTypeDef oc;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_0;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* TIM2 clock 72MHz, prescaler 72 -> 1MHz counter */
    tb.TIM_Prescaler         = 72 - 1;
    tb.TIM_CounterMode       = TIM_CounterMode_Up;
    tb.TIM_Period            = 1000 - 1;
    tb.TIM_ClockDivision     = TIM_CKD_DIV1;
    tb.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &tb);

    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse       = 500;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &oc);
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);

    TIM_Cmd(TIM2, ENABLE);
}

/* play one note; freq==0 means rest */
static void Buzzer_Note(uint16_t freq, uint16_t ms)
{
    if(freq == 0)
    {
        TIM_SetCompare1(TIM2, 0);          /* 0% duty -> silent */
    }
    else
    {
        uint16_t arr = (uint16_t)(1000000UL / freq);
        TIM_SetAutoreload(TIM2, arr - 1);
        TIM_SetCompare1(TIM2, arr / 2);    /* 50% duty */
    }
    Delay_ms(ms);
    TIM_SetCompare1(TIM2, 0);
    Delay_ms(20);                          /* small gap between notes */
}

typedef struct
{
    uint16_t freq;
    uint16_t ms;
} Note_t;

/* ---------- 1) sweep test : check if buzzer is passive ---------- */
static void Buzzer_Sweep(void)
{
    uint16_t f;
    for(f = 262; f <= 1046; f += 131)      /* C4..C6 rising */
    {
        Buzzer_Note(f, 350);
    }
    Delay_ms(300);
}

/* ---------- 2) little star (C major, 500ms / beat) ---------- */
static const Note_t LittleStar[] =
{
    {N_C5,400},{N_C5,400},{N_G5,400},{N_G5,400},{N_A5,400},{N_A5,400},{N_G5,800},
    {N_F5,400},{N_F5,400},{N_E5,400},{N_E5,400},{N_D5,400},{N_D5,400},{N_C5,800},
    {0,300}
};

/* ---------- 3) two tigers ---------- */
static const Note_t TwoTigers[] =
{
    {N_C5,400},{N_D5,400},{N_E5,400},{N_C5,400},
    {N_C5,400},{N_D5,400},{N_E5,400},{N_C5,400},
    {N_E5,400},{N_F5,400},{N_G5,800},
    {N_E5,400},{N_F5,400},{N_G5,800},
    {0,400}
};

static void Buzzer_PlayList(const Note_t *list, uint16_t n)
{
    uint16_t i;
    for(i = 0; i < n; i++)
    {
        Buzzer_Note(list[i].freq, list[i].ms);
    }
    Delay_ms(600);
}

int main(void)
{
    Buzzer_PwmInit();

    /* sweep once, so you can tell passive from active */
    Buzzer_Sweep();

    while(1)
    {
        Buzzer_PlayList(LittleStar, sizeof(LittleStar)/sizeof(Note_t));
        Buzzer_PlayList(TwoTigers,  sizeof(TwoTigers)/sizeof(Note_t));
    }
}