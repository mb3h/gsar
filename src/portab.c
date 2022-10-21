#include <endian.h>

#include <stdint.h>
#include "portab.h"

typedef uint32_t u32;
typedef  uint8_t u8;

u32 load_le24 (const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return 0xffffff & *(const u32 *)src;
#else
const u8 *p;
	p = (const u8 *)src;
	return p[0]<<16|p[1]<<8|p[2];
#endif
}

unsigned store_be24 (void *dst, u32 val)
{
u8 *p;
	p = (u8 *)dst;
	*p++ = (u8)(0xff & val >> 16);
	*p++ = (u8)(0xff & val >> 8);
	*p++ = (u8)(0xff & val);
	return 3;
}

unsigned store_le24 (void *dst, u32 val)
{
u8 *p;
	p = (u8 *)dst;
	*(p +0) = (u8)(0xff & val);
	*(p +1) = (u8)(0xff & val >> 8);
	*(p +2) = (u8)(0xff & val >> 16);
	return 3;
}
