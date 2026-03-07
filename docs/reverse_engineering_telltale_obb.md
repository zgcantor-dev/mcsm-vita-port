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

## Deeper finding: direct OBB handling in `Platform_Android::RegisterGameDataDirectories`

A targeted disassembly pass on `Platform_Android::RegisterGameDataDirectories` (`0x58eb44`) revealed explicit OBB-driven mounting flow in engine code.

### Evidence from strings + code path

`strings -t x` shows these literals in `libGameEngine.so`:

- `f67420` — `Using main obb %s from Google Play`
- `f67444` — `main obb %s does not exist.`
- `f67460` — `patch obb %s does not exist.`
- `f67480` — `Using patch obb %s from Google Play`

The function at `0x58eb44` logs those messages, then executes archive-loading calls.

### Reconstructed flow inside `RegisterGameDataDirectories`

For **main OBB** branch:

1. Log `Using main obb ...`.
2. Build String from main OBB path.
3. `DataStreamFactory::CreateFileStream(path, ...)`.
4. `TTArchive::Load(stream)`.
5. `ResourceDirectory_TTArchive(name, ttarchive)`.
6. Insert resulting resource directory into `ResourceFramer` set.

For **patch OBB** branch (parallel logic):

1. Log `Using patch obb ...`.
2. Build String from patch OBB path.
3. `DataStreamFactory::CreateFileStream(path, ...)`.
4. `TTArchive::Load(stream)`.
5. `ResourceDirectory_TTArchive(name, ttarchive)`.
6. Insert resulting resource directory into `ResourceFramer` set.

There are also explicit missing-file log paths (`main obb ... does not exist`, `patch obb ... does not exist`) and fallback handling.

### Implication for `so_loader`

This strongly suggests that at least this engine build expects OBB containers to be usable as file streams in normal startup flow. Therefore, the current extracted-only remap strategy may be insufficient by itself in scenarios where this branch is taken.

To support this engine behavior, `so_loader` likely needs one of:

1. **Real OBB file presence + correct path remap** (so engine opens/mounts OBB directly), or
2. **A compatibility layer that emulates OBB-as-stream behavior** expected by `TTArchive::Load` call sites.

Given this discovery, full support should prioritize matching this `RegisterGameDataDirectories` flow first, then keep extracted-file fallback as secondary path.

### Additional command snippets used

```bash
# locate OBB literals with addresses
strings -a -n 4 -t x references/libGameEngine.so | grep -Ei 'obb|ttarch2|ResourceCreateConcreteArchiveLocation'

# inspect OBB mounting function
/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=_ZN16Platform_Android27RegisterGameDataDirectoriesEv references/libGameEngine.so

# focused window around core main/patch OBB branches
/root/.swiftly/bin/llvm-objdump -d --start-address=0x58ec6c --stop-address=0x58ef10 references/libGameEngine.so
```

## Implementation-path checklist: what is still needed

Based on discovery so far, there are two viable compatibility strategies.

### Method A — "Direct OBB" (match engine startup path)

Target: satisfy `Platform_Android::RegisterGameDataDirectories` so engine loads main/patch OBB via `TTArchive::Load`.

#### Required pieces

1. **OBB path resolution must return valid file paths** used by the engine's OBB branch.
2. **`open/stat` on those paths must succeed** (engine logs explicit `... does not exist` otherwise).
3. **`DataStreamFactory::CreateFileStream` must be fed real OBB files** that `TTArchive::Load` can parse.
4. **Both main and patch handling should be supported**, since code has separate branches/messages for each.

#### Discovery clues supporting this

- OBB-specific strings in engine (`Using main obb ...`, `main obb ... does not exist`, `patch obb ... does not exist`, `Using patch obb ...`).
- OBB branch calls: `CreateFileStream` -> `TTArchive::Load` -> `ResourceDirectory_TTArchive`.

### Method B — "Extracted assets only" (bypass/avoid OBB reliance)

Target: ensure resource scripts can mount `*.ttarch2` + `_rescdesc_*.lua` directly from filesystem without requiring OBB stream path.

#### Required pieces

1. Correct extracted layout and exact filenames/case for descriptor + archive files.
2. Path remapping must resolve all requested paths into those extracted files.
3. Resource script functions (`ResourceCreateConcreteArchiveLocation` flow) must see expected archive names.

#### Risk

Engine clearly has direct OBB startup path; if that path is taken before script-only fallback, extracted-only mode may still fail unless OBB discovery is neutralized or redirected cleanly.

## Critical JNI gap discovered for OBB path method

Current port JNI method tables are empty (`nameToMethodId[] = {}` and all `methods*[] = {}`), while engine strings show Android activity/JNI method names related to OBB and storage, including:

- `getObbFileName`
- `getExternalStorageDirectory`
- `android.permission.READ_EXTERNAL_STORAGE`
- activity class names (`com/telltalegames/telltale/TelltaleActivity`, `org/libsdl/app/SDLActivity`)

In FalsoJNI bridge, unresolved methods return null/false/0 and log warnings (`method ID ... not found!`).

### Why this matters

If OBB path discovery in engine depends on these Java method callbacks, the current empty JNI method map can cause null/invalid paths, which would trigger the engine's `... does not exist` path and prevent OBB mount.

## Practical next implementation tasks (ordered)

1. **Instrument and log actual OBB path strings at runtime** right before `open/stat` (in so_loader wrappers) to confirm what engine asks for.
2. **Implement minimal JNI methods for OBB/storage path discovery** (at least those seen in strings) or hard-wire equivalent behavior in shims.
3. **Make OBB path remap deterministic** for both main and patch names to real Vita locations.
4. Keep extracted-data remap as fallback path for script-driven archive mounting.
5. Re-test and classify which startup path is being used on-device (direct OBB branch vs extracted-only branch).

## Commands used for this additional discovery

```bash
# contextual string mining with addresses
strings -a -n 4 -t x references/libGameEngine.so

# locate key Android/OBB strings
rg -n "getObbFileName|getExternalStorageDirectory|READ_EXTERNAL_STORAGE|Using main obb|patch obb|TelltaleActivity|SDLActivity|ResourceCreateConcreteArchiveLocation|\.ttarch2" /tmp/libGameEngine.strings_addr.txt

# inspect engine path/setup and OBB registration flows
/root/.swiftly/bin/llvm-objdump --dynamic-syms references/libGameEngine.so
/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=_ZN16Platform_Android20GetBaseUserDirectoryEv references/libGameEngine.so
/root/.swiftly/bin/llvm-objdump -d --disassemble-symbols=_ZN16Platform_Android27RegisterGameDataDirectoriesEv references/libGameEngine.so

# verify current port JNI implementation status
rg -n "nameToMethodId|MethodsObject|methodsObject|mAssetMgr" source/java.c lib/falso_jni/FalsoJNI_ImplBridge.c
```
