#include "sis661fx.h"
#include <unistd.h>
#include <sys/io.h>

/* Legacy VGA I/O. The X server normally has I/O privilege in PreInit. */
uint8_t sis661fx_inb(Sis661fxPtr p, uint16_t port)
{
    (void)p;
    return inb(port);
}

void sis661fx_outb(Sis661fxPtr p, uint16_t port, uint8_t val)
{
    (void)p;
    outb(val, port);
}

uint8_t sis661fx_read_idx(Sis661fxPtr p, uint16_t base, uint8_t idx)
{
    sis661fx_outb(p, base, idx);
    return sis661fx_inb(p, base + 1);
}

void sis661fx_write_idx(Sis661fxPtr p, uint16_t base, uint8_t idx, uint8_t val)
{
    sis661fx_outb(p, base, idx);
    sis661fx_outb(p, base + 1, val);
}

void sis661fx_rmw_idx(Sis661fxPtr p, uint16_t base, uint8_t idx, uint8_t mask, uint8_t val)
{
    uint8_t v = sis661fx_read_idx(p, base, idx);
    v = (v & mask) | val;
    sis661fx_write_idx(p, base, idx, v);
}

uint32_t sis661fx_mmio_read32(Sis661fxPtr p, uint32_t off)
{
    volatile uint32_t *addr = (volatile uint32_t *)(p->mmioBase + off);
    return *addr;
}

void sis661fx_mmio_write32(Sis661fxPtr p, uint32_t off, uint32_t val)
{
    volatile uint32_t *addr = (volatile uint32_t *)(p->mmioBase + off);
    *addr = val;
}

void sis661fx_mmio_write16(Sis661fxPtr p, uint32_t off, uint16_t val)
{
    volatile uint16_t *addr = (volatile uint16_t *)(p->mmioBase + off);
    *addr = val;
}
