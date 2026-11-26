#ifndef SIS661FX_REGS_H
#define SIS661FX_REGS_H

#include <stdint.h>

/* PCI */
#define SIS_VENDOR_ID           0x1039
#define SIS_DEVICE_661FX        0x6330

/* Legacy VGA I/O ports */
#define SIS_VGA_MISC_W          0x3C2
#define SIS_VGA_MISC_R          0x3CC
#define SIS_VGA_SEQ_INDEX       0x3C4
#define SIS_VGA_SEQ_DATA        0x3C5
#define SIS_VGA_CRTC_INDEX      0x3D4
#define SIS_VGA_CRTC_DATA       0x3D5
#define SIS_VGA_GFX_INDEX       0x3CE
#define SIS_VGA_GFX_DATA        0x3CF
#define SIS_VGA_ATTR_INDEX      0x3C0
#define SIS_VGA_ATTR_DATA_W     0x3C0
#define SIS_VGA_ATTR_DATA_R     0x3C1
#define SIS_VGA_INSTAT_R        0x3DA
#define SIS_VGA_DAC_MASK        0x3C6
#define SIS_VGA_DAC_READ        0x3C7
#define SIS_VGA_DAC_WRITE       0x3C8
#define SIS_VGA_DAC_DATA        0x3C9

/* SiS extended sequencer registers (SR) */
#define SIS_SR_UNLOCK           0x05  /* write 0x86 to unlock */
#define SIS_SR_EXT_MISC         0x06  /* extended misc */
#define SIS_SR_DRAM_SIZING      0x14  /* DRAM sizing */
#define SIS_SR_FB_ADDR_HIGH     0x20  /* LFB base high */
#define SIS_SR_CLK_NUM          0x2B  /* VCLK numerator */
#define SIS_SR_CLK_DEN          0x2C  /* VCLK denominator */
#define SIS_SR_CLK_POST         0x2D  /* VCLK post divider */

/* SiS extended CRTC registers (CR) - from RE (sisgrp.sys FUN_000166b2) */
#define SIS_CR_EXT_ID           0x5C  /* chip family: 0x10=661,0x80=760,0x90=741 */
#define SIS_CR_EXT_REV          0x5F  /* stepping */
#define SIS_CR_EXT_MISC_7C      0x7C  /* 661+ misc */
#define SIS_CR_EXT_MISC_7E      0x7E  /* VB type */

/* MMIO 2D engine registers (BAR1 offset) - from sis310_accel.h */
#define SIS_MMIO_SRC_ADDR       0x8200
#define SIS_MMIO_SRC_PITCH      0x8204
#define SIS_MMIO_SRC_Y          0x8208
#define SIS_MMIO_SRC_X          0x820A
#define SIS_MMIO_DST_Y          0x820C
#define SIS_MMIO_DST_X          0x820E
#define SIS_MMIO_DST_ADDR       0x8210
#define SIS_MMIO_DST_PITCH      0x8214
#define SIS_MMIO_DST_HEIGHT     0x8216
#define SIS_MMIO_RECT_WIDTH     0x8218
#define SIS_MMIO_RECT_HEIGHT    0x821A
#define SIS_MMIO_PAT_FG         0x821C
#define SIS_MMIO_PAT_BG         0x8220
#define SIS_MMIO_SRC_FG         0x8224
#define SIS_MMIO_SRC_BG         0x8228
#define SIS_MMIO_MONO_MASK      0x822C
#define SIS_MMIO_LEFT_CLIP      0x8234
#define SIS_MMIO_TOP_CLIP       0x8236
#define SIS_MMIO_RIGHT_CLIP     0x8238
#define SIS_MMIO_BOTTOM_CLIP    0x823A
#define SIS_MMIO_CMD_READY      0x823C
#define SIS_MMIO_FIRE_TRIGGER   0x8240
#define SIS_MMIO_PAT_BUF        0x8300
#define SIS_MMIO_Q_BASE         0x85C0
#define SIS_MMIO_Q_WRITE        0x85C4
#define SIS_MMIO_Q_READ         0x85C8
#define SIS_MMIO_Q_STATUS       0x85CC

/* 2D engine command bits (SiS 315/330/661 family) */
#define SIS_CMD_BITBLT          0x00000000
#define SIS_CMD_COLOREXP        0x00000001
#define SIS_CMD_ENCOLOREXP      0x00000002
#define SIS_CMD_RECT            0x00000010
#define SIS_CMD_RECT_CLIP_EN    0x00000020
#define SIS_CMD_RECT_FILL       0x00000040
#define SIS_CMD_LINE            0x00000080
#define SIS_CMD_3D              0x00000100
#define SIS_CMD_ALPHA           0x00000200
#define SIS_ROP_COPY            0xCC
#define SIS_CMD_AUTO            0x80000000

/* Chip family from RE (CR5C & 0xF8) */
#define SIS_FAMILY_661          0x10
#define SIS_FAMILY_760          0x80
#define SIS_FAMILY_741          0x90

#endif
