// Package necko implements the ghostfox network worker: the process the
// engine spawns to perform HTTP(S) transfers. It speaks the engine's binary
// wire protocol on stdin/stdout — the Go replacement for the upstream
// Python ruyijs_necko.oneshot worker.
//
// Request frame  (stdin):  text(method) text(url) u32 headerCount
//                          {text(name) text(value)}* u32 bodyLen body
// Response frame (stdout): success: u32 status u32 headerCount
//                          {text(name) text(value)}* u32 bodyLen body
//                          u32 version("RRT2") u64 encoded u64 decoded
//                          text(protocol) f64 reqTick f64 respTick f64 hdrTick
//                          error:   u32 0 text(message)
// where text = u32 little-endian length + bytes and ticks are monotonic
// milliseconds ordered 0 <= requestTick <= responseTick <= headersTick.
package necko

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"math"
	"net/http"
	"net/url"
	"os"
	"strings"
	"time"
)

// Limits mirror the engine-side bounds (necko_wire.cpp / the Python worker).
const (
	maxField     = 64 << 10 // kFieldLimit
	maxHeader    = 256      // kHeaderLimit
	defaultLimit = 8 << 20  // the Python worker's 8 MiB body budget
)

// Worker serves requests from its input until EOF.
type Worker struct {
	Client  *http.Client
	BodyMax int64
}

// New builds a Worker honouring GHOSTFOX_PROXY / RUYIJS_HTTP_PROXY (the
// engine forwards the Context proxy through the child environment).
func New() *Worker {
	transport := &http.Transport{
		ForceAttemptHTTP2: true,
		MaxIdleConns:      64,
		IdleConnTimeout:   90 * time.Second,
		Proxy: func(*http.Request) (*url.URL, error) {
			return proxyFromEnv(), nil
		},
	}
	return &Worker{Client: &http.Client{Transport: transport}, BodyMax: defaultLimit}
}

func readText(r *bufio.Reader) (string, error) {
	var head [4]byte
	if _, err := io.ReadFull(r, head[:]); err != nil {
		return "", err
	}
	size := binary.LittleEndian.Uint32(head[:])
	if size > maxField {
		return "", fmt.Errorf("field exceeds %d bytes", maxField)
	}
	buf := make([]byte, size)
	if _, err := io.ReadFull(r, buf); err != nil {
		return "", err
	}
	return string(buf), nil
}

func writeText(w *bufio.Writer, s string) {
	var head [4]byte
	binary.LittleEndian.PutUint32(head[:], uint32(len(s)))
	w.Write(head[:])
	w.WriteString(s)
}

// Serve runs the request loop until the input closes (clean session end).
func (wk *Worker) Serve(in io.Reader, out io.Writer) error {
	r := bufio.NewReaderSize(in, 1<<16)
	w := bufio.NewWriterSize(out, 1<<16)
	for {
		err := wk.once(r, w)
		if flushErr := w.Flush(); err == nil {
			err = flushErr
		}
		if err != nil {
			if err == io.EOF {
				return nil
			}
			return err
		}
	}
}

func (wk *Worker) once(r *bufio.Reader, w *bufio.Writer) error {
	method, err := readText(r)
	if err != nil {
		return err
	}
	target, err := readText(r)
	if err != nil {
		return err
	}
	var head [4]byte
	if _, err := io.ReadFull(r, head[:]); err != nil {
		return err
	}
	count := binary.LittleEndian.Uint32(head[:])
	if count > maxHeader {
		return fmt.Errorf("header count %d exceeds %d", count, maxHeader)
	}
	headers := make([][2]string, 0, count)
	for i := uint32(0); i < count; i++ {
		name, err := readText(r)
		if err != nil {
			return err
		}
		value, err := readText(r)
		if err != nil {
			return err
		}
		headers = append(headers, [2]string{name, value})
	}
	if _, err := io.ReadFull(r, head[:]); err != nil {
		return err
	}
	body := make([]byte, binary.LittleEndian.Uint32(head[:]))
	if _, err := io.ReadFull(r, body); err != nil {
		return err
	}

	res := wk.transfer(method, target, headers, body)
	res.write(w)
	return nil
}

func (wk *Worker) transfer(method, target string, headers [][2]string, body []byte) response {
	start := monotonicMillis()
	req, err := http.NewRequest(method, target, bytes.NewReader(body))
	if err != nil {
		return failure(err)
	}
	for _, h := range headers {
		if strings.EqualFold(h[0], "Host") {
			req.Host = h[1]
			continue
		}
		req.Header[h[0]] = append(req.Header[h[0]], h[1])
	}
	resp, err := wk.Client.Do(req)
	if err != nil {
		return failure(err)
	}
	defer resp.Body.Close()
	payload, err := io.ReadAll(io.LimitReader(resp.Body, wk.BodyMax))
	if err != nil {
		return failure(err)
	}
	now := monotonicMillis()
	out := response{
		status:       resp.StatusCode,
		body:         payload,
		encoded:      uint64(len(payload)),
		decoded:      uint64(len(payload)),
		protocol:     resp.Proto,
		requestTick:  start,
		responseTick: now,
		headersTick:  now,
	}
	for name, values := range resp.Header {
		for _, value := range values {
			out.headers = append(out.headers, [2]string{name, value})
		}
	}
	return out
}

func failure(err error) response {
	return response{failed: true, message: err.Error()}
}

// response is one wire reply.
type response struct {
	failed       bool
	message      string
	status       int
	headers      [][2]string
	body         []byte
	encoded      uint64
	decoded      uint64
	protocol     string
	requestTick  float64
	responseTick float64
	headersTick  float64
}

func (res *response) write(w *bufio.Writer) error {
	var head [8]byte
	if res.failed {
		binary.LittleEndian.PutUint32(head[:4], 0)
		if _, err := w.Write(head[:4]); err != nil {
			return err
		}
		writeText(w, res.message)
		return nil
	}
	binary.LittleEndian.PutUint32(head[:4], uint32(res.status))
	if _, err := w.Write(head[:4]); err != nil {
		return err
	}
	binary.LittleEndian.PutUint32(head[:4], uint32(len(res.headers)))
	if _, err := w.Write(head[:4]); err != nil {
		return err
	}
	for _, h := range res.headers {
		writeText(w, h[0])
		writeText(w, h[1])
	}
	binary.LittleEndian.PutUint32(head[:4], uint32(len(res.body)))
	if _, err := w.Write(head[:4]); err != nil {
		return err
	}
	if _, err := w.Write(res.body); err != nil {
		return err
	}
	// Timing trailer version 2 ("RRT2"): byte counters, protocol name and
	// the three monotonic ticks the engine replays as resource timing.
	const version2 = 0x32545252
	binary.LittleEndian.PutUint32(head[:4], version2)
	if _, err := w.Write(head[:4]); err != nil {
		return err
	}
	binary.LittleEndian.PutUint64(head[:], res.encoded)
	if _, err := w.Write(head[:8]); err != nil {
		return err
	}
	binary.LittleEndian.PutUint64(head[:], res.decoded)
	if _, err := w.Write(head[:8]); err != nil {
		return err
	}
	writeText(w, res.protocol)
	for _, tick := range [3]float64{res.requestTick, res.responseTick, res.headersTick} {
		binary.LittleEndian.PutUint64(head[:], math.Float64bits(clampTick(tick)))
		if _, err := w.Write(head[:8]); err != nil {
			return err
		}
	}
	return nil
}

// clampTick enforces the ordering the engine validates:
// finite, non-negative, responseTick >= requestTick, headersTick >= responseTick.
func clampTick(t float64) float64 {
	if math.IsNaN(t) || math.IsInf(t, 0) || t < 0 {
		return 0
	}
	return t
}

// monotonicMillis mirrors the QPC milliseconds the Python worker reports.
func monotonicMillis() float64 {
	return float64(time.Since(processStart)) / float64(time.Millisecond)
}

var processStart = time.Now()

// proxyFromEnv resolves the effective proxy URL, if any.
func proxyFromEnv() *url.URL {
	for _, key := range []string{"GHOSTFOX_PROXY", "RUYIJS_HTTP_PROXY"} {
		if v := os.Getenv(key); v != "" {
			if u, err := url.Parse(v); err == nil {
				return u
			}
		}
	}
	return nil
}
