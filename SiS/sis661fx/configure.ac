AC_PREREQ([2.60])
AC_INIT([xf86-video-sis661fx],[0.1.0],[https://github.com/], [xf86-video-sis661fx])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR(.)
AM_INIT_AUTOMAKE([foreign dist-xz])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_SYS_LARGEFILE

m4_ifndef([XORG_MACROS_VERSION],
          [m4_fatal([must install xorg-macros 1.8 or later])])
XORG_MACROS_VERSION(1.8)
XORG_DEFAULT_OPTIONS

PKG_CHECK_MODULES(XORG, [xorg-server >= 1.18 xproto fontsproto xf86dgaproto >= 2.1 pciaccess])
PKG_CHECK_MODULES(XEXT, [xextproto >= 7.0.99.1],
                  [AC_DEFINE(HAVE_XEXTPROTO_71,1,[xextproto 7.1])],[])

moduledir=`$PKG_CONFIG --variable=moduledir xorg-server`
AC_SUBST([moduledir])

DRIVER_NAME=sis661fx
AC_SUBST([DRIVER_NAME])

AC_CONFIG_FILES([
    Makefile
    src/Makefile
])
AC_OUTPUT
