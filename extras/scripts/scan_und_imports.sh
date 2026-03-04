#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
REF_DIR="${ROOT_DIR}/references"

READELF="${READELF:-arm-vita-eabi-readelf}"
if ! command -v "$READELF" >/dev/null 2>&1; then
  READELF=readelf
fi

for so in "$REF_DIR"/*.so; do
  [ -e "$so" ] || continue
  echo "=== $(basename "$so") ==="
  "$READELF" -Ws "$so" | awk '$4=="FUNC" && $7=="UND" {print $8}' | sort -u
  echo
 done
