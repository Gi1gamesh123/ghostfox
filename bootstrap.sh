#!/bin/bash
# ghostfox Linux/macOS bootstrap: fetch the official Firefox 158 source,
# overlay the ghostfox sources, and build the js engine.
#   Linux: toolchain via `./mach bootstrap` (~/.mozbuild) — see mozconfigs/
#   macOS: brew install llvm@19 rust cbindgen node  — see mac_build.sh
set -e
VER=158.0
cd "$(dirname "$0")"

if [ ! -d firefox-$VER ]; then
  echo "[bootstrap] downloading Firefox $VER source (~500MB)..."
  curl -L -o firefox-$VER.source.tar.xz \
    "https://archive.mozilla.org/pub/firefox/releases/$VER/source/firefox-$VER.source.tar.xz"
  echo "[bootstrap] extracting..."
  tar xf firefox-$VER.source.tar.xz
  mv firefox-$VER firefox-$VER 2>/dev/null || true
  rm -f firefox-$VER.source.tar.xz
fi

echo "[bootstrap] overlaying ghostfox sources..."
cp -r overlay/js/src/shell/.  firefox-$VER/js/src/shell/
cp -r overlay/js/src/vm/.     firefox-$VER/js/src/vm/
cp -r overlay/config/.        firefox-$VER/config/

case "$(uname -s)" in
  Darwin) echo "[bootstrap] macOS: run ./mac_build.sh next" ;;
  Linux)
    cp mozconfigs/mozconfig.linux firefox-$VER/mozconfig
    cd firefox-$VER
    ./mach bootstrap 2>/dev/null || true   # fetches clang19/rust/cbindgen/node
    python3 mach build 2>&1 | grep -aE ": error|Error 1|Error 2|successful" | head -40
    echo "[bootstrap] engine: obj-*/dist/bin/js"
    ;;
esac
