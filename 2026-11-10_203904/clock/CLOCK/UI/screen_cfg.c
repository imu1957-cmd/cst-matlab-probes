#include "screen_cfg.h"
#include "stm32f10x_flash.h"
#include <string.h>

/* Last 1KB page of a 64KB device (F103C8). Also valid on 128KB (F103CB),
 * where it is just an ordinary page inside the array -> safe on both chips. */
#define CFG_FLASH_ADDR   0x0800FC00u
#define CFG_MAGIC        0xA5u

Cfg_t g_cfg;

static uint8_t Cfg_Chk(const Cfg_t *c)
{
    return (uint8_t)(c->magic ^ c->low_bat ^ c->backlight
                     ^ (uint8_t)c->temp_ofs ^ 0x5Au);
}

void Cfg_Load(void)
{
    const Cfg_t *p = (const Cfg_t *)CFG_FLASH_ADDR;

    if (p->magic == CFG_MAGIC && Cfg_Chk(p) == p->chk)
    {
        memcpy(&g_cfg, p, sizeof(Cfg_t));
    }
    else
    {
        g_cfg.magic     = CFG_MAGIC;
        g_cfg.low_bat   = 45;
        g_cfg.backlight = 80;
        g_cfg.temp_ofs  = 0;
        g_cfg.chk       = Cfg_Chk(&g_cfg);
    }

    if (g_cfg.low_bat < 30)  g_cfg.low_bat = 30;
    if (g_cfg.low_bat > 60)  g_cfg.low_bat = 60;
    if (g_cfg.backlight < 10) g_cfg.backlight = 10;
    if (g_cfg.backlight > 100) g_cfg.backlight = 100;
    if (g_cfg.temp_ofs < -20) g_cfg.temp_ofs = -20;
    if (g_cfg.temp_ofs > 20)  g_cfg.temp_ofs = 20;
}

void Cfg_Save(void)
{
    uint16_t *src = (uint16_t *)&g_cfg;
    uint32_t addr = CFG_FLASH_ADDR;
    uint8_t i, n = (uint8_t)(sizeof(Cfg_t) / 2);

    g_cfg.magic = CFG_MAGIC;
    g_cfg.chk   = Cfg_Chk(&g_cfg);

    FLASH_Unlock();
    FLASH_ErasePage(CFG_FLASH_ADDR);
    for (i = 0; i < n; i++)
    {
        FLASH_ProgramHalfWord(addr, src[i]);
        addr += 2;
    }
    FLASH_Lock();
}
