#!/usr/bin/env python3
import re
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ANDROID_SO = ROOT / "references/libGameEngine.so"
VITA_EBOOT = ROOT / "references/vitalibs/eboot.bin.elf"

# Targets chosen from libGameEngine.so rendering path and one known local function.
TARGETS = {
    "RenderGeometry::BeginStaticVertices": 0x00AAC190,
    "GameRender::Initialize": 0x00A9CB04,
    "GameRender::RenderFrame": 0x00AA05C8,
    "GameRender::RenderScene": 0x00A9DD98,
    "OpenGLUtil::Initialize": 0x00B3139C,
}

# Program headers of references/vitalibs/eboot.bin.elf from readelf -l
VITA_TEXT_FILE_OFFSET = 0x0000A0
VITA_TEXT_VADDR = 0x81000000
VITA_TEXT_SIZE = 0x746D44

WORD_RE = re.compile(r"^\s*([0-9a-f]+):\s*([0-9a-f]{8})", re.IGNORECASE)


def run(cmd):
    return subprocess.check_output(cmd, text=True)


def disassemble_words(binary: Path, start: int, size: int = 0x50):
    out = run([
        "llvm-objdump",
        "-D",
        "--triple=armv7-none-eabi",
        f"--start-address=0x{start:x}",
        f"--stop-address=0x{start+size:x}",
        str(binary),
    ])
    words = []
    for line in out.splitlines():
        m = WORD_RE.match(line)
        if m:
            addr = int(m.group(1), 16)
            word = int(m.group(2), 16)
            words.append((addr, word))
    return words


def normalize_arm_word(word: int):
    # Mask relocation-heavy immediates while preserving opcode class.
    top = (word >> 24) & 0xFF

    # B / BL (cond and unconditional), mask imm24.
    if top in {0xEA, 0xEB, 0xFA, 0xFB}:
        return word & 0xFF000000, 0xFF000000

    # MOVW / MOVT style immediate fields (split in instruction encoding).
    # Keep opcode bits, ignore immediate fields.
    if (word & 0x0FF00000) in {0x03000000, 0x03400000}:
        return word & 0xFFF0F000, 0xFFF0F000

    return word, 0xFFFFFFFF


def load_vita_words():
    with VITA_EBOOT.open("rb") as f:
        f.seek(VITA_TEXT_FILE_OFFSET)
        blob = f.read(VITA_TEXT_SIZE)

    words = []
    for i in range(0, len(blob) - 3, 4):
        w = struct.unpack_from("<I", blob, i)[0]
        addr = VITA_TEXT_VADDR + i
        words.append((addr, w))
    return words


def find_candidates(pattern_words, vita_words, min_score=6, top_n=8):
    pat = [normalize_arm_word(w) for _, w in pattern_words]
    plen = len(pat)
    results = []

    vita_raw = [w for _, w in vita_words]
    vita_addr = [a for a, _ in vita_words]

    for i in range(0, len(vita_raw) - plen):
        score = 0
        for j, (pword, pmask) in enumerate(pat):
            if (vita_raw[i + j] & pmask) == (pword & pmask):
                score += 1
        if score >= min_score:
            results.append((score, vita_addr[i]))

    results.sort(key=lambda x: (-x[0], x[1]))
    return results[:top_n]


def main():
    vita_words = load_vita_words()

    print("# Android -> Vita render-function candidate mapping")
    print(f"Android: {ANDROID_SO}")
    print(f"Vita:    {VITA_EBOOT}")
    print()

    for name, addr in TARGETS.items():
        # 10 words gives a good signature while still tolerant of callsite differences.
        pattern = disassemble_words(ANDROID_SO, addr, size=0x28)
        if len(pattern) < 8:
            print(f"## {name} @ 0x{addr:08X}")
            print("Unable to decode enough instructions; skipping.\n")
            continue

        candidates = find_candidates(pattern[:10], vita_words, min_score=7, top_n=6)

        print(f"## {name} @ 0x{addr:08X}")
        print("Android signature words:")
        print(" ".join(f"{w:08x}" for _, w in pattern[:10]))

        if not candidates:
            print("No candidates meeting score threshold.\n")
            continue

        print("Top Vita candidates:")
        for score, caddr in candidates:
            print(f"- 0x{caddr:08X} (score {score}/10)")
        print()


if __name__ == "__main__":
    main()
