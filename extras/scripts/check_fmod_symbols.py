#!/usr/bin/env python3
"""Audit FMOD symbol coverage against reference archives.

Compares symbols declared in source/fmod_symbols.h with symbols exported by:
- references/libfmodpp.a
- references/vitalibs/sce_module/libfmodstudio_stub.a
- optional wrappers in source/fmodpp_compat.cpp (FMODPP_ALIAS)
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HDR = ROOT / "source" / "fmod_symbols.h"
COMPAT = ROOT / "source" / "fmodpp_compat.cpp"
LIBS = [
    ROOT / "references" / "libfmodpp.a",
    ROOT / "references" / "vitalibs" / "sce_module" / "libfmodstudio_stub.a",
]


def parse_header_symbols() -> list[str]:
    text = HDR.read_text(encoding="utf-8")
    return re.findall(r"extern void \*([A-Za-z0-9_]+);", text)


def parse_compat_aliases() -> set[str]:
    text = COMPAT.read_text(encoding="utf-8")
    return set(re.findall(r"FMODPP_ALIAS\(([^,]+),", text))


def nm_symbols(path: Path) -> set[str]:
    out = subprocess.check_output(["nm", "-g", str(path)], text=True, errors="ignore")
    result: set[str] = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            result.add(parts[2])
        elif len(parts) == 2:
            result.add(parts[1])
    return result


def main() -> int:
    missing_files = [str(p) for p in [HDR, COMPAT, *LIBS] if not p.exists()]
    if missing_files:
        print("Missing required files:")
        for p in missing_files:
            print(f"  - {p}")
        return 2

    header_symbols = parse_header_symbols()
    compat_aliases = parse_compat_aliases()

    lib_symbols: set[str] = set()
    for lib in LIBS:
        lib_symbols |= nm_symbols(lib)

    available = lib_symbols | compat_aliases
    unresolved = [s for s in header_symbols if s not in available]

    print(f"Header FMOD symbols: {len(header_symbols)}")
    print(f"Symbols in reference archives: {len(lib_symbols)}")
    print(f"Symbols covered by fmodpp_compat aliases: {len(compat_aliases)}")
    print(f"Unresolved symbols: {len(unresolved)}")

    if unresolved:
        print("\nMissing FMOD symbols:")
        for symbol in unresolved:
            print(f"  - {symbol}")
        return 1

    print("\nAll FMOD symbols are covered by reference archives and/or fmodpp_compat aliases.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
