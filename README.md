# ghostfox

A SpiderMonkey (Firefox 158) based browser-engine: the full DOM/network layer
of the ruyiJS engine, ported to Linux and macOS, driven over a cross-platform
stdio RPC protocol with a Go client.

- **Windows**: native (named pipes → stdio), VS2022 clang-cl
- **Linux**: native, CI on ubuntu-24.04 (releng clang via `mach artifact toolchain`)
- **macOS**: native (Apple silicon), CI on macos-26 (Mozilla clang + Apple ld)
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

The supported path is CI: the `macos.yml` workflow builds on a **macos-26**
runner and uploads `js` (+ the Go worker) as an artifact — grab that instead
of building locally. Local builds on older macOS releases do **not** work:
Firefox 158 requires the macOS **26.5 SDK** (Xcode 26). If you do have a
macOS 26 machine, replicate the workflow rather than `brew install llvm`:
it uses Mozilla's own clang (`mach artifact toolchain --from-build
macosx64-aarch64-clang`, native arm64) paired with Apple ld, and calls
`$(brew --prefix python@3.14)/bin/python3.14` explicitly (brew's default
python 3.15 exceeds `mach`'s supported maximum, and python@3.14 is keg-only
so there is no bare `python3` on PATH). `mac_build.sh` predates this recipe
and is kept for reference only.

### Cross-platform client + transport worker

    cd go
    CGO_ENABLED=0 GOOS=linux   go build -o ghostfox-client ./cmd/smoke
    CGO_ENABLED=0 GOOS=darwin  go build -o ghostfox-worker ./cmd/necko-worker

## Continuous integration

`.github/workflows/linux.yml` and `macos.yml` build the engine end to end on
every push (~48 min Linux, ~10 min macOS with caches) and upload the `js`
binary as a workflow artifact. The artifacts are the intended distribution
channel — prebuilt, no local Firefox toolchain needed. The worker is not in
the artifact; build it from `go/` (one command, no toolchain).

## Known build pitfalls (all hit in practice on the macOS CI bring-up)

- **SDK floor**: Firefox 158 refuses SDKs < 26.5 → only macos-26 runners work.
- **Linker pairing**: brew LLVM fails at "Failed to find an adequate linker"
  against Xcode 26's Apple ld. Use Mozilla's releng clang (see above).
- **python version**: `mach` supports ≤ 3.14; brew python is 3.15 on
  macos-26 and `rtoml` fails to compile under 3.15. The fallback python3.11
  cannot run taskgraph. Use the python@3.14 keg binary explicitly.
- **Apple libc++ float charconv**: `std::to_chars(float)` is available from
  macOS 13.3, but `std::from_chars(float)` only from macOS **26.0**. The
  overlay replaces two float `from_chars` call sites with strtod-based
  parsing (tokens are pre-validated numerics; see commit 5ee3aee).
- **libc++ 22 strictness**: rejects the two-raw-pointer `std::string`
  constructor, and headers like `<cmath>`/`<cctype>`/`<cstring>` are no
  longer transitively included — ~31 overlay TUs needed explicit includes.
- **CI artifact gating**: `actions/upload-artifact` defaults to
  `if-no-files-found: warn`, which turns a failed build into a silent
  green run. Ours is set to `error`.

## Engine protocol

Spawn `js --rpc stdio --mode default`; speak 4-byte little-endian length
framed JSON over its stdin/stdout. Ops: `eval`, `set`, `expose`, `native`,
`page` (load/navigate), `event_loop`, `configure`, `cookies`, `collect`,
`checkpoint`, `close`. The Go client wraps all of it (`go/ghostfox`).

Network traffic goes through a separate transport worker process
(`go/cmd/necko-worker`) spawned by the engine and wired over pipes, carrying
the same framing the upstream engine used for its Necko helper. The worker
pins HTTP/1.1: Google's front end kills Go's HTTP/2 connections on
`/search` (EOF), so `NextProtos` is pinned accordingly with a 45s client
timeout.

## Tests

    python3 tests/google_min_linux.py "some query"

Drives a real Google search end to end: fetch (curl_cffi, Firefox TLS
fingerprint), hand the challenge page to the engine, let it solve and set the
clearance cookie, then re-request and parse the results.

## License

MPL-2.0. The engine sources derive from the ruyiJS project (MPL-2.0) with
portability work for POSIX platforms; Firefox sources stay under their own
license and are fetched from Mozilla at build time, not redistributed here.
