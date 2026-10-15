# Wii / vWii platform work

This directory is the start of a native Wii/vWii platform backend for PSXRecomp.
It uses devkitPro/libogc and targets PowerPC 750 / Wii homebrew.

## Current status

- `psx_wii_input.c` maps libogc Wii Remote + Nunchuk samples to movement and
  aim axes plus a provisional PS1-style logical button mask.
- `psx_wii_video.c` is an initial GX presentation primitive for 32-bit ARGB
  frames. It converts frames to tiled RGB565 texture memory and displays them
  through GX. The caller is responsible for generating the ARGB frame.
- `smoke/` builds a diagnostic DOL that compiles the input and video modules.
- The parent repository's GitHub Actions workflow compiles and uploads the
  diagnostic `.dol`.
- This is **not** the PSXRecomp runtime port yet. No game boots from these
  modules; audio, scheduler, CD-ROM, BIOS, memory and overlay loader integration
  remain future work.

## Input contract

- Nunchuk analog stick maps to movement X/Y, in the range -127..127.
- Wii Remote IR coordinates map to screen-center-relative aim X/Y.
- D-pad maps to PS1 directions; A/B/1/2 and C/Z map to provisional action /
  shoulder buttons.
- IR aim is only available with a working sensor bar and valid IR sample.
- Button selection and aiming semantics must be tuned in Ape Escape gameplay.

## Video contract

- `psx_wii_video_init()` configures libogc video/GX and allocates two display
  framebuffers plus one tiled RGB565 texture.
- `psx_wii_video_present_argb()` accepts an ARGB image with explicit width,
  height and row pitch, converts it to the GX tiled format, and presents it.
- The first version targets ordinary 4:3 output and caps input at 1024x576.
  It is a bring-up path; timing, interlace, overscan and aspect handling need
  testing on Dolphin and real Wii/vWii hardware.
- The software renderer's existing `sw_render_display()` function already
  produces ARGB pixels, but the runtime has not yet been wired to call this
  presenter.

## Build locally

With devkitPro installed and `DEVKITPRO` / `DEVKITPPC` set:

```sh
cd runtime/platform/wii/smoke
make
```

The output is `psx-wii-input-smoke.dol`. On GitHub Actions it is uploaded as
the `psxrecomp-wii-input-smoke-dol` artifact.

## Next integration steps

1. Verify this input/video backend on Dolphin and physical Wii/vWii hardware.
2. Add a PS1 pad-state bridge matching the framework's current input API.
3. Introduce a Wii runtime target that does not include the SDL desktop entry
   point, desktop GL/Vulkan renderers, launcher or host-only services.
4. Use the portable GPU software rasterizer and make `sw_render_display()`
   feed `psx_wii_video_present_argb()`.
5. Bring up BIOS reset execution, RAM/BIOS loading and deterministic timing,
   then work through SPU, CD-ROM and dynamically loaded overlays.
