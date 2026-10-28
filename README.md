# ghostfox

A SpiderMonkey (Firefox 158) based browser-engine: the full DOM/network layer
of the ruyiJS engine, ported to Linux and macOS, driven over a cross-platform
stdio RPC protocol with a Go client.

- **Windows**: native (named pipes → stdio), VS2022 clang-cl
- **Linux**: native, LLVM 19
- **macOS**: native (planned, CI via GitHub Actions macos-14)
- **Go client**: `go/` — stdio frame protocol (4-byte LE length + JSON),
  cross-compiles everywhere (`CGO_ENABLED=0 GOOS=<os> go build`)

## Layout

    overlay/js/src/shell/ruyijs_src/    engine sources (C++ DOM/network/event loop)
    overlay/js/src/shell/ruyijs_vendor/ brotli/zlib/harfbuzz vendor build
    overlay/js/src/shell/*.cpp|.c       shell-level TUs (RPC loop, POSIX stubs, zlib shim)
    overlay/js/src/vm/StandaloneTrace.h runtime trace support
    overlay/config/gcc_hidden.h         visibility pragma disabled (static binary)
    go/                                 Go client + Necko transport worker
    tests/                              end-to-end drivers
    mozconfigs/                         per-platform mozconfig
    bootstrap.sh                        Linux/macOS: fetch Firefox source, overlay, build
    mac_build.sh                        macOS build (Apple silicon / Intel)
    build_js.bat                        Windows build

## Build

The engine is a js-standalone Firefox build: fetch the official Firefox 158.0
source, copy `overlay/` over it, build with the matching mozconfig.

### Linux

    ./bootstrap.sh                      # downloads Firefox source, overlays, builds
    # engine at firefox-158.0/obj-*/dist/bin/js

Toolchain: LLVM >= 19.1.4 (`~/.mozbuild/clang/bin` after `./mach bootstrap`),
rust, cbindgen, node. See `mozconfigs/mozconfig.linux`.

### Windows

    build_js.bat                        # VS2022 clang-cl; see mozconfigs/mozconfig.win

### macOS

    brew install llvm@19 rust cbindgen node
    ./mac_build.sh                      # Apple silicon or Intel

### Cross-platform client + transport worker

    cd go
    CGO_ENABLED=0 GOOS=linux   go build -o ghostfox-client ./cmd/smoke
    CGO_ENABLED=0 GOOS=darwin  go build -o ghostfox-worker ./cmd/necko-worker

## Engine protocol

Spawn `js --rpc stdio --mode default`; speak 4-byte little-endian length
framed JSON over its stdin/stdout. Ops: `eval`, `set`, `expose`, `native`,
`page` (load/navigate), `event_loop`, `configure`, `cookies`, `collect`,
`checkpoint`, `close`. The Go client wraps all of it (`go/ghostfox`).

Network traffic goes through a separate transport worker process
(`go/cmd/necko-worker`) spawned by the engine and wired over pipes, carrying
the same framing the upstream engine used for its Necko helper.

## Tests

    python3 tests/google_min_linux.py "some query"

Drives a real Google search end to end: fetch (curl_cffi, Firefox TLS
fingerprint), hand the challenge page to the engine, let it solve and set the
clearance cookie, then re-request and parse the results.

## License

MPL-2.0. The engine sources derive from the ruyiJS project (MPL-2.0) with
portability work for POSIX platforms; Firefox sources stay under their own
license and are fetched from Mozilla at build time, not redistributed here.
