#ifndef PSXRECOMP_WII_GP0_H
#define PSXRECOMP_WII_GP0_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal PS1 GP0 command adapter for Wii bring-up.
 *
 * This accepts one 32-bit word at a time, just like GP0 writes at 0x1F801810.
 * Supported packet families currently include:
 *   02h fill rectangle; 20h flat triangle; 28h flat quad; 30h Gouraud triangle;
 *   38h Gouraud quad; 40h flat line; 50h Gouraud line;
 *   60h variable flat rectangle; 68h 1x1; 70h 8x8; 78h 16x16;
 *   E3h/E4h draw area; E5h draw offset; E6h mask settings.
 *
 * Textured primitives, polylines, DMA and GPUSTAT/GPUREAD are not implemented
 * here. This is a first integration layer for native-width rendering, not a
 * replacement for the full PS1 GPU hardware model in runtime/src/gpu.c.
 */
void psx_wii_gp0_init(uint16_t *vram);
void psx_wii_gp0_reset(void);
void psx_wii_gp0_write(uint32_t word);
int psx_wii_gp0_busy(void);

#ifdef __cplusplus
}
#endif

#endif
