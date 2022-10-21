#include <stdint.h>

#include "common/pixel.h"
#include "bmp_grid.h"
#include "portab.h"
#include "vt100.h"
#include "assert.h"

#include <stdio.h> // printf
#include <stdlib.h> // exit
#include <memory.h>

typedef uint32_t u32, u24;
typedef  uint8_t u8;
typedef uint16_t u16;

#define min(a, b) ((a) < (b) ? (a) : (b))
#define ALIGN32(x) (-32 & (x) +32 -1)
///////////////////////////////////////////////////////////////

struct bmp_grid_ {
	u8 *rgb24be;
	u16 width;
	u16 height;
	u8 x_grid, x_grid0;
	u8 y_grid, y_grid0;
};
typedef struct bmp_grid_ bmp_grid_s;

static void bmp_grid_clear (struct bmp_grid *this_, u16 left, u16 top, u16 width, u16 height)
{
bmp_grid_s *m_;
	m_ = (bmp_grid_s *)this_;
BUG(0 == top && m_->height == height)
BUG(left < m_->width && top < m_->height && 0 < width && 0 < height)

u16 BPL;
	BPL = ALIGN32(m_->width * 24) / 8;
u16 y, x; u8 *start; u24 bgcl;
	for (y = 0; y < m_->height; ++y) {
		start = m_->rgb24be + y * BPL + left * 3;
		if (m_->y_grid0 % m_->y_grid == y % m_->y_grid) {
			bgcl = 0x008040; // RGB(0,128,64)
			for (x = 0; x < width; ++x)
				store_be24 (start +x * 3, bgcl);
			continue;
		}
		for (x = 0; x < width; ++x) {
			bgcl = (m_->x_grid0 % m_->x_grid == (left +x) % m_->x_grid) ? 0x008040 : 0; // RGB(0,128,64) RGB(0,0,0)
			store_be24 (start +x * 3, bgcl);
		}
	}
}

void bmp_grid_pset (struct bmp_grid *this_, u16 x, u16 y, u24 rgb24)
{
bmp_grid_s *m_;
	m_ = (bmp_grid_s *)this_;

	if (! (x < m_->width && y < m_->height))
		return;
u8 *left; u16 BPL;
	left = m_->rgb24be + y * (BPL = ALIGN32(m_->width * 24) / 8);
	store_be24 (left + x * 3, rgb24);
}

void bmp_grid_line (struct bmp_grid *this_, u16 x, u16 y, u16 dx, u16 dy, u24 rgb24)
{
bmp_grid_s *m_;
	m_ = (bmp_grid_s *)this_;
	if (! (x < m_->width && y < m_->height && dx < m_->width && dy < m_->height))
		return;

u32 nx, ny;
	nx = (x < dx) ? dx - x : x - dx;
	ny = (y < dy) ? dy - y : y - dy;
u16 px, py, i, delta;
	if (ny < nx)
		for (i = 1; i <= nx; ++i) {
			delta = (u16)(i * ny / nx);
			px = (x < dx) ? x +i : x -i;
			py = (y < dy) ? y +delta : y -delta;
			bmp_grid_pset (this_, px, py, rgb24);
		}
	else
		for (i = 1; i <= ny; ++i) {
			delta = (u16)(i * nx / ny);
			px = (x < dx) ? x +delta : x -delta;
			py = (y < dy) ? y +i : y -i;
			bmp_grid_pset (this_, px, py, rgb24);
		}
}

void bmp_grid_scroll (struct bmp_grid *this_, enum bmp_direction pd, u16 px)
{
bmp_grid_s *m_;
	m_ = (bmp_grid_s *)this_;
BUG(LEFT == pd && 0 < px)

u16 BPL, stay_left;
	BPL = ALIGN32(m_->width * 24) / 8;
	if ((stay_left = min(m_->width, px)) < m_->width) {
u16 y; u8 *left;
		for (y = 0; y < m_->height; ++y) {
			left = m_->rgb24be + y * BPL;
			memmove (left, left + stay_left * 3, (m_->width - stay_left) * 3);
		}
	}
unsigned delta;
	delta = px % m_->x_grid;
	m_->x_grid0 = (m_->x_grid0 + m_->x_grid - delta) % m_->x_grid;
	bmp_grid_clear (this_, m_->width - stay_left, 0, stay_left, m_->height);
}

u8 *bmp_grid_get (struct bmp_grid *this_, enum pixel_format pf)
{
bmp_grid_s *m_;
	m_ = (bmp_grid_s *)this_;
BUG(PF_RGB888 == pf)
	return m_->rgb24be;
}

void bmp_grid_dtor (struct bmp_grid *this_)
{
bmp_grid_s *m_;
	m_ = (bmp_grid_s *)this_;
BUG(m_)
	if (m_->rgb24be) {
		free (m_->rgb24be);
		m_->rgb24be = NULL;
	}
}

void bmp_grid_ctor (struct bmp_grid *this_, unsigned cb, u16 width, u16 height)
{
ASSERT2(sizeof(bmp_grid_s) <= cb, " %d <= " VTRR "%d" VTO, (unsigned)sizeof(bmp_grid_s), cb)
bmp_grid_s *m_;
	m_ = (bmp_grid_s *)this_;
	memset (m_, 0, sizeof(bmp_grid_s));
u16 BPL;
	BPL = ALIGN32(width * 24) / 8;
	m_->rgb24be = (u8 *)malloc (BPL * height);
	m_->width = width;
	m_->height = height;
	m_->x_grid = m_->y_grid = 12;
	bmp_grid_clear (this_, 0, 0, width, height);
}
