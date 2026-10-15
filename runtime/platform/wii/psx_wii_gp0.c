#include "psx_wii_gp0.h"

#include "gpu_sw_renderer.h"

#include <string.h>

#define PSX_WII_VRAM_WIDTH  1024
#define PSX_WII_VRAM_HEIGHT 512
#define PSX_WII_GP0_MAX_WORDS 8

static uint16_t *s_vram;
static uint32_t s_packet[PSX_WII_GP0_MAX_WORDS];
static int s_collected;
static int s_required;
static int s_area_x1, s_area_y1, s_area_x2, s_area_y2;

static int sign_extend_11(uint32_t value)
{
    value &= 0x7ffu;
    return (value & 0x400u) ? (int)value - 0x800 : (int)value;
}

static int packet_x(uint32_t value)
{
    return sign_extend_11(value);
}

static int packet_y(uint32_t value)
{
    return sign_extend_11(value >> 16);
}

static uint16_t rgb888_to_rgb555(uint32_t word)
{
    const unsigned int r = word & 0xffu;
    const unsigned int g = (word >> 8) & 0xffu;
    const unsigned int b = (word >> 16) & 0xffu;
    return (uint16_t)(((r >> 3) & 31u) |
                      (((g >> 3) & 31u) << 5) |
                      (((b >> 3) & 31u) << 10));
}

static int packet_length(uint32_t word)
{
    const unsigned int op = (word >> 24) & 0xfcu;

    switch (op) {
    case 0x00: /* NOP */
    case 0xe1: /* Draw mode */
    case 0xe2: /* Texture window */
    case 0xe3: /* Drawing area top-left */
    case 0xe4: /* Drawing area bottom-right */
    case 0xe5: /* Drawing offset */
    case 0xe6: /* Mask bit control */
        return 1;
    case 0x02: /* Fill rectangle in VRAM */
    case 0x60: /* Flat rectangle, variable size */
    case 0x40: /* Flat line */
        return 3;
    case 0x20: /* Flat triangle */
        return 4;
    case 0x28: /* Flat quad */
        return 5;
    case 0x30: /* Gouraud triangle */
        return 6;
    case 0x38: /* Gouraud quad */
        return 8;
    case 0x50: /* Gouraud line */
        return 4;
    case 0x68: /* 1x1 flat rectangle */
    case 0x70: /* 8x8 flat rectangle */
    case 0x78: /* 16x16 flat rectangle */
        return 2;
    default:
        return 0;
    }
}

static void set_draw_area_top_left(uint32_t word)
{
    s_area_x1 = (int)(word & 0x3ffu);
    s_area_y1 = (int)((word >> 10) & 0x1ffu);
    sw_set_draw_area(s_area_x1, s_area_y1, s_area_x2, s_area_y2);
}

static void set_draw_area_bottom_right(uint32_t word)
{
    s_area_x2 = (int)(word & 0x3ffu);
    s_area_y2 = (int)((word >> 10) & 0x1ffu);
    sw_set_draw_area(s_area_x1, s_area_y1, s_area_x2, s_area_y2);
}

static void execute_packet(void)
{
    const unsigned int op = (s_packet[0] >> 24) & 0xfcu;
    const uint16_t color = rgb888_to_rgb555(s_packet[0]);
    int x, y, w, h;

    switch (op) {
    case 0x00:
        return;
    case 0x02:
        x = (int)(s_packet[1] & 0x3ffu);
        y = (int)((s_packet[1] >> 16) & 0x1ffu);
        w = (int)(s_packet[2] & 0x3ffu);
        h = (int)((s_packet[2] >> 16) & 0x1ffu);
        /* PS1 fill-rectangle command aligns X and width to 16 pixels. */
        x &= ~15;
        w = (w + 15) & ~15;
        if (w > 0 && h > 0) sw_fill_rect(x, y, w, h, color);
        return;
    case 0x20:
        sw_draw_flat_triangle(packet_x(s_packet[1]), packet_y(s_packet[1]),
                              packet_x(s_packet[2]), packet_y(s_packet[2]),
                              packet_x(s_packet[3]), packet_y(s_packet[3]),
                              color);
        return;
    case 0x28: {
        const int x0 = packet_x(s_packet[1]), y0 = packet_y(s_packet[1]);
        const int x1 = packet_x(s_packet[2]), y1 = packet_y(s_packet[2]);
        const int x2 = packet_x(s_packet[3]), y2 = packet_y(s_packet[3]);
        const int x3 = packet_x(s_packet[4]), y3 = packet_y(s_packet[4]);
        sw_draw_flat_triangle(x0, y0, x1, y1, x2, y2, color);
        sw_draw_flat_triangle(x1, y1, x2, y2, x3, y3, color);
        return;
    }
    case 0x30:
        sw_draw_gouraud_triangle(
            packet_x(s_packet[1]), packet_y(s_packet[1]), color,
            packet_x(s_packet[3]), packet_y(s_packet[3]),
            rgb888_to_rgb555(s_packet[2]),
            packet_x(s_packet[5]), packet_y(s_packet[5]),
            rgb888_to_rgb555(s_packet[4]));
        return;
    case 0x38: {
        const int x0 = packet_x(s_packet[1]), y0 = packet_y(s_packet[1]);
        const int x1 = packet_x(s_packet[3]), y1 = packet_y(s_packet[3]);
        const int x2 = packet_x(s_packet[5]), y2 = packet_y(s_packet[5]);
        const int x3 = packet_x(s_packet[7]), y3 = packet_y(s_packet[7]);
        const uint16_t c1 = rgb888_to_rgb555(s_packet[2]);
        const uint16_t c2 = rgb888_to_rgb555(s_packet[4]);
        const uint16_t c3 = rgb888_to_rgb555(s_packet[6]);
        sw_draw_gouraud_triangle(x0, y0, color, x1, y1, c1, x2, y2, c2);
        sw_draw_gouraud_triangle(x1, y1, c1, x2, y2, c2, x3, y3, c3);
        return;
    }
    case 0x40:
        sw_draw_line(packet_x(s_packet[1]), packet_y(s_packet[1]),
                     packet_x(s_packet[2]), packet_y(s_packet[2]), color);
        return;
    case 0x50:
        sw_draw_shaded_line(packet_x(s_packet[1]), packet_y(s_packet[1]), color,
                            packet_x(s_packet[3]), packet_y(s_packet[3]),
                            rgb888_to_rgb555(s_packet[2]));
        return;
    case 0x60:
        x = packet_x(s_packet[1]);
        y = packet_y(s_packet[1]);
        w = (int)(s_packet[2] & 0x3ffu);
        h = (int)((s_packet[2] >> 16) & 0x1ffu);
        if (w > 0 && h > 0) sw_draw_flat_rect(x, y, w, h, color);
        return;
    case 0x68:
    case 0x70:
    case 0x78:
        x = packet_x(s_packet[1]);
        y = packet_y(s_packet[1]);
        w = (op == 0x68) ? 1 : ((op == 0x70) ? 8 : 16);
        h = w;
        sw_draw_flat_rect(x, y, w, h, color);
        return;
    case 0xe1:
        return; /* Draw mode is not needed by the initial untextured path. */
    case 0xe2:
        sw_set_texture_window(s_packet[0] & 0x00ffffffu);
        return;
    case 0xe3:
        set_draw_area_top_left(s_packet[0]);
        return;
    case 0xe4:
        set_draw_area_bottom_right(s_packet[0]);
        return;
    case 0xe5:
        sw_set_draw_offset(sign_extend_11(s_packet[0]),
                           sign_extend_11(s_packet[0] >> 11));
        return;
    case 0xe6:
        sw_set_mask_bits((s_packet[0] & 1u) != 0,
                         (s_packet[0] & 2u) != 0);
        return;
    default:
        return;
    }
}

void psx_wii_gp0_init(uint16_t *vram)
{
    s_vram = vram;
    s_collected = 0;
    s_required = 0;
    memset(s_packet, 0, sizeof(s_packet));
    s_area_x1 = 0;
    s_area_y1 = 0;
    s_area_x2 = PSX_WII_VRAM_WIDTH - 1;
    s_area_y2 = PSX_WII_VRAM_HEIGHT - 1;
    sw_renderer_init(vram);
    sw_renderer_set_scale(1);
    sw_set_draw_area(s_area_x1, s_area_y1, s_area_x2, s_area_y2);
    sw_set_draw_offset(0, 0);
    sw_set_mask_bits(0, 0);
    sw_set_semi_transparency(0, 0);
}

void psx_wii_gp0_reset(void)
{
    s_collected = 0;
    s_required = 0;
    memset(s_packet, 0, sizeof(s_packet));
    if (s_vram != NULL) {
        sw_set_draw_area(0, 0, PSX_WII_VRAM_WIDTH - 1,
                         PSX_WII_VRAM_HEIGHT - 1);
        sw_set_draw_offset(0, 0);
        sw_set_mask_bits(0, 0);
        sw_set_semi_transparency(0, 0);
    }
}

void psx_wii_gp0_write(uint32_t word)
{
    if (s_required == 0) {
        s_required = packet_length(word);
        if (s_required == 0) {
            /* This bring-up subset ignores unknown opcodes one word at a time. */
            return;
        }
        s_packet[0] = word;
        s_collected = 1;
    } else {
        if (s_collected >= PSX_WII_GP0_MAX_WORDS) {
            s_collected = 0;
            s_required = 0;
            return;
        }
        s_packet[s_collected++] = word;
    }

    if (s_collected == s_required) {
        execute_packet();
        s_collected = 0;
        s_required = 0;
    }
}

int psx_wii_gp0_busy(void)
{
    return s_required != 0;
}
