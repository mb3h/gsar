#include <stdint.h>
#include "common/pixel.h"

#include "portab.h"
#include "vt100.h"
#include "assert.h"

#include <stdio.h> // printf
#include <stdlib.h> // exit
typedef uint32_t u24;
typedef  uint8_t u8;
typedef uint16_t u16;

#define ALIGN32(x) (-32 & (x) +32 -1)
///////////////////////////////////////////////////////////////

unsigned bmp_magnify (const void *src_, u8 bpp, u16 width, u16 height, u8 mag, void *dst_, unsigned cb)
{
BUG(24 == bpp && 0 < width && 0 < height && 0 < mag)
unsigned cb_need; u16 bpl;
	cb_need = (bpl = ALIGN32(width * mag * 24) / 8) * (height * mag);
	if (!dst_)
		return cb_need;
BUG(src_ && cb_need <= cb)
	if (! (src_ && cb_need <= cb))
		return 0;
		
const u8 *src; u8 *dst;
	src = (const u8 *)src_, dst = (u8 *)dst_;
u16 src_bpl;
	src_bpl = ALIGN32(width * 24) / 8;

u8 *p; u16 y, x;
	for (y = 0; y < height; ++y) {
		p = dst + (y * mag) * bpl;
		for (x = 0; x < width; ++x) {
u24 rgb24; u16 yy, xx;
			rgb24 = load_le24 (src + y * src_bpl + x * 3);
			for (yy = 0; yy < mag; ++yy)
				for (xx = 0; xx < mag; ++xx)
					store_le24 (p + yy * bpl + xx * 3, rgb24);
			p += 3 * mag;
		}
	}
	return cb_need;
}
