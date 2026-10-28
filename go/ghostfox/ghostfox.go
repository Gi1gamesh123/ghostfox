// Package ghostfox drives the ghostfox engine (a SpiderMonkey 158 build with
// the full native browser surface) as a child process over stdio frame RPC —
// portable to every platform Go builds for: no named pipes, no sockets, no cgo.
//
// Wire format (both directions): uint32 little-endian frame length followed
// by a JSON request/response document, on the engine's stdin/stdout.
// The engine is spawned with --rpc stdio.
package ghostfox

// Engine process command line:
//   ghostfox --rpc stdio --mode default
// Proxy forwarding: GHOSTFOX_PROXY=<url> in the child environment.
const (
	defaultMode   = "default"
	maxFrameBytes = 64 << 20 // engine-side MAX_FRAME bound
)
