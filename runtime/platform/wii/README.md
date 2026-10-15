# Wii / vWii platform work

This directory is the start of a native Wii/vWii platform backend for PSXRecomp.
It uses devkitPro/libogc and targets PowerPC 750 / Wii homebrew. The first
module is an isolated controller adapter so its ABI and toolchain can be tested
before the full desktop runtime is untangled from SDL and desktop OS services.

## Current status

- `psx_wii_input.c` maps a libogc Wii Remote + Nunchuk sample to signed
  movement and aim axes plus a provisional PS1-style logical button mask.
- `smoke/` builds a small diagnostic `.dol` that prints those values.
- GitHub Actions compiles that DOL using the devkitPro Docker image.
- This is **not** the PSXRecomp runtime port yet. No game boots from this
  module; renderer, audio, scheduler, CD-ROM, BIOS, memory and overlay loader
  integration remain future work.

## Input contract

- Nunchuk analog stick maps to movement X/Y, in the range -127..127.
- Wii Remote IR coordinates map to screen-center-relative aim X/Y.
- D-pad maps to PS1 directions; A/B/1/2 and C/Z map to provisional action /
  shoulder buttons.
- IR aim is only available with a working sensor bar and valid IR sample.
- Button selection and aiming semantics must be tuned in Ape Escape gameplay.

## Build locally

With devkitPro installed and `DEVKITPRO` / `DEVKITPPC` set:

```sh
cd runtime/platform/wii/smoke
make
```

The output is `psx-wii-input-smoke.dol`. On GitHub Actions it is uploaded as
the `psxrecomp-wii-input-smoke-dol` artifact.

## Next integration steps

1. Verify this input adapter on Dolphin and physical Wii/vWii hardware.
2. Add a PS1 pad-state bridge which matches the framework's current input API.
3. Introduce a Wii runtime target that does not include the SDL desktop entry
   point, desktop GL/Vulkan renderers, launcher or host-only services.
4. Use libogc GX for presentation and Wii audio / filesystem services.
5. Bring up BIOS reset execution, RAM/BIOS loading, timing and the software GPU
   renderer, then work through SPU, CD-ROM and dynamically loaded overlays.
