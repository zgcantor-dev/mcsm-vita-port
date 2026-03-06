# Android `libGameEngine.so` -> Vita `eboot.bin.elf` rendering reverse-engineering notes

This documents an initial pass to map rendering functions from the Android binary to the stripped Vita EBOOT, using `llvm-objdump` and ARM instruction fingerprints.

## What was inspected

### Android symbol anchors (`references/libGameEngine.so`)
`nm -D -C references/libGameEngine.so` exposes enough exported C++ symbols to anchor rendering code:

- `GameRender::Initialize()` at `0x00A9CB04`
- `GameRender::RenderFrame()` at `0x00AA05C8`
- `GameRender::RenderScene(...)` at `0x00A9DD98`
- `OpenGLUtil::Initialize()` at `0x00B3139C`
- `RenderGeometry::BeginStaticVertices(...)` at `0x00AAC190`

The Android binary also imports `SDL_GL_*` and `egl*` entry points, confirming GL/EGL-backed rendering on Android.

### Vita target (`references/vitalibs/eboot.bin.elf`)
The Vita EBOOT has no section table and no symbols, but it *does* contain executable `PT_LOAD` data and many `SceGxm`/`SceDisplay` strings, which suggests an official Vita backend path.

## Helper script

A script was added to automate signature matching:

```bash
python3 extras/scripts/map_render_funcs.py
```

What it does:

1. Disassembles selected Android rendering functions with `llvm-objdump`.
2. Builds normalized ARM opcode signatures (masking branch immediates).
3. Scans the Vita executable segment (`PT_LOAD#0`) for high-score matches.

## Current result

On this pass, no high-confidence full-function matches were found for the selected targets. This is expected if:

- Vita build used different optimization/inlining,
- backend code was rewritten for GXM,
- function bodies are not structurally equivalent to Android GL paths.

## Practical hook direction from here

Because direct body matching did not produce reliable one-to-one mapping, the most practical route is:

1. Keep Android rendering hooks at stable Android addresses/symbols in `libGameEngine.so`.
2. Bridge those calls to Vita GL/GXM compatibility shims on the loader side.
3. For Vita-native backend behavior, identify EBOOT rendering entry points by import-call fan-in to `SceGxm*` NIDs (instead of pure byte-signature matching).

That NID-driven callgraph pass should be the next step if you want direct EBOOT hooks.
