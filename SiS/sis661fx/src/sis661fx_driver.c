/*
 * xf86-video-sis661fx - SiS 661FX Xorg driver (direct MMIO/I/O)
 *
 * Written from scratch based on static reverse engineering of the vendor
 * Windows driver (sisgrp.sys, sisgr.drv, VBIOS) and SiS 661FX hardware
 * register semantics.  No code copied from xf86-video-sis or vendor binaries.
 *
 * Target: Xorg Server >= 1.18, tested on 21.1.22.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sis661fx.h"

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include <xf86Modes.h>
#include <fb.h>
#include <micmap.h>
#include <mi.h>
#include <shadow.h>
#include <xf86cmap.h>
#include <xf86Crtc.h>
#include <X11/extensions/dpmsconst.h>
#include <xf86fbman.h>

static Bool SIS661FXProbe(DriverPtr drv, int flags);
static Bool SIS661FXPreInit(ScrnInfoPtr pScrn, int flags);
static Bool SIS661FXScreenInit(SCREEN_INIT_ARGS_DECL);
static Bool SIS661FXSwitchMode(SWITCH_MODE_ARGS_DECL);
static void SIS661FXAdjustFrame(ADJUST_FRAME_ARGS_DECL);
static Bool SIS661FXEnterVT(VT_FUNC_ARGS_DECL);
static void SIS661FXLeaveVT(VT_FUNC_ARGS_DECL);
static Bool SIS661FXCloseScreen(CLOSE_SCREEN_ARGS_DECL);
static void SIS661FXFreeScreen(FREE_SCREEN_ARGS_DECL);

enum { OPTION_ACCEL, OPTION_NOACCEL };

static const OptionInfoRec SIS661FXOptions[] = {
    { OPTION_ACCEL,    "Accel",    OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_NOACCEL,  "NoAccel",  OPTV_BOOLEAN, {0}, FALSE },
    { -1, NULL, OPTV_NONE, {0}, FALSE }
};

static SymTabRec SIS661FXChipsets[] = {
    { SIS_DEVICE_661FX, "SiS 661FX/662/741/760/761/770 (1039:6330)" },
    { -1, NULL }
};

static PciChipsets SIS661FXPciChipsets[] = {
    { SIS_DEVICE_661FX, SIS_DEVICE_661FX, NULL },
    { -1, -1, NULL }
};

static void SIS661FXIdentify(int flags)
{
    (void)flags;
    xf86PrintChipsets(SIS661FX_NAME, "driver for SiS 661FX (1039:6330) direct MMIO", SIS661FXChipsets);
}

_X_EXPORT DriverRec SIS661FX = {
    .driverVersion = 1,
    .driverName    = SIS661FX_DRIVER_NAME,
    .Identify      = SIS661FXIdentify,
    .Probe         = SIS661FXProbe,
    .AvailableOptions = NULL,
    .module        = NULL,
    .refCount      = 0,
};


static Bool SIS661FXPciProbe(DriverPtr drv, int entity_num,
                              struct pci_device *dev, intptr_t match_data)
{
    ScrnInfoPtr pScrn;
    (void)match_data;
    pScrn = xf86ConfigPciEntity(NULL, 0, entity_num, NULL, NULL, NULL, NULL, NULL, NULL);
    if (!pScrn) return FALSE;
    pScrn->driverVersion = 1;
    pScrn->driverName    = SIS661FX_DRIVER_NAME;
    pScrn->name          = SIS661FX_NAME;
    pScrn->PreInit       = SIS661FXPreInit;
    pScrn->ScreenInit    = SIS661FXScreenInit;
    pScrn->SwitchMode    = SIS661FXSwitchMode;
    pScrn->AdjustFrame   = SIS661FXAdjustFrame;
    pScrn->EnterVT       = SIS661FXEnterVT;
    pScrn->LeaveVT       = SIS661FXLeaveVT;
    pScrn->FreeScreen    = SIS661FXFreeScreen;
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Probing SiS 661FX at %02x:%02x.%d\n",
               dev->bus, dev->dev, dev->func);
    return TRUE;
}

static Bool SIS661FXProbe(DriverPtr drv, int flags)
{
    int numDevSections;
    GDevPtr *devSections;
    Bool found = FALSE;
    int *usedChips;
    int numUsed;

    if ((numDevSections = xf86MatchDevice(SIS661FX_DRIVER_NAME, &devSections)) <= 0)
        return FALSE;

    numUsed = xf86MatchPciInstances(SIS661FX_NAME, SIS_VENDOR_ID, SIS661FXChipsets,
                                    SIS661FXPciChipsets, devSections, numDevSections,
                                    drv, &usedChips);
    free(devSections);
    if (numUsed <= 0) return FALSE;

    for (int i = 0; i < numUsed; i++) {
        struct pci_device *pPci = xf86GetPciInfoForEntity(usedChips[i]);
        if (!pPci) continue;
        if (pPci->vendor_id != SIS_VENDOR_ID || pPci->device_id != SIS_DEVICE_661FX) continue;
        if (SIS661FXPciProbe(drv, usedChips[i], pPci, 0))
            found = TRUE;
    }
    free(usedChips);
    return found;
}

static Bool SIS661FXGetRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate) return TRUE;
    pScrn->driverPrivate = XNFcallocarray(1, sizeof(Sis661fxRec));
    return pScrn->driverPrivate != NULL;
}

static void SIS661FXFreeRec(ScrnInfoPtr pScrn)
{
    if (!pScrn || !pScrn->driverPrivate) return;
    free(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

static void SIS661FXFreeScreen(FREE_SCREEN_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    SIS661FXFreeRec(pScrn);
}

static void SIS661FXAdjustFrame(ADJUST_FRAME_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    Sis661fxPtr p = SIS661FX(pScrn);
    int offset = (y * p->pitch + x * (p->bpp / 8)) / 4;
    /* CRTC start address */
    sis661fx_write_idx(p, SIS_VGA_CRTC_INDEX, 0x0D, (uint8_t)(offset & 0xFF));
    sis661fx_write_idx(p, SIS_VGA_CRTC_INDEX, 0x0C, (uint8_t)((offset >> 8) & 0xFF));
    sis661fx_rmw_idx(p, SIS_VGA_CRTC_INDEX, 0x1B, 0xF0, (uint8_t)((offset >> 16) & 0x0F));
}

static Bool SIS661FXSwitchMode(SWITCH_MODE_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    return sis661fx_set_mode(pScrn, mode);
}

static Bool SIS661FXEnterVT(VT_FUNC_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    if (!sis661fx_set_mode(pScrn, pScrn->currentMode))
        return FALSE;
    return TRUE;
}

static void SIS661FXLeaveVT(VT_FUNC_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    sis661fx_save(pScrn);
}

static Bool SIS661FXMapMem(ScrnInfoPtr pScrn)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    int err;
    struct pci_device *pci = p->pci;

    /* BAR0 = framebuffer, BAR1 = MMIO (standard SiS layout) */
    p->fbPhys = pci->regions[0].base_addr;
    p->fbSize = pci->regions[0].size;
    p->mmioPhys = pci->regions[1].base_addr;
    p->mmioSize = pci->regions[1].size;

    err = pci_device_map_range(pci, p->fbPhys, p->fbSize,
                               PCI_DEV_MAP_FLAG_WRITABLE | PCI_DEV_MAP_FLAG_WRITE_COMBINE,
                               (void **)&p->fbBase);
    if (err) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to map framebuffer: %s\n", strerror(err));
        return FALSE;
    }
    err = pci_device_map_range(pci, p->mmioPhys, p->mmioSize,
                               PCI_DEV_MAP_FLAG_WRITABLE,
                               (void **)&p->mmioBase);
    if (err) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to map MMIO: %s\n", strerror(err));
        pci_device_unmap_range(pci, p->fbBase, p->fbSize);
        p->fbBase = NULL;
        return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Mapped FB 0x%lx/0x%lx, MMIO 0x%lx/0x%lx\n",
               p->fbPhys, p->fbSize, p->mmioPhys, p->mmioSize);
    return TRUE;
}

static void SIS661FXUnmapMem(ScrnInfoPtr pScrn)
{
    Sis661fxPtr p = SIS661FX(pScrn);
    if (p->mmioBase) { pci_device_unmap_range(p->pci, p->mmioBase, p->mmioSize); p->mmioBase = NULL; }
    if (p->fbBase)   { pci_device_unmap_range(p->pci, p->fbBase, p->fbSize); p->fbBase = NULL; }
}

static Bool SIS661FXPreInit(ScrnInfoPtr pScrn, int flags)
{
    Sis661fxPtr p;
    ClockRangePtr clockRanges;
    MessageType from;
    int maxClock = 230000; /* conservative for 661FX */

    if (pScrn->numEntities != 1) return FALSE;
    if (flags & PROBE_DETECT) return FALSE;

    if (!SIS661FXGetRec(pScrn)) return FALSE;
    p = SIS661FX(pScrn);
    p->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    p->pci = xf86GetPciInfoForEntity(p->pEnt->index);
    if (!p->pci) return FALSE;

    pScrn->monitor = pScrn->confScreen->monitor;
    pScrn->progClock = TRUE;
    pScrn->rgbBits = 8;

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support24bppFb | Support32bppFb))
        return FALSE;
    if (pScrn->depth != 16 && pScrn->depth != 24 && pScrn->depth != 8) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Unsupported depth %d\n", pScrn->depth);
        return FALSE;
    }
    xf86PrintDepthBpp(pScrn);

    if (!xf86SetDefaultVisual(pScrn, -1)) return FALSE;

    xf86CollectOptions(pScrn, NULL);
    if (!(p->Options = malloc(sizeof(SIS661FXOptions))))
        return FALSE;
    memcpy(p->Options, SIS661FXOptions, sizeof(SIS661FXOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, p->Options);
    p->noAccel = xf86ReturnOptValBool(p->Options, OPTION_NOACCEL, FALSE);

    if (xf86LoadSubModule(pScrn, "fb") == NULL) return FALSE;
    if (!p->noAccel) xf86LoadSubModule(pScrn, "exa");

    if (!SIS661FXMapMem(pScrn)) return FALSE;
    sis661fx_unlock(p);
    sis661fx_save(pScrn);
    if (!sis661fx_mode_preinit(pScrn)) return FALSE;

    pScrn->videoRam = p->videoRamKB;

    clockRanges = XNFcallocarray(1, sizeof(ClockRange));
    clockRanges->next = NULL;
    clockRanges->minClock = 16250;
    clockRanges->maxClock = maxClock;
    clockRanges->clockIndex = -1;
    clockRanges->interlaceAllowed = FALSE;
    clockRanges->doubleScanAllowed = FALSE;

    {
        DisplayModePtr modes = xf86GetDefaultModes();
        if (pScrn->monitor->Modes)
            modes = xf86ModesAdd(modes, pScrn->monitor->Modes);
        {
            int n = xf86ValidateModes(pScrn, modes, pScrn->display->modes,
                                         clockRanges, NULL, 256, 2048,
                                         8 * pScrn->bitsPerPixel, 128, 2048,
                                         pScrn->display->virtualX, pScrn->display->virtualY,
                                         p->videoRamKB * 1024, LOOKUP_BEST_REFRESH);
            (void)n;
        }
        if (!pScrn->modes) {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes\n");
            return FALSE;
        }
        xf86PruneDriverModes(pScrn);
        if (!pScrn->modes) return FALSE;
        pScrn->currentMode = pScrn->modes;
        xf86SetCrtcForModes(pScrn, 0);
    }
    pScrn->displayWidth = ((pScrn->virtualX * pScrn->bitsPerPixel / 8) + 15) & ~15;
    pScrn->displayWidth /= (pScrn->bitsPerPixel / 8);

    xf86PrintModes(pScrn);
    {
        Gamma zeros = {0.0f, 0.0f, 0.0f};
        if (!xf86SetGamma(pScrn, zeros)) return FALSE;
    }
    if (pScrn->chipset == NULL)
        pScrn->chipset = "SiS 661FX (direct MMIO)";

    p->chipId = p->pci->device_id;
    p->revision = (uint8_t)p->pci->revision;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using direct MMIO driver for SiS 661FX\n");
    (void)from; (void)clockRanges;
    return TRUE;
}

static Bool SIS661FXCreateScreenResources(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    Bool ret;

    pScreen->CreateScreenResources = p->CreateScreenResources;
    ret = pScreen->CreateScreenResources(pScreen);
    pScreen->CreateScreenResources = SIS661FXCreateScreenResources;
    return ret;
}

static Bool SIS661FXCloseScreen(CLOSE_SCREEN_ARGS_DECL)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);

    if (pScrn->vtSema)
        sis661fx_restore(pScrn);

    SIS661FXUnmapMem(pScrn);

    pScrn->vtSema = FALSE;
    pScreen->CloseScreen = p->CloseScreen;
    pScreen->CreateScreenResources = p->CreateScreenResources;
    return (*pScreen->CloseScreen)(CLOSE_SCREEN_ARGS);
}

static Bool SIS661FXScreenInit(SCREEN_INIT_ARGS_DECL)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    Sis661fxPtr p = SIS661FX(pScrn);
    VisualPtr visual;
    int fbPitch;
    BoxRec AvailFBArea;

    if (!sis661fx_set_mode(pScrn, pScrn->currentMode))
        return FALSE;

    miClearVisualTypes();
    if (!miSetVisualTypes(pScrn->depth, miGetDefaultVisualMask(pScrn->depth),
                          pScrn->rgbBits, pScrn->defaultVisual))
        return FALSE;
    if (!miSetPixmapDepths()) return FALSE;

    fbPitch = p->pitch / (pScrn->bitsPerPixel / 8);
    pScrn->displayWidth = fbPitch;

    if (!fbScreenInit(pScreen, p->fbBase,
                      pScrn->virtualX, pScrn->virtualY,
                      pScrn->xDpi, pScrn->yDpi,
                      fbPitch, pScrn->bitsPerPixel))
        return FALSE;

    if (pScrn->bitsPerPixel > 8) {
        visual = pScreen->visuals + pScreen->numVisuals;
        while (--visual >= pScreen->visuals) {
            if ((visual->class | DynamicClass) == DirectColor) {
                visual->offsetRed = pScrn->offset.red;
                visual->offsetGreen = pScrn->offset.green;
                visual->offsetBlue = pScrn->offset.blue;
                visual->redMask = pScrn->mask.red;
                visual->greenMask = pScrn->mask.green;
                visual->blueMask = pScrn->mask.blue;
            }
        }
    }

    fbPictureInit(pScreen, NULL, 0);
    xf86SetBlackWhitePixels(pScreen);
    sis661fx_accel_init(pScreen);
    sis661fx_cursor_init(pScreen);
    xf86DPMSInit(pScreen, xf86DPMSSet, 0);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    if (!miCreateDefColormap(pScreen)) return FALSE;


    p->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = SIS661FXCloseScreen;
    p->CreateScreenResources = pScreen->CreateScreenResources;
    pScreen->CreateScreenResources = SIS661FXCreateScreenResources;

    if (!xf86HandleColormaps(pScreen, 256, 8, NULL, NULL, CMAP_PALETTED_TRUECOLOR))
        return FALSE;

    pScreen->SaveScreen = xf86SaveScreen;
    p->EnterVT = pScrn->EnterVT;
    p->LeaveVT = pScrn->LeaveVT;
    pScrn->EnterVT = SIS661FXEnterVT;
    pScrn->LeaveVT = SIS661FXLeaveVT;

    AvailFBArea.x1 = 0; AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = p->videoRamKB * 1024 / p->pitch;
    xf86InitFBManager(pScreen, &AvailFBArea);

    if (serverGeneration == 1)
        xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

    pScrn->vtSema = TRUE;
    return TRUE;
}

/* ---- Xorg module glue ---- */

static pointer SIS661FXSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;
    if (!setupDone) {
        setupDone = TRUE;
        xf86AddDriver(&SIS661FX, module, HaveDriverFuncs);
        return (pointer)1;
    }
    if (errmaj) *errmaj = LDR_ONCEONLY;
    return NULL;
}

static XF86ModuleVersionInfo SIS661FXVersRec = {
    SIS661FX_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    0, 1, 0,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0,0,0,0}
};

_X_EXPORT XF86ModuleData sis661fxModuleData = {
    &SIS661FXVersRec,
    SIS661FXSetup,
    NULL
};
