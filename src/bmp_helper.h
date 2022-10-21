#ifndef BMP_HELPER_H_INCLUDED__
#define BMP_HELPER_H_INCLUDED__

#define sizeofBITMAPFILEHEADER 0x0E
#define sizeofBITMAPINFOHEADER 0x28
#define sizeofBMPHDR (sizeofBITMAPFILEHEADER + sizeofBITMAPINFOHEADER)

unsigned bmp_magnify (const void *src, uint8_t bpp, uint16_t width, uint16_t height, uint8_t mag, void *dst, unsigned cb);

#endif // BMP_HELPER_H_INCLUDED__
