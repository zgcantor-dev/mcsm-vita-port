# Boot-logo hang root-cause analysis (libGameEngine + classes.dex)

Date: 2026-04-08

## What was analyzed

- `references/libGameEngine.so` with `readelf`/`llvm-objdump`.
- `references/classes.dex` with direct DEX parsing (method/class/signature extraction).

## Verified call chain (from `libGameEngine.so`)

`Platform_Android::GetUserLocation(String const&)` calls:

1. `Platform_Android::HasPermission(String)` (`0x58f49c -> 0x58c2e0`)
2. Then `JNIEnv::CallStaticObjectMethod` (`0x58f750`) to fetch an external storage path.
3. Then `stat`/filesystem checks and write-path setup before continuing boot.

This is all inside early startup flow and runs before first render.

## Verified Java side contract (from `classes.dex`)

`classes.dex` contains static methods on `com/telltalegames/telltale/TelltaleActivity`:

- `getExternalStorageDirectory()Ljava/lang/String;`
- `getExternalStorageDirs()[Ljava/lang/String;`
- `getObbFileName(Z)Ljava/lang/String;`
- `hasPermission(Ljava/lang/String;)Z`
- `requestPermission(I)V`
- `nativeOnPermissionComplete(IZ)V`

Method access flags confirm these are static (`0x9`, and native static `0x109` for `nativeOnPermissionComplete`).

## Root cause found

The engine requests writable locations under Android external storage roots (e.g. `/storage/emulated/0/...`) during `GetUserLocation`.

In this loader, path remapping handled:

- Android data prefixes (`/storage/emulated/0/Android/data/...`)
- Android OBB prefixes (`/storage/emulated/0/Android/obb/...`)

but **did not remap generic external-storage roots** like:

- `/storage/emulated/0/...`
- `/sdcard/...`
- `/mnt/sdcard/...`
- `/data/media/0/...`

So when the engine boot path touches these root-based paths, it stays on non-existent Android mount points on Vita. This blocks progress in startup filesystem prep and manifests as a boot-logo hang before rendering.

## Fix applied

In `source/reimpl/io.c`, `remap_android_storage_path()` now also remaps generic Android external-storage roots to `DATA_PATH`, after specific `Android/data` and `Android/obb` remaps.

That keeps startup path creation/stat calls inside Vita-accessible storage and allows boot to continue past logo.

