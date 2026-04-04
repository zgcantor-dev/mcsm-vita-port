# FMOD Studio linker mismatch analysis

## What was checked

Using only files in `references/`:

1. `references/libfmodpp.a`
2. `references/vitalibs/sce_module/libfmodstudio_stub.a`
3. `references/libfmodstudio.so`

## Findings

- `libfmodpp.a` is missing several C++ wrappers that `source/dynlib.c` expects to take addresses of.
  - Example missing wrappers: `FMOD::Studio::CueInstance::trigger()`, `FMOD::Studio::Bus::setFaderLevel(float)`, and many `const`-qualified `FMOD::Studio::*` methods.
- `libfmodpp.a` contains some similarly named methods but with **different signatures/mangling**.
  - Example present: `FMOD::Studio::EventInstance::getUserData(void**)` (non-const)
  - Example expected by loader table: `FMOD::Studio::EventInstance::getUserData(void**) const`
- `libfmodstudio_stub.a` exports/imports mostly the C API (`FMOD_Studio_*`) symbols, not the C++ mangled wrapper methods.
- `libfmodstudio.so` does export the missing C++ mangled methods, but those are not directly represented in the Vita stub archive used at link time.

## Why link fails

`dynlib.c` binds a symbol table containing mangled C++ names (including const-qualified wrappers). The current `fmodpp` archive available to the build does not define all of those symbols, and the stub archive does not provide C++-mangled imports to fill the gap. Result: unresolved references from `dynlib.c.obj` at final link.

## Practical direction

- Keep using your own project sources (not reference blobs) and either:
  1. prune/conditionally exclude unsupported FMOD C++ entries from the binding table in `dynlib.c`, or
  2. add local compatibility wrappers that provide the missing mangled names and forward to available C API calls where possible.

This is a **version/interface mismatch** between the expected FMOD C++ wrapper surface and the `fmodpp`+stub pair currently in toolchain/reference assets.
