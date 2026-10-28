@echo off
rem ruyi-clone SpiderMonkey 构建 (原生 mingw make, 绕开 msys2 fork 问题)
set PATH=E:\ruyi-clone\shims;E:\ruyi-clone\firefox-158.0\clang\bin;E:\ruyi-clone\firefox-158.0\rustc\bin;E:\ruyi-clone\cargo-home\bin;C:\mozilla-build\msys2\usr\bin;C:\mozilla-build\bin;C:\Windows\System32
set MOZILLABUILD=C:\mozilla-build
set MOZBUILD_STATE_PATH=E:\ruyi-clone\mozbuild-state
set PIP_CACHE_DIR=E:\ruyi-clone\pip-cache
set TEMP=E:\ruyi-clone\tmp
set TMP=E:\ruyi-clone\tmp
set CARGO_HOME=E:\ruyi-clone\cargo-home
cd /d E:\ruyi-clone\firefox-158.0
C:\mozilla-build\python3\python.exe mach build
