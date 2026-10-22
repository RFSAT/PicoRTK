#!/usr/bin/env bash
# Build the PicoRTK UF2 with arduino-cli (same command as CI)
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tools/embed_web.py
VERSION="${PICORTK_VERSION:-$(git describe --tags --always --dirty 2>/dev/null || echo dev)}"
arduino-cli compile \
  --fqbn "rp2040:rp2040:rpipico2w:flash=4194304_1048576,ipbtstack=ipv4btcble,arch=arm,opt=Small" \
  --build-property "compiler.cpp.extra_flags=-DPICORTK_VERSION=\"$VERSION\"" \
  --output-dir build firmware/PicoRTK "$@"
ls -la build/*.uf2
