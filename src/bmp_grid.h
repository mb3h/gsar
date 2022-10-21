#ifndef BMP_GRID_H_INCLUDED__
#define BMP_GRID_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum bmp_direction {
	LEFT = 0
};

struct bmp_grid {
#ifdef __x86_64__
	uint8_t priv[16]; // gcc's value to x86_64
#else
	uint8_t priv[12]; // gcc's value to i386
#endif
};
void bmp_grid_pset (struct bmp_grid *this_, uint16_t x, uint16_t y, uint32_t rgb24);
void bmp_grid_line (struct bmp_grid *this_, uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint32_t rgb24);
void bmp_grid_scroll (struct bmp_grid *this_, enum bmp_direction pd, uint16_t px);
uint8_t *bmp_grid_get (struct bmp_grid *this_, enum pixel_format pf);
void bmp_grid_dtor (struct bmp_grid *this_);
void bmp_grid_ctor (struct bmp_grid *this_, unsigned cb, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // BMP_GRID_H_INCLUDED__
