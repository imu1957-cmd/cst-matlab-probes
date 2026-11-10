#ifndef __SCREEN_CFG_H_
#define __SCREEN_CFG_H_

#include "stm32f10x.h"

/* Persistent UI settings (stored in the last 1KB page of internal flash).
 * Layout is fixed; do not reorder fields without bumping CFG_MAGIC. */
typedef struct
{
    uint8_t magic;      /* 0xA5 when valid */
    uint8_t low_bat;    /* low battery threshold, 30..60 (%) */
    uint8_t backlight;  /* LCD backlight, 10..100 (%) */
    int8_t  temp_ofs;   /* temperature offset, -20..+20 (degC) */
    uint8_t rsv;        /* padding: keep sizeof(Cfg_t) even for halfword writes */
    uint8_t chk;        /* check byte */
} Cfg_t;

extern Cfg_t g_cfg;

void Cfg_Load(void);
void Cfg_Save(void);

#endif
