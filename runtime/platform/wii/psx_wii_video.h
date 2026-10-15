#ifndef PSXRECOMP_WII_VIDEO_H
#define PSXRECOMP_WII_VIDEO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal Wii GX presenter for 32-bit ARGB frames.
 * This is a platform primitive, not yet connected to PSXRecomp's main loop.
 *
 * init: initialize libogc video and GX.
 * present: upload one ARGB source frame and display it. pitch_bytes is the
 * source row stride in bytes. Current implementation supports up to 1024x576.
 * shutdown: release buffers owned by the presenter.
 */
int psx_wii_video_init(void);
int psx_wii_video_present_argb(const uint32_t *pixels,
                               uint32_t width,
                               uint32_t height,
                               uint32_t pitch_bytes);
void psx_wii_video_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
