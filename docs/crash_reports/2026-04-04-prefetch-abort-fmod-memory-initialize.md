# Crash investigation: prefetch abort at `pc = 0x00000000`

Date: 2026-04-04

## Provided dump

- Exception: Prefetch abort
- `pc = 0x00000000`
- `lr = 0x9892EA6C`
- `ip = 0x9902BB44`
- `BadVAddr = 0x00000000`

This pattern is a null indirect branch (`ldr pc, [slot]` where slot contains 0).

## Address mapping (no assumptions)

The loader uses:

- `LOAD_ADDRESS = 0x98000000` (`source/utils/init.c`).

From the dump:

- `lr_runtime = 0x9892EA6C`
- `lr_relative = lr_runtime - LOAD_ADDRESS = 0x92EA6C`

`0x92EA6C` resolves inside `references/libGameEngine.so` in:

- `_ZN19SoundSystemInternal11SoundMemory10InitializeEv`

The exact instruction at `0x92EA68` is a PLT call:

- `bl 0x53739c`

with return at `0x92EA6C` (matching dump LR).

## PLT/GOT resolution

The PLT stub at `0x53739c` loads PC from GOT slot at runtime.

From relocation entries (`readelf -r references/libGameEngine.so`):

- GOT `0x0102BB44` corresponds to `FMOD_Memory_Initialize` (`R_ARM_JUMP_SLOT`).

With loader base `0x98000000`, runtime GOT address becomes:

- `0x98000000 + 0x0102BB44 = 0x9902BB44`

This exactly matches dump `ip = 0x9902BB44`.

## Conclusion

Root cause is confirmed:

- During `SoundSystemInternal::SoundMemory::Initialize`, code calls `FMOD_Memory_Initialize` via PLT.
- The GOT slot for `FMOD_Memory_Initialize` at runtime (`0x9902BB44`) is null.
- PLT performs an indirect jump to `0x00000000` -> prefetch abort.

So this is specifically an unresolved/null `FMOD_Memory_Initialize` import at call time.

## Most likely failure point to inspect next

In this repository, import resolution path is:

- `resolve_imports(mod)` in startup (`source/utils/init.c`) and
- FMOD symbols exported in `default_dynlib` (`source/dynlib.c`).

If `FMOD_Memory_Initialize` is not resolved to a valid function pointer before game-engine init, this exact crash occurs.
