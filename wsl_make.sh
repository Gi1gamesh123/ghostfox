#!/bin/bash
# Sync E: sources, re-read moz.build via config.status (from the objdir, as
# mach does), then build with direct make.
set -e
SRC=/mnt/e/ruyi-clone/firefox-158.0/js/src/shell
DST=/root/ghostfox/firefox-158.0/js/src/shell
# --checksum: E:(Windows) and WSL clocks skew, so mtime comparison silently
# skips freshly edited files; content comparison is the only safe test.
rsync -a --delete --checksum "$SRC/ruyijs_src/" "$DST/ruyijs_src/"
cp "$SRC/moz.build" "$DST/moz.build"
cp "$SRC/ruyijs_posix_stubs.cpp" "$DST/ruyijs_posix_stubs.cpp"
cp "$SRC/ruyijs_vendor_zshim.c" "$DST/ruyijs_vendor_zshim.c"
cp /mnt/e/ruyi-clone/firefox-158.0/js/src/vm/StandaloneTrace.h \
   /root/ghostfox/firefox-158.0/js/src/vm/ 2>/dev/null || true
cd /root/ghostfox/firefox-158.0/obj-x86_64-pc-linux-gnu
export PATH=/root/.mozbuild/clang/bin:/root/.cargo/bin:$PATH
/root/.mozbuild/srcdirs/firefox-158.0-d909e44614e9/_virtualenvs/build/bin/python \
  ./config.status > /root/cs.log 2>&1 || { echo CONFIG_STATUS_FAILED; tail -12 /root/cs.log; exit 1; }
# set -e 下 make 失败会当场退出，错误报告段必须挂在 set +e 区间内
set +e
make -j"$(nproc)" > /root/full_err.log 2>&1
MAKE_EXIT=$?
set -e
echo "MAKE_EXIT=$MAKE_EXIT"
if [ "$MAKE_EXIT" -ne 0 ]; then
  grep -aE ": error|ld.lld: error" /root/full_err.log | head -12
fi
tail -2 /root/full_err.log
[ "$MAKE_EXIT" -eq 0 ] || exit 1
