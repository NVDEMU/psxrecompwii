#include "psx_wii_input.h"

#include <string.h>

#define PSX_WII_IR_WIDTH  640.0f
#define PSX_WII_IR_HEIGHT 480.0f
#define PSX_WII_AXIS_DEADZONE 9

static s8 psx_wii_scale_axis(int value, int minimum, int center, int maximum,
                             int invert)
{
    int span;
    int delta;
    int scaled;

    if (value >= center) {
        span = maximum - center;
        delta = value - center;
    } else {
        span = center - minimum;
        delta = value - center;
    }

    if (span <= 0) {
        return 0;
    }

    scaled = (delta * 127) / span;
    if (scaled > 127) scaled = 127;
    if (scaled < -127) scaled = -127;
    if (scaled > -PSX_WII_AXIS_DEADZONE &&
        scaled < PSX_WII_AXIS_DEADZONE) {
        scaled = 0;
    }
    if (invert) {
        scaled = -scaled;
    }
    return (s8)scaled;
}

static s8 psx_wii_scale_pointer(float coordinate, float extent)
{
    float normalized;
    int scaled;

    if (extent <= 0.0f) {
        return 0;
    }

    normalized = (coordinate - (extent * 0.5f)) / (extent * 0.5f);
    if (normalized > 1.0f) normalized = 1.0f;
    if (normalized < -1.0f) normalized = -1.0f;

    scaled = (int)(normalized * 127.0f);
    if (scaled > -PSX_WII_AXIS_DEADZONE &&
        scaled < PSX_WII_AXIS_DEADZONE) {
        scaled = 0;
    }
    return (s8)scaled;
}

void psx_wii_read_input(const WPADData *data, u32 held, PsxWiiInput *out)
{
    const struct ir_t *ir;

    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));

    if (held & WPAD_BUTTON_MINUS) out->buttons |= PSX_WII_SELECT;
    if (held & WPAD_BUTTON_PLUS) out->buttons |= PSX_WII_START;
    if (held & WPAD_BUTTON_UP) out->buttons |= PSX_WII_UP;
    if (held & WPAD_BUTTON_RIGHT) out->buttons |= PSX_WII_RIGHT;
    if (held & WPAD_BUTTON_DOWN) out->buttons |= PSX_WII_DOWN;
    if (held & WPAD_BUTTON_LEFT) out->buttons |= PSX_WII_LEFT;

    /* Provisional defaults; verify button choice during live gameplay testing. */
    if (held & WPAD_BUTTON_A) out->buttons |= PSX_WII_CROSS;
    if (held & WPAD_BUTTON_B) out->buttons |= PSX_WII_CIRCLE;
    if (held & WPAD_BUTTON_1) out->buttons |= PSX_WII_SQUARE;
    if (held & WPAD_BUTTON_2) out->buttons |= PSX_WII_TRIANGLE;
    if (held & WPAD_NUNCHUK_BUTTON_C) out->buttons |= PSX_WII_L1;
    if (held & WPAD_NUNCHUK_BUTTON_Z) out->buttons |= PSX_WII_R1;

    if (data == NULL || data->err != WPAD_ERR_NONE) {
        return;
    }

    if ((data->data_present & WPAD_DATA_EXPANSION) &&
        data->exp.type == WPAD_EXP_NUNCHUK) {
        const joystick_t *js = &data->exp.nunchuk.js;
        out->has_nunchuk = true;
        out->move_x = psx_wii_scale_axis(js->pos.x, js->min.x, js->center.x,
                                         js->max.x, 0);
        /* PS1-facing +Y is down; Nunchuk's natural positive Y is up. */
        out->move_y = psx_wii_scale_axis(js->pos.y, js->min.y, js->center.y,
                                         js->max.y, 1);
    }

    ir = &data->ir;
    if ((data->data_present & WPAD_DATA_IR) && ir->valid) {
        out->has_ir = true;
        out->aim_x = psx_wii_scale_pointer(ir->x, PSX_WII_IR_WIDTH);
        out->aim_y = psx_wii_scale_pointer(ir->y, PSX_WII_IR_HEIGHT);
    }
}
