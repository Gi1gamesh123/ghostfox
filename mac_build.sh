#!/bin/bash
# ghostfox macOS build: run ON the Mac. E: tree reaches the Mac via git
# bundle / rsync-over-ssh / USB; everything below assumes ./firefox-158.0
# beside this script is the synced tree.
set -e

# One-time toolchain (Apple clang is too old for FF158, which pins LLVM >=19):
#   brew install llvm@19 rust cbindgen node
#   rustup target add aarch64-apple-darwin   (or x86_64-apple-darwin on Intel)
LLVM=/opt/homebrew/opt/llvm@19          # /usr/local/opt/llvm@19 on Intel
export CC="$LLVM/bin/clang"
export CXX="$LLVM/bin/clang++"
export PATH="$LLVM/bin:$PATH"

cat > mozconfig.mac <<'EOF'
ac_add_options --enable-application=js
ac_add_options --enable-js-shell
ac_add_options --enable-optimize
ac_add_options --disable-debug
mk_add_options MOZ_MAKE_FLAGS="-j$(sysctl -n hw.ncpu)"
EOF

cd "$(dirname "$0")/firefox-158.0"
if [ ! -d obj-aarch64-apple-darwin ] && [ ! -d obj-x86_64-apple-darwin ]; then
  ./mach bootstrap
fi
MOZCONFIG=../mozconfig.mac ./mach build 2>&1 | grep -aE ": error|Error 1|Error 2|successful" | head -40

BIN=$(ls -d obj-*/dist/bin/js 2>/dev/null | head -1)
echo "engine: $BIN"
echo "next:   GOOS=darwin go build -o ghostfox-client ./go  (on any machine)"
