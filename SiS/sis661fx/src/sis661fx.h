#ifndef SIS661FX_H
#define SIS661FX_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#include <xorg-server.h>
#include <xf86.h>
#include <xf86_OSproc.h>
#include <xf86Pci.h>
#include <xf86Cursor.h>
#include <pciaccess.h>
#include <xf86DDC.h>
#include <xf86i2c.h>
#include <xf86Crtc.h>

#include "compat-api.h"
#include "sis661fx_regs.h"

#define SIS661FX_NAME    "SIS661FX"
#define SIS661FX_DRIVER_NAME "sis661fx"
#define SIS661FX_VERSION  PACKAGE_VERSION

typedef struct {
    struct pci_device *pci;
    EntityInfoPtr pEnt;
    OptionInfoPtr Options;

    /* MMIO / FB mapping */
    uint8_t *fbBase;
    uint8_t *mmioBase;
    unsigned long fbPhys;
    unsigned long mmioPhys;
    unsigned long fbSize;
    unsigned long mmioSize;
    int fbMapSize; /* bytes used for screen */

    /* I/O (legacy VGA) */
    unsigned long ioBase; /* 0x3C0 base assumed */

    /* Chip info */
    uint16_t chipId;
    uint8_t  revision;
    uint8_t  chipFamily; /* CR5C & 0xF8 */
    uint8_t  chipRev;    /* CR5F */

    /* Mode */
    DisplayModeRec currentMode;
    int bpp;
    int depth;
    int pitch; /* bytes */
    int videoRamKB;

    /* Accel */
    Bool useAccel;
    Bool noAccel;
    void *exa;
    void *exaPriv;

    /* Cursor */
    xf86CursorInfoPtr cursorInfo;
    uint32_t cursorAddr;

    /* Saved state */
    uint8_t savedSR[0x80];
    uint8_t savedCR[0x100];
    uint8_t savedGR[0x10];
    uint8_t savedAR[0x20];
    uint8_t savedMisc;

    CreateScreenResourcesProcPtr CreateScreenResources;
    CloseScreenProcPtr CloseScreen;
    Bool (*EnterVT)(VT_FUNC_ARGS_DECL);
    void (*LeaveVT)(VT_FUNC_ARGS_DECL);
} Sis661fxRec, *Sis661fxPtr;

#define SIS661FX(p) ((Sis661fxPtr)((p)->driverPrivate))

/* mmio/io */
uint8_t  sis661fx_inb(Sis661fxPtr p, uint16_t port);
void     sis661fx_outb(Sis661fxPtr p, uint16_t port, uint8_t val);
uint8_t  sis661fx_read_idx(Sis661fxPtr p, uint16_t base, uint8_t idx);
void     sis661fx_write_idx(Sis661fxPtr p, uint16_t base, uint8_t idx, uint8_t val);
void     sis661fx_rmw_idx(Sis661fxPtr p, uint16_t base, uint8_t idx, uint8_t mask, uint8_t val);

uint32_t sis661fx_mmio_read32(Sis661fxPtr p, uint32_t off);
void     sis661fx_mmio_write32(Sis661fxPtr p, uint32_t off, uint32_t val);
void     sis661fx_mmio_write16(Sis661fxPtr p, uint32_t off, uint16_t val);

/* mode */
Bool sis661fx_mode_preinit(ScrnInfoPtr pScrn);
Bool sis661fx_set_mode(ScrnInfoPtr pScrn, DisplayModePtr mode);
void sis661fx_save(ScrnInfoPtr pScrn);
void sis661fx_restore(ScrnInfoPtr pScrn);
void sis661fx_unlock(Sis661fxPtr p);

/* accel */
Bool sis661fx_accel_init(ScreenPtr pScreen);
/* cursor */
Bool sis661fx_cursor_init(ScreenPtr pScreen);
Bool sis661fx_video_init(ScreenPtr pScreen);

#endif
