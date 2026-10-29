@echo off
rem ruyi-clone SpiderMonkey 构建 (原生 mingw make, 绕开 msys2 fork 问题)
rem
rem 路径自适应，不钉死盘符：环境根目录按
rem   1. 环境变量 RUYI_CLONE_ROOT
rem   2. 本脚本所在目录
rem   3. 本脚本的上级目录
rem 顺序探测（以 firefox-158.0 源码树在哪为准）。
rem MozillaBuild 系统安装默认 C:\mozilla-build，可用环境变量 MOZILLABUILD 覆盖。

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
if not "%RUYI_CLONE_ROOT%"=="" set "ROOT=%RUYI_CLONE_ROOT%"
if exist "%ROOT%\firefox-158.0\mach" goto root_ok
for %%i in ("%ROOT%\..") do set "ROOT=%%~fi"
if exist "%ROOT%\firefox-158.0\mach" goto root_ok
echo [build_js] 未找到 firefox-158.0 源码树（探测过 %~dp0 及其上级）
echo [build_js] 请把本脚本放进环境根目录或其下仓库目录，或先 set RUYI_CLONE_ROOT=环境根目录
exit /b 1
:root_ok
if "%MOZILLABUILD%"=="" set "MOZILLABUILD=C:\mozilla-build"
if not exist "%MOZILLABUILD%\python3\python.exe" (
    echo [build_js] 未找到 %MOZILLABUILD%\python3\python.exe
    echo [build_js] 请安装 MozillaBuild，或先 set MOZILLABUILD=安装目录
    exit /b 1
)
set "PATH=%ROOT%\shims;%ROOT%\firefox-158.0\clang\bin;%ROOT%\firefox-158.0\rustc\bin;%ROOT%\cargo-home\bin;%MOZILLABUILD%\msys2\usr\bin;%MOZILLABUILD%\bin;C:\Windows\System32"
set "MOZBUILD_STATE_PATH=%ROOT%\mozbuild-state"
set "PIP_CACHE_DIR=%ROOT%\pip-cache"
set "TEMP=%ROOT%\tmp"
set "TMP=%ROOT%\tmp"
set "CARGO_HOME=%ROOT%\cargo-home"
cd /d "%ROOT%\firefox-158.0"
"%MOZILLABUILD%\python3\python.exe" mach build
