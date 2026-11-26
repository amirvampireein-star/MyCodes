/*
 * SiS 661FX 2D acceleration - direct MMIO, based on RE of sisgrv.dll/sisgrp.sys.
 *
 * Register map (from RE):
 *  0x8200 SRC_ADDR   0x8204 SRC_PITCH   0x8208 SRC_Y  0x820A SRC_X
 *  0x820C DST_Y      0x820E DST_X       0x8210 DST_ADDR 0x8214 DST_PITCH
 *  0x8216 DST_HEIGHT 0x8218 RECT_WIDTH  0x821A RECT_HEIGHT
 *  0x821C PAT_FG     0x8220 PAT_BG      0x8224 SRC_FG 0x8228 SRC_BG
 *  0x822C MONO_MASK  0x8234 L_CLIP      0x8236 T_CLIP 0x8238 R_CLIP 0x823A B_CLIP
 *  0x823C CMD_READY  0x8240 FIRE_TRIGGER
 *  0x85CC Q_STATUS (bit31 = ready)
 *
 * Command encoding (SiS 315/330/661 family, used by Windows driver):
 *  ROP in bits 15:8, op bits in 7:0, depth in 17:16
 *  BitBlt: 0x00000000, Screen-Screen: dir bits, Solid fill: 0x000000F0
 */

#include "sis661fx.h"
#include <exa.h>
#include <string.h>

#define SIS_ROP_PATCOPY  0xF0
#define SIS_ROP_SRCCOPY  0xCC

#define SIS_CMD_BITBLT   0x0000
#define SIS_CMD_COLOREXP 0x0001
#define SIS_CMD_ENCOLOREXP 0x0002
#define SIS_CMD_RECT     0x0010
#define SIS_CMD_RECT_CLIP 0x0020
#define SIS_CMD_FILL     0x0040
#define SIS_CMD_DIR_XINC 0x0100
#define SIS_CMD_DIR_YINC 0x0200

static inline void sis_wq(Sis661fxPtr p, uint32_t reg, uint32_t val) {
    sis661fx_mmio_write32(p, reg, val);
}
static inline uint32_t sis_rq(Sis661fxPtr p, uint32_t reg) {
    return sis661fx_mmio_read32(p, reg);
}

static void sis661fx_engine_idle(Sis661fxPtr p)
{
    int i;
    for (i = 0; i < 2000000; i++) {
        uint32_t s = sis_rq(p, SIS_MMIO_Q_STATUS);
        if (s & 0x80000000) return;
        if ((s & 0xFF000000) == 0xFF000000) return;
    }
    /* fallback: FIRE_TRIGGER bit10 idle */
    for (i = 0; i < 2000000; i++) {
        uint32_t t = sis_rq(p, SIS_MMIO_FIRE_TRIGGER);
        if (!(t & (1u << 10))) return;
    }
}

static void sis661fx_set_clip(Sis661fxPtr p, int x1, int y1, int x2, int y2)
{
    sis_wq(p, SIS_MMIO_LEFT_CLIP,  (uint32_t)(x1 & 0xFFFF) | ((uint32_t)(x2 & 0xFFFF) << 16));
    sis_wq(p, SIS_MMIO_TOP_CLIP,   (uint32_t)(y1 & 0xFFFF) | ((uint32_t)(y2 & 0xFFFF) << 16));
}

/* ---- EXA ---- */

typedef struct {
    Sis661fxPtr pSiS;
    uint32_t fg;
    uint32_t rop;
    int pitch;
    int bpp;
    int xdir, ydir;
} SisExa, *SisExaPtr;

static Bool SISPrepareSolid(PixmapPtr pPix, int alu, Pixel pm, Pixel fg)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pPix->drawable.pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    SisExaPtr exa = (SisExaPtr)p->exaPriv;
    if (alu != GXcopy) return FALSE;
    if ((pm & FbFullMask(pPix->drawable.depth)) != FbFullMask(pPix->drawable.depth)) return FALSE;
    if (pPix->drawable.bitsPerPixel != 16 && pPix->drawable.bitsPerPixel != 32) return FALSE;
    exa->fg = (uint32_t)fg;
    exa->pitch = exaGetPixmapPitch(pPix);
    exa->bpp = pPix->drawable.bitsPerPixel;
    exa->rop = SIS_ROP_PATCOPY;
    return TRUE;
}

static void SISSolid(PixmapPtr pPix, int x1, int y1, int x2, int y2)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pPix->drawable.pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    SisExaPtr exa = (SisExaPtr)p->exaPriv;
    unsigned long dst = exaGetPixmapOffset(pPix);
    int w = x2 - x1, h = y2 - y1;
    uint32_t cmd;
    if (w <= 0 || h <= 0) return;
    sis661fx_engine_idle(p);
    sis_wq(p, SIS_MMIO_DST_ADDR, (uint32_t)dst);
    sis_wq(p, SIS_MMIO_DST_PITCH, (uint32_t)(exa->pitch & 0xFFFF));
    sis_wq(p, SIS_MMIO_DST_Y, (uint32_t)(y1 & 0xFFFF) | ((uint32_t)(x1 & 0xFFFF) << 16));
    sis_wq(p, SIS_MMIO_RECT_WIDTH, (uint32_t)((w) & 0xFFFF) | ((uint32_t)((h) & 0xFFFF) << 16));
    sis_wq(p, SIS_MMIO_PAT_FG, exa->fg);
    sis_wq(p, SIS_MMIO_PAT_BG, exa->fg);
    sis_wq(p, SIS_MMIO_MONO_MASK, 0);
    sis661fx_set_clip(p, 0, 0, pScrn->virtualX - 1, pScrn->virtualY - 1);

    cmd = SIS_CMD_RECT | SIS_CMD_FILL | (exa->rop << 8);
    if (exa->bpp == 16) cmd |= (1u << 16);
    else if (exa->bpp == 32) cmd |= (2u << 16);
    sis_wq(p, SIS_MMIO_CMD_READY, 0x80);
    sis_wq(p, SIS_MMIO_FIRE_TRIGGER, cmd);
}

static void SISDoneSolid(PixmapPtr pPix) { (void)pPix; }

static Bool SISPrepareCopy(PixmapPtr pSrc, PixmapPtr pDst, int xdir, int ydir, int alu, Pixel pm)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pDst->drawable.pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    SisExaPtr exa = (SisExaPtr)p->exaPriv;
    if (alu != GXcopy) return FALSE;
    if ((pm & FbFullMask(pDst->drawable.depth)) != FbFullMask(pDst->drawable.depth)) return FALSE;
    if (pSrc->drawable.bitsPerPixel != pDst->drawable.bitsPerPixel) return FALSE;
    if (pDst->drawable.bitsPerPixel != 16 && pDst->drawable.bitsPerPixel != 32) return FALSE;
    exa->pitch = exaGetPixmapPitch(pDst);
    exa->bpp = pDst->drawable.bitsPerPixel;
    exa->xdir = xdir; exa->ydir = ydir;
    exa->rop = SIS_ROP_SRCCOPY;
    return TRUE;
}

static void SISCopy(PixmapPtr pDst, int srcX, int srcY, int dstX, int dstY, int w, int h)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pDst->drawable.pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    SisExaPtr exa = (SisExaPtr)p->exaPriv;
    unsigned long srcBase = exaGetPixmapOffset(pDst); /* EXA uses same offset for src/dst in screen-screen */
    unsigned long dstBase = exaGetPixmapOffset(pDst);
    uint32_t cmd;
    int sx = srcX, sy = srcY, dx = dstX, dy = dstY;
    if (w <= 0 || h <= 0) return;
    if (exa->xdir < 0) { sx += w - 1; dx += w - 1; }
    if (exa->ydir < 0) { sy += h - 1; dy += h - 1; }
    sis661fx_engine_idle(p);
    sis_wq(p, SIS_MMIO_SRC_ADDR, (uint32_t)srcBase);
    sis_wq(p, SIS_MMIO_SRC_PITCH, (uint32_t)(exa->pitch & 0xFFFF));
    sis_wq(p, SIS_MMIO_DST_ADDR, (uint32_t)dstBase);
    sis_wq(p, SIS_MMIO_DST_PITCH, (uint32_t)(exa->pitch & 0xFFFF));
    sis_wq(p, SIS_MMIO_SRC_Y, (uint32_t)(sy & 0xFFFF) | ((uint32_t)(sx & 0xFFFF) << 16));
    sis_wq(p, SIS_MMIO_DST_Y, (uint32_t)(dy & 0xFFFF) | ((uint32_t)(dx & 0xFFFF) << 16));
    sis_wq(p, SIS_MMIO_RECT_WIDTH, (uint32_t)(w & 0xFFFF) | ((uint32_t)(h & 0xFFFF) << 16));
    sis661fx_set_clip(p, 0, 0, pScrn->virtualX - 1, pScrn->virtualY - 1);

    cmd = SIS_CMD_BITBLT | (exa->rop << 8);
    if (exa->xdir > 0) cmd |= SIS_CMD_DIR_XINC;
    if (exa->ydir > 0) cmd |= SIS_CMD_DIR_YINC;
    if (exa->bpp == 16) cmd |= (1u << 16);
    else if (exa->bpp == 32) cmd |= (2u << 16);
    sis_wq(p, SIS_MMIO_CMD_READY, 0x80);
    sis_wq(p, SIS_MMIO_FIRE_TRIGGER, cmd);
}

static void SISDoneCopy(PixmapPtr pDst) { (void)pDst; }

static void SISWaitMarker(ScreenPtr pScreen, int marker)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    (void)marker;
    sis661fx_engine_idle(p);
}

static Bool SISUploadToScreen(PixmapPtr pDst, int x, int y, int w, int h, char *src, int src_pitch)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pDst->drawable.pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    unsigned char *dst;
    int bpp = pDst->drawable.bitsPerPixel / 8;
    int dst_pitch = exaGetPixmapPitch(pDst);
    unsigned long offset = exaGetPixmapOffset(pDst);
    sis661fx_engine_idle(p);
    dst = p->fbBase + offset + y * dst_pitch + x * bpp;
    for (int i = 0; i < h; i++)
        memcpy(dst + i * dst_pitch, src + i * src_pitch, w * bpp);
    return TRUE;
}

static Bool SISDownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h, char *dst, int dst_pitch)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pSrc->drawable.pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    unsigned char *src;
    int bpp = pSrc->drawable.bitsPerPixel / 8;
    int src_pitch = exaGetPixmapPitch(pSrc);
    unsigned long offset = exaGetPixmapOffset(pSrc);
    sis661fx_engine_idle(p);
    src = p->fbBase + offset + y * src_pitch + x * bpp;
    for (int i = 0; i < h; i++)
        memcpy(dst + i * dst_pitch, src + i * src_pitch, w * bpp);
    return TRUE;
}

Bool sis661fx_accel_init(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    ExaDriverPtr exa;
    struct {
        Sis661fxPtr pSiS;
        uint32_t fg, rop;
        int pitch, bpp, xdir, ydir;
    } *priv;

    if (p->noAccel) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "2D acceleration disabled\n");
        return TRUE;
    }

    /* Init engine */
    sis_wq(p, SIS_MMIO_Q_BASE, 0);
    sis_wq(p, SIS_MMIO_Q_WRITE, 0);
    sis_wq(p, SIS_MMIO_Q_READ, 0);
    sis_wq(p, SIS_MMIO_Q_STATUS, 0x80000000);
    sis661fx_engine_idle(p);

    exa = exaDriverAlloc();
    if (!exa) return FALSE;
    priv = calloc(1, sizeof(*priv));
    if (!priv) { free(exa); return FALSE; }
    priv->pSiS = p;
    p->exaPriv = priv;
    p->exa = exa;

    exa->exa_major = EXA_VERSION_MAJOR;
    exa->exa_minor = EXA_VERSION_MINOR;
    exa->memoryBase = p->fbBase;
    exa->offScreenBase = (pScrn->virtualY * p->pitch + 4095) & ~4095;
    exa->memorySize = p->videoRamKB * 1024;
    exa->pixmapOffsetAlign = 16;
    exa->pixmapPitchAlign = 16;
    exa->flags = EXA_OFFSCREEN_PIXMAPS;
    exa->maxX = 2048; exa->maxY = 2048;
    exa->PrepareSolid = SISPrepareSolid;
    exa->Solid = SISSolid;
    exa->DoneSolid = SISDoneSolid;
    exa->PrepareCopy = SISPrepareCopy;
    exa->Copy = SISCopy;
    exa->DoneCopy = SISDoneCopy;
    exa->WaitMarker = SISWaitMarker;
    exa->UploadToScreen = SISUploadToScreen;
    exa->DownloadFromScreen = SISDownloadFromScreen;

    if (!exaDriverInit(pScreen, exa)) {
        free(priv); free(exa);
        p->exaPriv = NULL; p->exa = NULL;
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "EXA init failed, software rendering\n");
        return TRUE;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EXA 2D acceleration enabled (direct MMIO)\n");
    return TRUE;
}
