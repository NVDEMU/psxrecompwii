#ifndef PSXRECOMP_WII_INPUT_H
#define PSXRECOMP_WII_INPUT_H

/*
 * Wii/vWii input adapter for PSXRecomp.
 *
 * This is an isolated platform module; it is not yet wired into the desktop
 * runtime target or the PS1 pad event loop. Keep this API independent of
 * renderer/runtime internals so it can be tested with devkitPPC first.
 */

#include <gccore.h>
#include <wiiuse/wpad.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PsxWiiInput {
    s8 move_x;       /* Nunchuk horizontal axis, -127..127 */
    s8 move_y;       /* Nunchuk vertical axis, -127..127; positive is down */
    s8 aim_x;        /* IR pointer relative to screen center, -127..127 */
    s8 aim_y;        /* IR pointer relative to screen center, -127..127 */
    u16 buttons;     /* PS1-style logical button mask */
    bool has_nunchuk;
    bool has_ir;
} PsxWiiInput;

enum PsxWiiButtons {
    PSX_WII_SELECT   = 0x0001,
    PSX_WII_START    = 0x0008,
    PSX_WII_UP       = 0x0010,
    PSX_WII_RIGHT    = 0x0020,
    PSX_WII_DOWN     = 0x0040,
    PSX_WII_LEFT     = 0x0080,
    PSX_WII_L1       = 0x0400,
    PSX_WII_R1       = 0x0800,
    PSX_WII_TRIANGLE = 0x1000,
    PSX_WII_CIRCLE   = 0x2000,
    PSX_WII_CROSS    = 0x4000,
    PSX_WII_SQUARE   = 0x8000
};

/*
 * Convert a sampled libogc controller state into a portable logical snapshot.
 * Call WPAD_SetDataFormat(channel, WPAD_FMT_BTNS_ACC_IR) and
 * WPAD_SetVRes(channel, 640, 480) during controller initialization.
 */
void psx_wii_read_input(const WPADData *data, u32 buttons_held,
                        PsxWiiInput *out);

#ifdef __cplusplus
}
#endif

#endif
