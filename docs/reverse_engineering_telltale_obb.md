# OBB/TTArchive reverse-engineering summary (`libGameEngine.so`)

## TL;DR

The Android engine build has a **real OBB startup path**. It tries to open main/patch OBB files, load them through `TTArchive`, and register those archives as resource directories. If OBB paths are wrong (or JNI path helpers fail), startup can fall back into missing-file behavior and stall.

---

## What was analyzed

- `references/libGameEngine.so`
- `references/libSDL2.so`

Tools used:

- `llvm-objdump` (symbols + targeted disassembly)
- `strings`

---

## High-confidence findings

### 1) TTArchive loading is built into the engine

`libGameEngine.so` includes symbols and code paths for:

- `TTArchive::Load`
- `TTArchive2::Activate`
- `ResourceDirectory_TTArchive::*`
- `ResourceLocationFactory::CreateTTArchive(...)`

So the native engine can mount archive-backed resources directly.

### 2) `TTArchive2::Activate` validates archive magic

`_ZN10TTArchive28ActivateE3PtrI10DataStreamE` reads a 4-byte marker and accepts only:

- `0x54544133`
- `0x54544134`
- `0x54544132`

If magic doesn’t match, activation exits early.

### 3) Parser behavior matches archive index loading

After magic check, the function performs bounds checks, allocates entry metadata, and iterates entry records—consistent with table/index parsing.

### 4) Android file access surface is present

Imports include standard FS APIs (`open`, `fopen`, `stat`, `opendir`, etc.) and Android asset APIs (`AAssetManager_*`) plus SDL Android storage helpers.

### 5) Early string scan suggested limited direct OBB clues

Initial `strings` checks found `ttarch`/`*.ttarch2` hints but not obvious `NCTT`/`TTCN` anchors, implying TTArchive logic is primary and container specifics may be indirect.

---

## Major update: direct OBB mounting path was confirmed

Disassembly of `Platform_Android::RegisterGameDataDirectories` (`0x58eb44`) plus nearby strings confirms explicit OBB handling:

- `Using main obb %s from Google Play`
- `main obb %s does not exist.`
- `Using patch obb %s from Google Play`
- `patch obb %s does not exist.`

### Reconstructed flow (main + patch are parallel)

1. Build OBB path string.
2. `DataStreamFactory::CreateFileStream(path, ...)`
3. `TTArchive::Load(stream)`
4. `ResourceDirectory_TTArchive(...)`
5. Insert into resource directory set.

**Implication:** OBB-file compatibility is not optional for this startup path.

---

## What this means for `so_loader`

### Priority behavior to support

1. OBB path resolution must return valid, real file paths.
2. `open/stat` for those paths must succeed.
3. Main and patch OBB flows should both work.
4. Extracted `*.ttarch2` + `_rescdesc_*.lua` support should remain as fallback.

### Critical blocker found

Current JNI method tables are effectively empty, while engine strings reference methods/permissions like:

- `getObbFileName`
- `getExternalStorageDirectory`
- `android.permission.READ_EXTERNAL_STORAGE`

If those JNI calls are unresolved, OBB path discovery can fail and trigger the engine’s “obb does not exist” branches.

---

## Recommended next steps (implementation-focused)

1. Log real engine-requested paths at `open/stat` wrapper boundaries.
2. Implement minimal JNI methods needed for OBB/storage path discovery.
3. Map both main/patch OBB requests to deterministic Vita locations.
4. Keep extracted-file routing as secondary fallback.
5. Re-test and classify startup path used on-device (direct OBB vs extracted fallback).

---

## Key symbols/functions referenced

- `0x00b85a88` — `_ZN10TTArchive24LoadERK3PtrI10DataStreamE`
- `0x00b84f4c` — `_ZN10TTArchive28ActivateE3PtrI10DataStreamE`
- `0x00b636d8` — `ResourceLocationFactory::CreateTTArchive(Symbol, String, Ptr<DataStream>, DataStreamCacheMode)`
- `0x00b638cc` — `ResourceLocationFactory::CreateTTArchive(Symbol, Ptr<DataStream>, DataStreamCacheMode)`
- `0x00b6306c` — `ResourceDirectory_TTArchive` ctor
- `0x58eb44` — `Platform_Android::RegisterGameDataDirectories`

---

## Command log (condensed)

```bash
/root/.swiftly/bin/llvm-objdump --dynamic-syms references/libGameEngine.so > /tmp/libGameEngine.dynsym.txt
/root/.swiftly/bin/llvm-objdump -d -C --no-show-raw-insn references/libGameEngine.so > /tmp/libGameEngine.S
strings -a -n 4 references/libGameEngine.so > /tmp/libGameEngine.strings.txt

/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=_ZN10TTArchive28ActivateE3PtrI10DataStreamE references/libGameEngine.so
/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=_ZN16Platform_Android27RegisterGameDataDirectoriesEv references/libGameEngine.so
/root/.swiftly/bin/llvm-objdump -d --start-address=0x58ec6c --stop-address=0x58ef10 references/libGameEngine.so

strings -a -n 4 -t x references/libGameEngine.so > /tmp/libGameEngine.strings_addr.txt
rg -n "obb|ttarch2|getObbFileName|getExternalStorageDirectory|READ_EXTERNAL_STORAGE" /tmp/libGameEngine.strings_addr.txt

strings -a -n 4 references/libSDL2.so > /tmp/libSDL2.strings.txt
/root/.swiftly/bin/llvm-objdump --dynamic-syms references/libSDL2.so > /tmp/libSDL2.dynsym.txt

rg -n "nameToMethodId|MethodsObject|methodsObject|mAssetMgr" source/java.c lib/falso_jni/FalsoJNI_ImplBridge.c
```
