#include "psx_wii_video.h"

#include <gccore.h>
#include <ogc/cache.h>
#include <malloc.h>
#include <string.h>

#define PSX_WII_TEX_WIDTH  1024u
#define PSX_WII_TEX_HEIGHT 576u
#define PSX_WII_TEX_BYTES  (PSX_WII_TEX_WIDTH * PSX_WII_TEX_HEIGHT * 2u)

static GXRModeObj *s_mode;
static void *s_xfb[2];
static unsigned int s_next_xfb;
static uint16_t *s_texture_pixels;
static GXTexObj s_texture;
static int s_initialized;

static uint16_t argb_to_rgb565(uint32_t pixel)
{
    const uint32_t r = (pixel >> 16) & 0xffu;
    const uint32_t g = (pixel >> 8) & 0xffu;
    const uint32_t b = pixel & 0xffu;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/*
 * GX RGB565 textures are stored in 4x4 tiles, not as linear scanlines.
 * Swizzle while converting ARGB -> RGB565, and pad uncopied pixels with black.
 */
static void upload_tiled_rgb565(const uint32_t *pixels,
                                uint32_t width,
                                uint32_t height,
                                uint32_t pitch_bytes)
{
    const uint32_t tiles_per_row = PSX_WII_TEX_WIDTH / 4u;
    const uint32_t tiles_per_col = PSX_WII_TEX_HEIGHT / 4u;
    const uint32_t pitch_pixels = pitch_bytes / sizeof(uint32_t);
    uint32_t ty, tx, iy, ix;

    /* Clear the whole fixed-size texture so smaller frames never show stale data. */
    memset(s_texture_pixels, 0, PSX_WII_TEX_BYTES);

    for (ty = 0; ty < tiles_per_col; ++ty) {
        for (tx = 0; tx < tiles_per_row; ++tx) {
            const uint32_t tile_base = ((ty * tiles_per_row) + tx) * 16u;
            for (iy = 0; iy < 4u; ++iy) {
                const uint32_t y = ty * 4u + iy;
                if (y >= height) continue;
                const uint32_t *src = (const uint32_t *)
                    ((const uint8_t *)pixels + y * pitch_bytes);
                for (ix = 0; ix < 4u; ++ix) {
                    const uint32_t x = tx * 4u + ix;
                    if (x < width) {
                        s_texture_pixels[tile_base + iy * 4u + ix] =
                            argb_to_rgb565(src[x]);
                    }
                }
            }
        }
    }

    DCFlushRange(s_texture_pixels, PSX_WII_TEX_BYTES);
    (void)pitch_pixels; /* pitch is expressed in bytes above for alignment safety. */
}

int psx_wii_video_init(void)
{
    GXColor black = { 0, 0, 0, 0 };
    Mtx modelview;
    Mtx44 projection;
    unsigned int i;

    if (s_initialized) {
        return 0;
    }

    VIDEO_Init();
    s_mode = VIDEO_GetPreferredMode(NULL);
    if (s_mode == NULL) {
        return -1;
    }

    for (i = 0; i < 2u; ++i) {
        s_xfb[i] = memalign(32, s_mode->fbWidth * s_mode->xfbHeight *
                                  VI_DISPLAY_PIX_SZ);
        if (s_xfb[i] == NULL) {
            psx_wii_video_shutdown();
            return -1;
        }
        memset(s_xfb[i], 0,
               s_mode->fbWidth * s_mode->xfbHeight * VI_DISPLAY_PIX_SZ);
        DCFlushRange(s_xfb[i],
                     s_mode->fbWidth * s_mode->xfbHeight * VI_DISPLAY_PIX_SZ);
    }

    s_texture_pixels = (uint16_t *)memalign(32, PSX_WII_TEX_BYTES);
    if (s_texture_pixels == NULL) {
        psx_wii_video_shutdown();
        return -1;
    }
    memset(s_texture_pixels, 0, PSX_WII_TEX_BYTES);

    VIDEO_Configure(s_mode);
    VIDEO_SetNextFramebuffer(s_xfb[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (s_mode->viTVMode & VI_NON_INTERLACE) {
        VIDEO_WaitVSync();
    }

    GX_Init(NULL, 0);
    GX_SetCopyClear(black, 0x00ffffff);
    GX_SetViewport(0.0f, 0.0f, (f32)s_mode->fbWidth,
                   (f32)s_mode->efbHeight, 0.0f, 1.0f);
    GX_SetScissor(0, 0, s_mode->fbWidth, s_mode->efbHeight);
    GX_SetDispCopySrc(0, 0, s_mode->fbWidth, s_mode->efbHeight);
    GX_SetDispCopyDst(s_mode->fbWidth, s_mode->xfbHeight);
    GX_SetDispCopyYScale((f32)s_mode->xfbHeight / (f32)s_mode->efbHeight);
    GX_SetCopyFilter(s_mode->aa, s_mode->sample_pattern, GX_TRUE, s_mode->vfilter);
    GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GX_SetColorUpdate(GX_TRUE);
    GX_SetAlphaUpdate(GX_FALSE);
    GX_SetNumChans(1);
    GX_SetNumTexGens(1);
    GX_SetNumTevStages(1);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0,
                   GX_DF_NONE, GX_AF_NONE);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetNumIndStages(0);

    guMtxIdentity(modelview);
    GX_LoadPosMtxImm(modelview, GX_PNMTX0);
    guOrtho(projection, 0.0f, (f32)s_mode->efbHeight,
            0.0f, (f32)s_mode->fbWidth, 0.0f, 1.0f);
    GX_LoadProjectionMtx(projection, GX_ORTHOGRAPHIC);

    GX_InitTexObj(&s_texture, s_texture_pixels,
                  PSX_WII_TEX_WIDTH, PSX_WII_TEX_HEIGHT,
                  GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GX_InitTexObjLOD(&s_texture, GX_NEAR, GX_NEAR,
                     0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);

    s_next_xfb = 0;
    s_initialized = 1;
    return 0;
}

int psx_wii_video_present_argb(const uint32_t *pixels,
                               uint32_t width,
                               uint32_t height,
                               uint32_t pitch_bytes)
{
    const f32 tex_u = (f32)width / (f32)PSX_WII_TEX_WIDTH;
    const f32 tex_v = (f32)height / (f32)PSX_WII_TEX_HEIGHT;
    const f32 screen_w = s_mode ? (f32)s_mode->fbWidth : 0.0f;
    const f32 screen_h = s_mode ? (f32)s_mode->efbHeight : 0.0f;

    if (!s_initialized || pixels == NULL || width == 0u || height == 0u ||
        width > PSX_WII_TEX_WIDTH || height > PSX_WII_TEX_HEIGHT ||
        pitch_bytes < width * sizeof(uint32_t)) {
        return -1;
    }

    upload_tiled_rgb565(pixels, width, height, pitch_bytes);
    GX_InvalidateTexAll();
    GX_LoadTexObj(&s_texture, GX_TEXMAP0);

    GX_SetTevColor(GX_TEVREG0, (GXColor){ 255, 255, 255, 255 });
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
        GX_Position3f32(0.0f, 0.0f, 0.0f);          GX_TexCoord2f32(0.0f, 0.0f);
        GX_Position3f32(screen_w, 0.0f, 0.0f);      GX_TexCoord2f32(tex_u, 0.0f);
        GX_Position3f32(screen_w, screen_h, 0.0f);  GX_TexCoord2f32(tex_u, tex_v);
        GX_Position3f32(0.0f, screen_h, 0.0f);      GX_TexCoord2f32(0.0f, tex_v);
    GX_End();

    GX_DrawDone();
    GX_CopyDisp(s_xfb[s_next_xfb], GX_TRUE);
    GX_Flush();
    VIDEO_SetNextFramebuffer(s_xfb[s_next_xfb]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    s_next_xfb ^= 1u;
    return 0;
}

void psx_wii_video_shutdown(void)
{
    unsigned int i;
    if (s_texture_pixels != NULL) {
        free(s_texture_pixels);
        s_texture_pixels = NULL;
    }
    for (i = 0; i < 2u; ++i) {
        if (s_xfb[i] != NULL) {
            free(s_xfb[i]);
            s_xfb[i] = NULL;
        }
    }
    s_mode = NULL;
    s_next_xfb = 0;
    s_initialized = 0;
}
