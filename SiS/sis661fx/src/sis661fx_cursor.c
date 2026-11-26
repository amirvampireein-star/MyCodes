#include "sis661fx.h"
#include <xf86Cursor.h>
#include <cursorstr.h>

/*
 * Hardware cursor for SiS 661FX (300/315/330/661 family).
 * Cursor registers: MMIO 0x8500 (addr), 0x8504 (fg), 0x8508 (bg).
 * Pattern: 64x64 ARGB-like 2bpp, 1KB at cursor base in framebuffer.
 */

#define SIS_CURSOR_SIZE 1024

static void sis661fx_cursor_setpos(ScrnInfoPtr pScrn, int x, int y)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    uint8_t xpre = 0, ypre = 0;
    if (x < 0) { xpre = (-x) & 0x3F; x = 0; }
    if (y < 0) { ypre = (-y) & 0x3F; y = 0; }
    sis661fx_mmio_write32(p, 0x8500, (p->cursorAddr >> 10) | ((uint32_t)xpre << 16) | ((uint32_t)ypre << 24));
    sis661fx_mmio_write32(p, 0x8504, ((uint32_t)y << 16) | (uint32_t)(x & 0xFFFF));
}

static void sis661fx_cursor_show(ScrnInfoPtr pScrn)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    /* Enable cursor - CR45 bit0 or MMIO 0x8500 bit? 661 uses MMIO 0x8500 enable */
    uint32_t v = sis661fx_mmio_read32(p, 0x8500);
    sis661fx_mmio_write32(p, 0x8500, v | 0x40000000);
    sis661fx_rmw_idx(p, SIS_VGA_CRTC_INDEX, 0x45, 0xFE, 0x01);
}

static void sis661fx_cursor_hide(ScrnInfoPtr pScrn)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    uint32_t v = sis661fx_mmio_read32(p, 0x8500);
    sis661fx_mmio_write32(p, 0x8500, v & ~0x40000000u);
    sis661fx_rmw_idx(p, SIS_VGA_CRTC_INDEX, 0x45, 0xFE, 0x00);
}

static void sis661fx_cursor_setcolors(ScrnInfoPtr pScrn, int bg, int fg)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    sis661fx_mmio_write32(p, 0x8504, (uint32_t)fg & 0xFFFFFF);
    sis661fx_mmio_write32(p, 0x8508, (uint32_t)bg & 0xFFFFFF);
    /* Reload position register with colors? 661 uses 0x8504/8508 for colors */
    sis661fx_mmio_write32(p, 0x850C, ((uint32_t)fg & 0xFFFFFF));
    sis661fx_mmio_write32(p, 0x8510, ((uint32_t)bg & 0xFFFFFF));
}

static void sis661fx_cursor_load(ScrnInfoPtr pScrn, unsigned char *src)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    uint8_t *dst = p->fbBase + p->cursorAddr;
    memcpy(dst, src, SIS_CURSOR_SIZE);
}

static Bool sis661fx_cursor_useargb(ScreenPtr pScreen, CursorPtr pCurs)
{
    (void)pScreen; (void)pCurs;
    return FALSE;
}

Bool sis661fx_cursor_init(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    xf86CursorInfoPtr info;

    info = xf86CreateCursorInfoRec();
    if (!info) return FALSE;

    p->cursorAddr = (p->videoRamKB * 1024) - 4096;
    info->MaxWidth = 64;
    info->MaxHeight = 64;
    info->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                  HARDWARE_CURSOR_INVERT_MASK |
                  HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
                  HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
                  HARDWARE_CURSOR_NIBBLE_SWAPPED;
    info->SetCursorColors = sis661fx_cursor_setcolors;
    info->SetCursorPosition = sis661fx_cursor_setpos;
    info->LoadCursorImage = sis661fx_cursor_load;
    info->HideCursor = sis661fx_cursor_hide;
    info->ShowCursor = sis661fx_cursor_show;
    info->UseHWCursorARGB = sis661fx_cursor_useargb;

    if (!xf86InitCursor(pScreen, info)) {
        xf86DestroyCursorInfoRec(info);
        return FALSE;
    }
    p->cursorInfo = info;
    return TRUE;
}
