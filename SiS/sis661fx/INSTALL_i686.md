# نصب درایور SiS 661FX روی سیستم 32-bit (i686)

هدف نمونه: Intel Celeron 2.40GHz، هستهٔ تک، معماری i686، Xorg 1.18+ (تست‌شده با 21.1).

## 1) پیش‌نیازها
روی سیستم مقصد:
```sh
sudo apt-get install xserver-xorg-core xserver-xorg-video-fbdev pciutils
# یا دیستروهای دیگر معادل xorg-server و pciutils
```
فایل باینری: `dist-i686/sis661fx_drv.so`

## 2) نصب ماژول
```sh
# مسیر ماژول را پیدا کنید
MODDIR=$(pkg-config --variable=moduledir xorg-server 2>/dev/null || echo /usr/lib/xorg/modules)
sudo install -Dm755 dist-i686/sis661fx_drv.so "$MODDIR/drivers/sis661fx_drv.so"
```

## 3) پیکربندی Xorg
فایل `/etc/X11/xorg.conf.d/20-sis661fx.conf` بسازید:
```conf
Section "Device"
    Identifier "SiS 661FX"
    Driver     "sis661fx"
    Option     "NoAccel" "false"
EndSection

Section "Screen"
    Identifier "Screen0"
    Device     "SiS 661FX"
    DefaultDepth 24
    SubSection "Display"
        Depth 24
        Modes "1024x768" "800x600"
    EndSubSection
EndSection
```

## 4) تشخیص کارت
```sh
lspci -nn | grep -i '1039:6330'
```
اگر دستگاه دیده شد، ری‌استارت سرویس گرافیکی/ریبوت کنید.

## 5) عیب‌یابی
- لاگ: `/var/log/Xorg.0.log`
- اگر صفحه سیاه شد: `Option "NoAccel" "true"` و رزولوشن پایین‌تر.
- اگر درایور لود نشد: ABI را چک کنید؛ ماژول باید با همان نسخهٔ Xorg مقصد ساخته شود.
- برای ساخت محلی روی خود سیستم 32-bit:
  ```sh
  sudo apt-get install build-essential xserver-xorg-dev pkg-config libpciaccess-dev libdrm-dev
  autoreconf -fi && ./configure --prefix=/usr && make -j1
  sudo make install
  ```

## 6) ساخت باینری i686 روی میزبان 64-bit
در این مخزن اسکریپت `build-i686.sh` وجود دارد که با sysroot i386 و `gcc -m32` کامپایل می‌کند:
```sh
cd Linux/sis661fx
./build-i686.sh
# خروجی: dist-i686/sis661fx_drv.so
```
