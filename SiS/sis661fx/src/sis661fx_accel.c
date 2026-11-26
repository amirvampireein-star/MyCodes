#include "sis661fx.h"

/*
 * Minimal 2D engine bring-up using SiS 661FX MMIO registers (BAR1).
 * Offsets recovered from sis310_accel.h / Windows driver MMIO usage.
 *
 * This file initializes the engine and provides direct MMIO primitives.
 * Full EXA acceleration can be layered on top; the base driver remains
 * correct and stable with software rendering.
 */

static void sis661fx_engine_wait(Sis661fxPtr p)
{
    int i;
    for (i = 0; i < 1000000; i++) {
        uint32_t s = sis661fx_mmio_read32(p, SIS_MMIO_Q_STATUS);
        if (s & 0x80000000) return;
    }
    for (i = 0; i < 1000000; i++) {
        uint32_t t = sis661fx_mmio_read32(p, SIS_MMIO_FIRE_TRIGGER);
        if (!(t & (1u << 10))) return;
    }
}

Bool sis661fx_accel_init(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);

    if (p->noAccel) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "2D acceleration disabled\n");
        return TRUE;
    }

    sis661fx_mmio_write32(p, SIS_MMIO_Q_BASE, 0);
    sis661fx_mmio_write32(p, SIS_MMIO_Q_WRITE, 0);
    sis661fx_mmio_write32(p, SIS_MMIO_Q_READ, 0);
    sis661fx_mmio_write32(p, SIS_MMIO_Q_STATUS, 0x80000000);
    sis661fx_engine_wait(p);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "SiS661FX: 2D engine MMIO initialized (BAR1 + 0x8200)\n");
    return TRUE;
}
