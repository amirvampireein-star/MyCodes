/*
 * SiS 661FX mode setting - direct VGA/SR/CR/MMIO programming.
 *
 * Based on static reverse engineering of the vendor Windows miniport
 * (sisgrp.sys, UniVGA3 3.93) and SiS 661FX register semantics.
 *
 * Key RE facts (see Linux/docs/REVERSE_ENGINEERING.md):
 *  - PCI 1039:6330, family discriminated by CR5C[7:3]: 0x10=661, 0x80=760, 0x90=741
 *  - Indexed register helpers: bbe(port,idx)=read, bda(port,idx,mask,val)=rmw
 *  - MMIO 2D engine at BAR1: 0x8200..0x85CC
 *  - Engine idle wait: MMIO 0x85CC bit 31 (command queue)
 */

#include "sis661fx.h"
#include <string.h>
#include <unistd.h>

/* ---- low level helpers ---- */

static inline uint8_t rseq(Sis661fxPtr p, uint8_t idx) { return sis661fx_read_idx(p, SIS_VGA_SEQ_INDEX, idx); }
static inline void    wseq(Sis661fxPtr p, uint8_t idx, uint8_t v) { sis661fx_write_idx(p, SIS_VGA_SEQ_INDEX, idx, v); }
static inline void    rmwseq(Sis661fxPtr p, uint8_t idx, uint8_t m, uint8_t v) { sis661fx_rmw_idx(p, SIS_VGA_SEQ_INDEX, idx, m, v); }
static inline uint8_t rcrt(Sis661fxPtr p, uint8_t idx) { return sis661fx_read_idx(p, SIS_VGA_CRTC_INDEX, idx); }
static inline void    wcrt(Sis661fxPtr p, uint8_t idx, uint8_t v) { sis661fx_write_idx(p, SIS_VGA_CRTC_INDEX, idx, v); }
static inline void    rmwcrt(Sis661fxPtr p, uint8_t idx, uint8_t m, uint8_t v) { sis661fx_rmw_idx(p, SIS_VGA_CRTC_INDEX, idx, m, v); }
static inline uint8_t rgfx(Sis661fxPtr p, uint8_t idx) { return sis661fx_read_idx(p, SIS_VGA_GFX_INDEX, idx); }
static inline void    wgfx(Sis661fxPtr p, uint8_t idx, uint8_t v) { sis661fx_write_idx(p, SIS_VGA_GFX_INDEX, idx, v); }

void sis661fx_unlock(Sis661fxPtr p)
{
    /* SiS extension unlock: SR05 = 0x86 */
    wseq(p, SIS_SR_UNLOCK, 0x86);
    /* CRTC unlock: CR11 bit7 protects CR0-7; clear during mode set */
}

static void sis661fx_wait_engine_idle(Sis661fxPtr p)
{
    /* From RE: vgasetmode waits on MMIO 0x85CC bit 31 and 0x85D8 status */
    int i;
    for (i = 0; i < 500000; i++) {
        uint32_t s = sis661fx_mmio_read32(p, SIS_MMIO_Q_STATUS);
        if (s & 0x80000000) break;
    }
    for (i = 0; i < 500000; i++) {
        uint32_t s = sis661fx_mmio_read32(p, SIS_MMIO_Q_STATUS);
        if (s & 0x80000000) break;
    }
}

/* ---- PLL programming ----
 * SiS 661FX pixel clock: write numerator/denominator/post to SR2B/SR2C/SR2D
 * (standard SiS 315/330/661 family layout).  This is a direct I/O programming
 * path; VCLK = 14.318 * (N+1) / ((D+1) * postdiv).
 */
static void sis661fx_set_vclk(Sis661fxPtr p, int clock_khz)
{
    /* Find a reasonable N/D/post. Reference: 14.31818 MHz. */
    static const uint8_t post_div[] = {1, 2, 4, 8};
    static const uint8_t post_bits[] = {0x00, 0x01, 0x02, 0x03};
    int best_n = 0, best_d = 0, best_p = 0;
    long best_err = 100000000L;
    int i, n, d;
    long target = clock_khz;

    for (i = 0; i < 4; i++) {
        for (d = 0; d < 128; d++) {
            long denom = (long)(d + 1) * post_div[i];
            n = (int)((target * denom + 7159) / 14318L) - 1;
            if (n < 0 || n > 255) continue;
            {
                long v = 14318L * (n + 1) / denom;
                long err = v - target;
                if (err < 0) err = -err;
                if (err < best_err) {
                    best_err = err;
                    best_n = n; best_d = d; best_p = i;
                    if (err == 0) goto found;
                }
            }
        }
    }
found:
    /* SR2B = N, SR2C = D, SR2D[1:0] = post, SR2D[7]=vclk source select */
    wseq(p, 0x2B, (uint8_t)best_n);
    wseq(p, 0x2C, (uint8_t)best_d);
    rmwseq(p, 0x2D, 0xFC, post_bits[best_p]);
    /* Fire VCLK: SR07[3:2] sequence - toggle to latch */
    {
        uint8_t sr07 = rseq(p, 0x07);
        wseq(p, 0x07, sr07 | 0x10);
        wseq(p, 0x07, sr07 & ~0x10);
    }
}

/* ---- Mode programming ---- */

static void sis661fx_set_linear_base(Sis661fxPtr p)
{
    /* SR20 = LFB address bits [31:24] or [27:20] depending on chip.
     * For 661FX, SR20 contains high bits of linear framebuffer address. */
    uint32_t base = (uint32_t)(p->fbPhys >> 20);
    wseq(p, 0x20, (uint8_t)(base & 0xFF));
    /* SR21: additional bits / config */
    rmwseq(p, 0x21, 0xF0, (uint8_t)((base >> 8) & 0x0F));
}

static void sis661fx_set_pitch(Sis661fxPtr p, int width, int bpp)
{
    int pitch = ((width * bpp / 8) + 15) & ~15;
    int offset = pitch / 8; /* CRTC offset in "word/quad" units varies; use byte/8 */
    p->pitch = pitch;
    wcrt(p, 0x13, (uint8_t)(offset & 0xFF));
    rmwcrt(p, 0x0C, 0x00, 0x00); /* screen start high */
    /* SiS ext offset high bits: CR1A/CR1B or CR0C/CR0D ext - use CR2E/CR2F on 661? */
    /* Conservative: set CR1C/CR1D extended screen address on 661 family */
    rmwcrt(p, 0x1B, 0xF0, (uint8_t)((offset >> 8) & 0x0F));
}

Bool sis661fx_set_mode(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    int htotal, hdisplay, hsyncstart, hsyncend;
    int vtotal, vdisplay, vsyncstart, vsyncend;
    int bpp = pScrn->bitsPerPixel;
    uint8_t misc;

    sis661fx_unlock(p);
    sis661fx_wait_engine_idle(p);

    /* Turn off screen */
    sis661fx_outb(p, SIS_VGA_SEQ_INDEX, 0x01);
    sis661fx_outb(p, SIS_VGA_SEQ_DATA, (uint8_t)(sis661fx_inb(p, SIS_VGA_SEQ_DATA) | 0x20));

    htotal = mode->CrtcHTotal / 8 - 5;
    hdisplay = mode->CrtcHDisplay / 8 - 1;
    hsyncstart = mode->CrtcHSyncStart / 8;
    hsyncend = mode->CrtcHSyncEnd / 8;
    vtotal = mode->CrtcVTotal - 2;
    vdisplay = mode->CrtcVDisplay - 1;
    vsyncstart = mode->CrtcVSyncStart;
    vsyncend = mode->CrtcVSyncEnd;

    /* Misc output: clock select bits + sync polarity */
    misc = 0x23; /* enable RAM, use CRTC 0x3Dx, clock bits 0 */
    if (!(mode->Flags & V_PHSYNC)) misc |= 0x40;
    if (!(mode->Flags & V_PVSYNC)) misc |= 0x80;
    sis661fx_outb(p, SIS_VGA_MISC_W, misc);

    /* Sequencer */
    wseq(p, 0x00, 0x03); /* reset */
    wseq(p, 0x01, 0x01); /* screen off, 8 dot */
    wseq(p, 0x02, 0x0F); /* plane mask */
    wseq(p, 0x03, 0x00); /* character map */
    wseq(p, 0x04, 0x0E); /* memory mode: ext mem, chain4 off, odd/even off */

    /* CRTC unlock */
    wcrt(p, 0x11, (uint8_t)((rcrt(p, 0x11) & 0x7F))); /* unprotect CR0-7 */

    /* Standard CRTC */
    wcrt(p, 0x00, (uint8_t)(htotal & 0xFF));
    wcrt(p, 0x01, (uint8_t)(hdisplay & 0xFF));
    wcrt(p, 0x02, (uint8_t)(hdisplay & 0xFF));
    wcrt(p, 0x03, (uint8_t)(((htotal + 1) & 0x1F) | 0x80));
    wcrt(p, 0x04, (uint8_t)(hsyncstart & 0xFF));
    wcrt(p, 0x05, (uint8_t)(((hsyncend & 0x1F)) | (((htotal + 1) & 0x20) << 2)));
    wcrt(p, 0x06, (uint8_t)(vtotal & 0xFF));
    wcrt(p, 0x07, (uint8_t)(
        ((vtotal >> 8) & 0x01) |
        (((vdisplay) >> 8) & 0x02) << 1 |
        (((vsyncstart) >> 8) & 0x01) << 2 |
        (((vdisplay) >> 8) & 0x01) << 3 |
        0x10 |
        (((vtotal >> 8) & 0x02) << 4) |
        (((vsyncstart) >> 8) & 0x02) << 5)
    );
    wcrt(p, 0x08, 0x00);
    wcrt(p, 0x09, (uint8_t)((((vdisplay >> 8) & 0x02) << 5) | 0x40));
    wcrt(p, 0x0A, 0x00);
    wcrt(p, 0x0B, 0x00);
    wcrt(p, 0x0C, 0x00);
    wcrt(p, 0x0D, 0x00);
    wcrt(p, 0x0E, 0x00);
    wcrt(p, 0x0F, 0x00);
    wcrt(p, 0x10, (uint8_t)(vsyncstart & 0xFF));
    wcrt(p, 0x11, (uint8_t)((vsyncend & 0x0F) | 0x20)); /* protect off, clear */
    wcrt(p, 0x12, (uint8_t)(vdisplay & 0xFF));
    wcrt(p, 0x13, 0x00); /* offset - set later */
    wcrt(p, 0x14, 0x40); /* underline off, dword mode */
    wcrt(p, 0x15, (uint8_t)(vdisplay & 0xFF));
    wcrt(p, 0x16, (uint8_t)((vtotal + 1) & 0xFF));
    wcrt(p, 0x17, 0xE3); /* mode control */
    wcrt(p, 0x18, 0xFF);

    /* Graphics controller: linear, 256 color, packed pixel */
    wgfx(p, 0x00, 0x00);
    wgfx(p, 0x01, 0x00);
    wgfx(p, 0x02, 0x00);
    wgfx(p, 0x03, 0x00);
    wgfx(p, 0x04, 0x00);
    wgfx(p, 0x05, 0x40); /* 256 color, write mode 0 */
    wgfx(p, 0x06, 0x05); /* graphics, linear, A0000-AFFFF */
    wgfx(p, 0x07, 0x0F);
    wgfx(p, 0x08, 0xFF);

    /* Attribute controller */
    {
        int i;
        sis661fx_inb(p, SIS_VGA_INSTAT_R); /* reset flip-flop */
        for (i = 0; i < 16; i++) {
            sis661fx_outb(p, SIS_VGA_ATTR_INDEX, (uint8_t)i);
            sis661fx_outb(p, SIS_VGA_ATTR_DATA_W, (uint8_t)i);
        }
        sis661fx_outb(p, SIS_VGA_ATTR_INDEX, 0x10); sis661fx_outb(p, SIS_VGA_ATTR_DATA_W, 0x41);
        sis661fx_outb(p, SIS_VGA_ATTR_INDEX, 0x11); sis661fx_outb(p, SIS_VGA_ATTR_DATA_W, 0x00);
        sis661fx_outb(p, SIS_VGA_ATTR_INDEX, 0x12); sis661fx_outb(p, SIS_VGA_ATTR_DATA_W, 0x0F);
        sis661fx_outb(p, SIS_VGA_ATTR_INDEX, 0x13); sis661fx_outb(p, SIS_VGA_ATTR_DATA_W, 0x00);
        sis661fx_outb(p, SIS_VGA_ATTR_INDEX, 0x14); sis661fx_outb(p, SIS_VGA_ATTR_DATA_W, 0x00);
        sis661fx_outb(p, SIS_VGA_ATTR_INDEX, 0x20); /* enable video */
    }

    /* SiS extended: linear base, pitch */
    sis661fx_set_linear_base(p);
    sis661fx_set_pitch(p, mode->CrtcHDisplay, bpp);

    /* Color depth mode in SR/CR ext - 661FX specific */
    if (bpp <= 8) {
        rmwseq(p, 0x06, 0xE0, 0x00);
    } else if (bpp <= 16) {
        rmwseq(p, 0x06, 0xE0, 0x04); /* 16bpp */
    } else {
        rmwseq(p, 0x06, 0xE0, 0x0C); /* 32bpp */
    }

    /* Enable linear framebuffer / high-res mode */
    wseq(p, 0x0A, 0x02); /* enable ext */
    rmwcrt(p, 0x19, 0x00, 0x00);
    /* CR38/CR79 etc for 661? 661 uses different layout (NewCRLayout in upstream) */
    rmwcrt(p, 0x38, 0x00, 0x00);

    /* PLL */
    sis661fx_set_vclk(p, mode->Clock);

    /* Unblank */
    wseq(p, 0x01, (uint8_t)(rseq(p, 0x01) & ~0x20));
    sis661fx_outb(p, SIS_VGA_SEQ_INDEX, 0x00);
    sis661fx_outb(p, SIS_VGA_SEQ_DATA, 0x03);

    memcpy(&p->currentMode, mode, sizeof(DisplayModeRec));
    p->bpp = bpp;
    p->depth = pScrn->depth;
    return TRUE;
}

void sis661fx_save(ScrnInfoPtr pScrn)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    int i;
    sis661fx_unlock(p);
    p->savedMisc = sis661fx_inb(p, SIS_VGA_MISC_R);
    for (i = 0; i < 0x10; i++) p->savedGR[i] = rgfx(p, (uint8_t)i);
    for (i = 0; i < 0x80; i++) p->savedSR[i] = rseq(p, (uint8_t)i);
    for (i = 0; i < 0x100; i++) p->savedCR[i] = rcrt(p, (uint8_t)i);
    sis661fx_inb(p, SIS_VGA_INSTAT_R);
    for (i = 0; i < 0x15; i++) {
        sis661fx_outb(p, SIS_VGA_ATTR_INDEX, (uint8_t)i);
        p->savedAR[i] = sis661fx_inb(p, SIS_VGA_ATTR_DATA_R);
    }
    sis661fx_outb(p, SIS_VGA_ATTR_INDEX, 0x20);
}

void sis661fx_restore(ScrnInfoPtr pScrn)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    int i;
    sis661fx_unlock(p);
    sis661fx_outb(p, SIS_VGA_MISC_W, p->savedMisc);
    for (i = 0; i < 0x80; i++) wseq(p, (uint8_t)i, p->savedSR[i]);
    /* unprotect CR0-7 before restore */
    wcrt(p, 0x11, (uint8_t)(p->savedCR[0x11] & 0x7F));
    for (i = 0; i < 0x100; i++) wcrt(p, (uint8_t)i, p->savedCR[i]);
    for (i = 0; i < 0x10; i++) wgfx(p, (uint8_t)i, p->savedGR[i]);
    sis661fx_inb(p, SIS_VGA_INSTAT_R);
    for (i = 0; i < 0x15; i++) {
        sis661fx_outb(p, SIS_VGA_ATTR_INDEX, (uint8_t)i);
        sis661fx_outb(p, SIS_VGA_ATTR_DATA_W, p->savedAR[i]);
    }
    sis661fx_outb(p, SIS_VGA_ATTR_INDEX, 0x20);
}

Bool sis661fx_mode_preinit(ScrnInfoPtr pScrn)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    /* Read VRAM size: 661FX UMA from SR14[3:0] or config. Common sizes 32/64/128MB.
     * From RE: miniport reads pHWDE->ulFBMaxLength. We'll use a conservative default
     * and allow override via VideoRam option. */
    uint8_t sr14 = rseq(p, 0x14);
    int mb = 0;
    switch (sr14 & 0x3F) {
    case 0x01: case 0x10: mb = 8; break;
    case 0x02: case 0x11: mb = 16; break;
    case 0x03: case 0x12: mb = 32; break;
    case 0x04: case 0x13: mb = 64; break;
    case 0x05: case 0x14: mb = 128; break;
    case 0x06: case 0x15: mb = 256; break;
    default: mb = 64; break; /* safe default */
    }
    p->videoRamKB = mb * 1024;
    if (pScrn->videoRam == 0) pScrn->videoRam = p->videoRamKB;
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SiS661FX: detected ~%d MB VRAM (SR14=0x%02x)\n", mb, sr14);

    /* Detect chip family via CR5C, as recovered from vendor driver */
    {
        uint8_t cr5c = rcrt(p, 0x5C) & 0xF8;
        uint8_t cr5f = rcrt(p, 0x5F);
        p->chipFamily = cr5c;
        p->chipRev = cr5f;
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SiS661FX: CR5C=0x%02x CR5F=0x%02x (661FX family)\n", cr5c, cr5f);
    }
    return TRUE;
}
