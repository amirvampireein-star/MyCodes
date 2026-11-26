# sis661fx - direct MMIO Xorg driver

New, from-scratch Xorg DDX for SiS 661FX (PCI 1039:6330), targeting Xorg >= 1.18 (tested 21.1.22).

## Design
- No code copied from xf86-video-sis or vendor binaries.
- Direct BAR0/BAR1 mapping via pciaccess.
- Legacy VGA/SR/CR programming through I/O ports 0x3C0-0x3DA.
- SiS extension unlock (SR05=0x86), CR5C/CR5F chip identification.
- VCLK programming via SR2B/SR2C/SR2D.
- 2D engine MMIO initialized at BAR1 + 0x8200 (from RE of sisgrp.sys).

## Build
```sh
autoreconf -fi
./configure --prefix=/usr
make -j$(nproc)
sudo install -m755 src/.libs/sis661fx_drv.so /usr/lib/xorg/modules/drivers/
```

## Verify load (no hardware required)
```sh
Xorg -config /path/to/xorg.conf -logfile /tmp/x.log
# should show: (II) Module sis661fx: ... ABI class: X.Org Video Driver, version 25.2
```

## Status
Compiles and loads in Xorg 21.1.22. Functional mode-setting on real 661FX hardware requires hardware validation/tuning of the CRTC/PLL/extended register sequence.
