#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SYSROOT="${SYSROOT:-/opt/sysroot-i386}"
OUT="$HERE/dist-i686"
mkdir -p "$OUT"

export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/i386-linux-gnu/pkgconfig:$SYSROOT/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"

CFLAGS="-m32 -fPIC -O2 -Wall -Wextra -Wno-unused-parameter -I$HERE -I$HERE/src -I$SYSROOT/usr/include -I$SYSROOT/usr/include/xorg -I$SYSROOT/usr/include/pixman-1 -I$SYSROOT/usr/include/libdrm $(pkg-config --cflags xorg-server xproto fontsproto pciaccess) -DHAVE_CONFIG_H -include $HERE/config.h"
LDFLAGS="-m32 -shared -fPIC -Wl,-soname,sis661fx_drv.so"

SRCS="src/sis661fx_driver.c src/sis661fx_mmio.c src/sis661fx_mode.c src/sis661fx_accel.c src/sis661fx_cursor.c"
OBJS=""
for s in $SRCS; do
  o="$OUT/$(basename ${s%.c}.o)"
  echo "CC $s"
  gcc $CFLAGS -c "$HERE/$s" -o "$o"
  OBJS="$OBJS $o"
done
echo "LD sis661fx_drv.so"
gcc $LDFLAGS -o "$OUT/sis661fx_drv.so" $OBJS
file "$OUT/sis661fx_drv.so"
