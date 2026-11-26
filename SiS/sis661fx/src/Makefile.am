AM_CFLAGS = $(XORG_CFLAGS) $(XEXT_CFLAGS) -Wall -Wextra -Wno-unused-parameter
sis661fx_drv_la_LTLIBRARIES = sis661fx_drv.la
sis661fx_drv_la_LDFLAGS = -module -avoid-version
sis661fx_drv_ladir = @moduledir@/drivers
sis661fx_drv_la_SOURCES = \
	sis661fx.h \
	sis661fx_regs.h \
	sis661fx_driver.c \
	sis661fx_mmio.c \
	sis661fx_mode.c \
	sis661fx_accel.c \
	sis661fx_cursor.c
