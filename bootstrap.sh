#!/bin/bash
# ghostfox Linux/macOS bootstrap: fetch the official Firefox 158 source,
# overlay the ghostfox sources, and build the js engine.
#   Linux: toolchain via `./mach bootstrap` (~/.mozbuild) — see mozconfigs/
#   macOS: python@3.14 + Mozilla clang via mac_build.sh — see mac_build.sh
#   （brew llvm@19 配不动 Xcode 26 的 Apple ld，勿用，见 mac_build.sh 头注）
set -e
set -o pipefail
VER=158.0
cd "$(dirname "$0")"

if [ ! -d firefox-$VER ]; then
  echo "[bootstrap] downloading Firefox $VER source (~500MB)..."
  curl -L -o firefox-$VER.source.tar.xz \
    "https://archive.mozilla.org/pub/firefox/releases/$VER/source/firefox-$VER.source.tar.xz"
  echo "[bootstrap] extracting..."
  tar xf firefox-$VER.source.tar.xz
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
    ./mach bootstrap   # fetches clang19/rust/cbindgen/node into ~/.mozbuild
    # 全量日志落文件再过滤：管道 grep|head 会吞退出码（假绿）且 head 退出后
    # 的 SIGPIPE 会掐死构建进程
    if ! python3 mach build > /tmp/ghostfox-build.log 2>&1; then
      grep -aE ": error|Error 1|Error 2|ld.lld: error" /tmp/ghostfox-build.log | head -40
      exit 1
    fi
    grep -a "successful" /tmp/ghostfox-build.log | head -3 || true
    echo "[bootstrap] engine: obj-*/dist/bin/js"
    ;;
esac
