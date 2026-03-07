# Reverse engineering notes: `libGameEngine.so` OBB / TTArchive loading (study stage)

## Scope

This is a discovery-only reverse engineering pass over:

- `references/libGameEngine.so`

Using:

- `llvm-objdump` (dynamic symbols + targeted disassembly)
- `strings`

No code changes are included in this study document.

## High-confidence findings

### 1) Engine-side TTArchive pipeline exists and is active

`libGameEngine.so` contains explicit TTArchive classes/functions and resource-location wiring:

- `TTArchive::Load`
- `TTArchive2::Activate`
- `ResourceDirectory_TTArchive::*`
- `ResourceLocationFactory::CreateTTArchive(...)`

This means the engine has native logic to mount and read Telltale archives via internal `DataStream` plumbing.

### 2) Archive header verification happens in `TTArchive2::Activate`

Disassembly of `_ZN10TTArchive28ActivateE3PtrI10DataStreamE` shows:

- It creates/uses `DataSequentialStream` over a source `DataStream`.
- It reads a 4-byte header marker.
- It compares marker against **three magic values**:
  - `0x54544133`
  - `0x54544134`
  - `0x54544132`
- If none match, it bails from activation.

Interpretation: this function is a versioned TTArchive-format gatekeeper (likely TTA2/TTA3/TTA4 tags) before parsing entry metadata.

### 3) Parser shape matches archive table loading

Still in `TTArchive2::Activate`, after magic check:

- Reads small fixed-size fields (4-byte/2-byte reads).
- Applies sanity bounds checks on parsed values (for counts/sizes).
- Allocates/initializes arrays of per-entry metadata.
- Iterates and reads entry records repeatedly.

Interpretation: this is consistent with archive index/table parsing and resource-entry map construction.

### 4) Symbol evidence for Android file access entrypoints

Dynamic imports include:

- `open`, `fopen`, `read`, `pread`, `lseek`, `stat`, `fstat`, `opendir`, `readdir`
- `AAssetManager_fromJava`, `AAssetManager_open`, `AAssetManager_openDir`, etc.
- `SDL_AndroidGetInternalStoragePath`, `SDL_AndroidGetExternalStorageState`

Interpretation: on Android, engine can use both regular FS paths and AAsset APIs; in this port those are shimmed by `so_loader`.

### 5) String scan observations

`strings` found:

- `ttarch`
- `*.ttarch2`
- resource-assembly script strings such as `ResourceCreateConcreteArchiveLocation`

`strings` did **not** reveal direct literals for:

- `NCTT` / `TTCN`
- `Android/obb`
- `main.obb`, `patch.obb`
- package id `com.telltalegames.minecraft100`

Interpretation: OBB container specifics may be handled in other modules/scripts, while `libGameEngine.so` primarily mounts TTArchive streams and resource locations.

## Key function map (addresses from current binary)

- `0x00b85a88` — `_ZN10TTArchive24LoadERK3PtrI10DataStreamE`
- `0x00b84f4c` — `_ZN10TTArchive28ActivateE3PtrI10DataStreamE`
- `0x00b636d8` — `ResourceLocationFactory::CreateTTArchive(Symbol, String, Ptr<DataStream>, DataStreamCacheMode)`
- `0x00b638cc` — `ResourceLocationFactory::CreateTTArchive(Symbol, Ptr<DataStream>, DataStreamCacheMode)`
- `0x00b6306c` — `ResourceDirectory_TTArchive` ctor

## What this implies for so_loader design

### Most likely required behavior

1. Ensure the engine can resolve resource descriptors and archive names (`_rescdesc_*.lua`, `*.ttarch2`) from Vita paths.
2. Ensure `open/fopen/stat/opendir/readdir` remapping feeds correct paths into engine-side stream creation.
3. Preserve expected filename/case/layout exactly so resource scripts can mount archives.

### Possibly required later

If game scripts/libs insist on scanning a monolithic `.obb` container first (outside this binary’s visible strings), we may need a TTCN-compatible virtual file view. But this pass does **not** yet prove that `libGameEngine.so` itself directly parses TTCN OBB headers (`NCTT`) in this build.

## Recommended next RE step (still discovery-only)

1. Disassemble and inspect `DataStreamFactory::CreateFileStream` and Android resource directory constructors/callers to capture exact path expectations.
2. Trace xrefs/callers to `ResourceLocationFactory::CreateTTArchive(...)` to recover where archive filenames originate.
3. In runtime logs, capture first failing path(s) before black screen and compare to expected `*.ttarch2` / `_rescdesc_*.lua` layout.

## Command log used in this pass

```bash
/root/.swiftly/bin/llvm-objdump --dynamic-syms references/libGameEngine.so > /tmp/libGameEngine.dynsym.txt
/root/.swiftly/bin/llvm-objdump -d -C --no-show-raw-insn references/libGameEngine.so > /tmp/libGameEngine.S
strings -a -n 4 references/libGameEngine.so > /tmp/libGameEngine.strings.txt

rg -n "NCTT|TTCN|ttarch2?|rescdesc|resdesc|Android/obb|minecraft100|\.obb" /tmp/libGameEngine.strings.txt
rg -n "AAssetManager|SDL_AndroidGetInternalStoragePath|open$|fopen$|read$|lseek$|stat$|opendir$|readdir$|TTArchive" /tmp/libGameEngine.dynsym.txt

/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=_ZN10TTArchive24LoadERK3PtrI10DataStreamE references/libGameEngine.so
/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=_ZN10TTArchive28ActivateE3PtrI10DataStreamE references/libGameEngine.so
/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=_ZN23ResourceLocationFactory15CreateTTArchiveERK6SymbolRK6StringRK3PtrI10DataStreamE19DataStreamCacheMode references/libGameEngine.so
/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=_ZN23ResourceLocationFactory15CreateTTArchiveERK6SymbolRK3PtrI10DataStreamE19DataStreamCacheMode references/libGameEngine.so
```

## Additional discovery: `references/libSDL2.so`

A follow-up pass was done to test the hypothesis that custom OBB/TTCN logic might live in the game's SDL build.

### Findings

1. `libSDL2.so` shows Android JNI/storage and file-IO helper symbols (e.g. `SDL_AndroidGetInternalStoragePath_REAL`, `SDL_AndroidGetExternalStoragePath_REAL`, `Android_JNI_FileOpen`).
2. `libSDL2.so` string scan did **not** reveal TTCN/OBB-specific anchors (`NCTT`, `TTCN`, `.obb`, `Android/obb`, `rescdesc`).
3. `libSDL2.so` string scan did show SDL logging exports (`SDL_Log*` family), but no game-specific "file found" style message text.

### Interpretation

This suggests `libSDL2.so` is primarily providing generic Android platform glue (JNI, path helpers, file wrappers, logging), while game-specific TTArchive/asset mounting logic remains in `libGameEngine.so` + resource scripts.

So SDL is likely the logging surface, not the location of TTCN container parsing logic.

### Command snippets for this SDL pass

```bash
strings -a -n 4 references/libSDL2.so > /tmp/libSDL2.strings.txt
rg -n "NCTT|TTCN|ttarch2?|rescdesc|resdesc|Android/obb|\.obb|com\.telltalegames|minecraft100|AAsset|SDL_Log|Successfully created context|24-bit depth|storage/emulated|/sdcard" /tmp/libSDL2.strings.txt

/root/.swiftly/bin/llvm-objdump --dynamic-syms references/libSDL2.so > /tmp/libSDL2.dynsym.txt
rg -n "\*UND\*|AAsset|open|fopen|read|lseek|stat|opendir|readdir|SDL_Android|JNI|Java|obb|ttarch|rescdesc" /tmp/libSDL2.dynsym.txt

/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=SDL_AndroidGetInternalStoragePath_REAL --disassemble-symbols=SDL_AndroidGetExternalStoragePath_REAL --disassemble-symbols=Android_JNI_FileOpen references/libSDL2.so
```
