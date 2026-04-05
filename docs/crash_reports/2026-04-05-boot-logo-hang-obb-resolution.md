# Boot-logo hang analysis: `libGameEngine.so` waiting on OBB path checks

## Symptom

After the crash fixes, the game remains on the startup logo and never reaches first-frame rendering.

## Root cause hypothesis (high confidence)

`libGameEngine.so` performs OBB registration early in startup (`Platform_Android::RegisterGameDataDirectories`).
That flow repeatedly probes for main/patch OBB files before gameplay systems initialize.

On Vita, those probes are intercepted by `stat_soloader/open_soloader/fopen_soloader` path remaps in `source/reimpl/io.c`.
The remapper attempted a recursive filesystem scan under `DATA_PATH` for every unresolved OBB probe.
When OBB files are missing or in an unexpected location, this can become a long repeated scan loop and appears as a boot-logo hang.

## Why this matches the observed behavior

- The app no longer crashes, so startup proceeds into engine resource registration.
- Rendering does not begin, which is consistent with blocking before archive/resource initialization completes.
- Existing reverse-engineering notes already confirm explicit OBB existence checks in `libGameEngine.so` startup paths.

## Fix applied

- Added an OBB path resolution cache (separate entries for main/patch).
  - Successful and failed lookups are memoized so repeated engine probes do not repeat expensive scans.
- Added a bounded recursive scan budget (`OBB_SCAN_ENTRY_BUDGET`) to prevent unbounded directory walking.
- Added warning log when scan budget is exhausted.

This keeps OBB probing deterministic and prevents startup from stalling in repeated path discovery.
