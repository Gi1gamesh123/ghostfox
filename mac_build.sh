#!/bin/bash
# ghostfox macOS build: run ON the Mac. Tree reaches the Mac via git
# bundle / rsync-over-ssh / USB; everything below assumes ./firefox-158.0
# beside this script is the synced tree.
#
# 配方与 .github/workflows/macos.yml 同源（CI 实账验证），三条铁律：
#  - CC/CXX 必须 Mozilla 官方 clang（mach artifact toolchain
#    macosx64-aarch64-clang）。brew llvm@19 配不动 Xcode 26 的 Apple ld，
#    configure 死在 "Failed to find an adequate linker"。Intel Mac 需换对应
#    toolchain artifact（CI 只验 arm64）。
#  - python 必须 3.14（mach 硬上限，3.15 编不过 taskgraph 依赖）；brew 的
#    python@3.14 是 keg-only，必须显式调用版本化解释器。
#  - macOS SDK >= 26.5（FF158 硬性要求），Xcode 26+。
set -e
set -o pipefail

PY3="$(brew --prefix python@3.14)/bin/python3.14"
if [ ! -x "$PY3" ]; then
  echo "[mac_build] 缺 python@3.14：brew install python@3.14 rust cbindgen node"
  exit 1
fi

cat > mozconfig.mac <<'EOF'
ac_add_options --enable-project=js
ac_add_options --enable-js-shell
ac_add_options --enable-optimize
ac_add_options --disable-debug
ac_add_options --disable-jemalloc
# libc++ marks floating-point to_chars available(macos 13.3); the engine's
# float from_chars sites are strtod-portable (macOS 26 would otherwise be
# required).
ac_add_options --enable-macos-target=13.3
mk_add_options MOZ_MAKE_FLAGS="-j$(sysctl -n hw.ncpu)"
EOF

cd "$(dirname "$0")/firefox-158.0"
if [ ! -x clang/bin/clang ]; then
  "$PY3" mach artifact toolchain --from-build macosx64-aarch64-clang
fi
export PATH="$PWD/clang/bin:$PATH"
export CC="$PWD/clang/bin/clang"
export CXX="$PWD/clang/bin/clang++"
"$CC" --version

# 全量日志落文件再过滤：管道 grep|head 会吞退出码（假绿）且 head 退出后的
# SIGPIPE 会掐死构建进程
if ! "$PY3" mach build > /tmp/ghostfox-build.log 2>&1; then
  grep -aE ": error|Error 1|Error 2|ld: error" /tmp/ghostfox-build.log | head -40
  exit 1
fi
grep -a "successful" /tmp/ghostfox-build.log | head -3 || true

BIN=$(ls -d obj-*/dist/bin/js 2>/dev/null | head -1)
echo "engine: $BIN"
echo "next:   (cd go && go build -o worker ./cmd/necko-worker) — 见 README 部署一节"
