/*
 * SiS 661FX Xv overlay - direct MMIO register programming.
 * Based on SiS 315/330/661 video overlay registers (Index_VI_* family)
 * recovered from vendor driver analysis.
 */
#include "sis661fx.h"
#include <xf86xv.h>
#include <X11/extensions/Xv.h>
#include <fourcc.h>

#define VI_REG(reg) (0x9000 + ((reg) * 4)) /* common 661 overlay MMIO base */


static void vi_write(Sis661fxPtr p, uint8_t idx, uint8_t val) {
    sis661fx_mmio_write32(p, VI_REG(idx), val);
}
static uint8_t vi_read(Sis661fxPtr p, uint8_t idx) {
    return (uint8_t)sis661fx_mmio_read32(p, VI_REG(idx));
}

static void sis661fx_overlay_off(Sis661fxPtr p)
{
    vi_write(p, 0x2F, vi_read(p, 0x2F) & ~0x01); /* disable overlay */
    vi_write(p, 0x30, vi_read(p, 0x30) & ~0x01);
}

static int sis661fx_put_image(ScrnInfoPtr pScrn, short src_x, short src_y, short drw_x, short drw_y,
                              short src_w, short src_h, short drw_w, short drw_h,
                              int id, unsigned char *buf, short width, short height, Bool sync, RegionPtr clipBoxes, pointer data, DrawablePtr pDraw)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    uint32_t yAddr = (uint32_t)(p->videoRamKB * 1024 - 4*1024*1024); /* top of VRAM reserved */
    uint32_t uvAddr;
    int pitch = (width + 7) & ~7;
    int size = pitch * height;
    BoxPtr b = REGION_RECTS(clipBoxes);
    int n = REGION_NUM_RECTS(clipBoxes);
    (void)sync; (void)data; (void)pDraw; (void)src_x; (void)src_y;

    if (id != FOURCC_YV12 && id != FOURCC_I420) return BadMatch;
    if (n < 1) return Success;

    uvAddr = yAddr + size;
    /* copy Y */
    for (int i = 0; i < height; i++)
        memcpy(p->fbBase + yAddr + i * pitch, buf + i * width, width);
    /* copy UV */
    for (int i = 0; i < height/2; i++) {
        memcpy(p->fbBase + uvAddr + i * (pitch/2), buf + size + i * (width/2), width/2);
        memcpy(p->fbBase + uvAddr + (size/4) + i * (pitch/2), buf + size + (size/4) + i * (width/2), width/2);
    }

    /* Program overlay registers (661/315 family layout) */
    vi_write(p, 0x00, 0x80); /* password */
    vi_write(p, 0x07, yAddr & 0xFF);
    vi_write(p, 0x08, (yAddr >> 8) & 0xFF);
    vi_write(p, 0x09, (yAddr >> 16) & 0xFF);
    vi_write(p, 0x0A, uvAddr & 0xFF);
    vi_write(p, 0x0B, (uvAddr >> 8) & 0xFF);
    vi_write(p, 0x0C, (uvAddr >> 16) & 0xFF);
    vi_write(p, 0x0D, uvAddr & 0xFF);
    vi_write(p, 0x0E, (uvAddr >> 8) & 0xFF);
    vi_write(p, 0x0F, (uvAddr >> 16) & 0xFF);
    vi_write(p, 0x10, pitch & 0xFF);
    vi_write(p, 0x11, (pitch/2) & 0xFF);
    vi_write(p, 0x12, ((pitch >> 8) & 0x0F) | (((pitch/2) >> 4) & 0xF0));

    /* window */
    vi_write(p, 0x01, b->x1 & 0xFF);
    vi_write(p, 0x02, (b->x2 - 1) & 0xFF);
    vi_write(p, 0x03, ((b->x1 >> 8) & 0x0F) | (((b->x2 - 1) >> 4) & 0xF0));
    vi_write(p, 0x04, b->y1 & 0xFF);
    vi_write(p, 0x05, (b->y2 - 1) & 0xFF);
    vi_write(p, 0x06, ((b->y1 >> 8) & 0x0F) | (((b->y2 - 1) >> 4) & 0xF0));

    /* scale */
    {
        uint32_t hs = (src_w << 20) / max(drw_w,1);
        uint32_t vs = (src_h << 20) / max(drw_h,1);
        vi_write(p, 0x18, hs & 0xFF); vi_write(p, 0x19, (hs>>8)&0xFF);
        vi_write(p, 0x1A, vs & 0xFF); vi_write(p, 0x1B, (vs>>8)&0xFF);
        vi_write(p, 0x1C, ((hs>>16)&0x0F) | (((vs>>16)&0x0F)<<4));
    }

    vi_write(p, 0x2F, 0x01); /* enable, colorkey off */
    vi_write(p, 0x30, 0x80); /* YUV planar */
    vi_write(p, 0x31, 0x00);
    return Success;
}

static int sis661fx_query_image(ScrnInfoPtr pScrn, int id, unsigned short *w, unsigned short *h, int *pitches, int *offsets)
{
    int size, tmp;
    (void)pScrn;
    if (id != FOURCC_YV12 && id != FOURCC_I420) return 0;
    *w = (*w + 1) & ~1; *h = (*h + 1) & ~1;
    if (pitches) pitches[0] = *w;
    size = (*w) * (*h);
    if (offsets) offsets[0] = 0;
    tmp = (*w >> 1) * (*h >> 1);
    if (pitches) { pitches[1] = pitches[2] = *w >> 1; }
    if (offsets) { offsets[1] = size; offsets[2] = size + tmp; }
    return size + 2*tmp;
}

static void sis661fx_video_stop(ScrnInfoPtr pScrn, pointer data, Bool cleanup)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    (void)data;
    if (cleanup) sis661fx_overlay_off(p);
}

Bool sis661fx_video_init(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    XF86VideoAdaptorPtr adapt;
    static XF86AttributeRec attrs[] = { {XvSettable|XvGettable, 0, 255, "XV_BRIGHTNESS"}, {XvSettable|XvGettable,0,255,"XV_CONTRAST"} };
    static XF86ImageRec images[] = { XVIMAGE_YV12, XVIMAGE_I420 };
    adapt = xf86XVAllocateVideoAdaptorRec(pScrn);
    if (!adapt) return FALSE;
    adapt->type = XvInputMask | XvImageMask;
    adapt->nEncodings = 1;
    adapt->pEncodings = calloc(1, sizeof(XF86VideoEncodingRec));
    adapt->pEncodings[0].id = 0;
    adapt->pEncodings[0].name = "XV_IMAGE"; adapt->pEncodings[0].width=2048; adapt->pEncodings[0].height=2048;
    adapt->pEncodings[0].rate.numerator=1; adapt->pEncodings[0].rate.denominator=1;
    adapt->nAttributes = 2; adapt->pAttributes = attrs;
    adapt->nImages = 2; adapt->pImages = images;
    adapt->PutImage = sis661fx_put_image;
    adapt->QueryImageAttributes = sis661fx_query_image;
    adapt->StopVideo = sis661fx_video_stop;
    xf86XVScreenInit(pScreen, &adapt, 1);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Xv overlay adaptor initialized\n");
    return TRUE;
}
