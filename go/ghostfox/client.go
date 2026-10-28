package ghostfox

import (
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sync"
	"time"
)

// JSError is a JS exception reported by the engine.
type JSError struct {
	Name, Message, Stack string
}

func (e *JSError) Error() string { return e.Name + ": " + e.Message }

// Context is one engine process with its page realm.
type Context struct {
	mu      sync.Mutex
	nextID  int
	shell   string
	mode    string
	timeout time.Duration
	proxy   string
	stdin   io.WriteCloser
	stdout  io.ReadCloser
	process *os.Process
	closed  bool
}

// Option configures a new Context.
type Option func(*Context)

// WithShell selects the engine executable (defaults to GHOSTFOX_SHELL, then
// RUYIJS_SHELL, then ghostfox/js beside the host binary).
func WithShell(path string) Option { return func(c *Context) { c.shell = path } }

// WithTimeout bounds every request.
func WithTimeout(d time.Duration) Option { return func(c *Context) { c.timeout = d } }

// WithProxy sets the engine's Necko transport proxy.
func WithProxy(url string) Option { return func(c *Context) { c.proxy = url } }

// New spawns the engine and speaks the frame protocol over its stdio.
func New(opts ...Option) (*Context, error) {
	c := &Context{mode: defaultMode, timeout: 30 * time.Second}
	for _, opt := range opts {
		opt(c)
	}
	if c.shell == "" {
		c.shell = os.Getenv("GHOSTFOX_SHELL")
	}
	if c.shell == "" {
		c.shell = os.Getenv("RUYIJS_SHELL")
	}
	if c.shell == "" {
		if exe, err := os.Executable(); err == nil {
			for _, name := range engineNames() {
				candidate := filepath.Join(filepath.Dir(exe), name)
				if fileExists(candidate) {
					c.shell = candidate
					break
				}
			}
		}
	}
	if c.shell == "" {
		return nil, errors.New("ghostfox: no engine found (set GHOSTFOX_SHELL or WithShell)")
	}

	cmd := exec.Command(c.shell, "--rpc", "stdio", "--mode", c.mode)
	cmd.Env = append(os.Environ(), "GHOSTFOX_PROXY="+c.proxy)
	stdin, err := cmd.StdinPipe()
	if err != nil {
		return nil, err
	}
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		stdin.Close()
		return nil, err
	}
	// stderr is a free-form log channel, kept out of the protocol stream and
	// drained so the engine never blocks writing it.
	logR, logW, err := os.Pipe()
	if err != nil {
		stdin.Close()
		return nil, err
	}
	cmd.Stderr = logW
	if err := cmd.Start(); err != nil {
		stdin.Close()
		return nil, fmt.Errorf("ghostfox: spawn engine: %w", err)
	}
	go func() {
		buf := make([]byte, 4096)
		for {
			if _, err := logR.Read(buf); err != nil {
				return
			}
		}
	}()
	c.stdin, c.stdout, c.process = stdin, stdout, cmd.Process
	return c, nil
}

func engineNames() []string {
	if runtime.GOOS == "windows" {
		return []string{"ghostfox.exe", "js.exe"}
	}
	return []string{"ghostfox", "js"}
}

func fileExists(p string) bool {
	st, err := os.Stat(p)
	return err == nil && !st.IsDir()
}

// call performs one locked RPC round trip.
func (c *Context) call(op string, fields map[string]interface{}) (interface{}, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.do(op, fields)
}

// do performs one RPC round trip; the caller holds c.mu.
func (c *Context) do(op string, fields map[string]interface{}) (interface{}, error) {
	if c.closed {
		return nil, errors.New("ghostfox: context is closed")
	}
	c.nextID++
	payload := map[string]interface{}{"id": c.nextID, "op": op}
	for k, v := range fields {
		payload[k] = v
	}
	data, err := json.Marshal(payload)
	if err != nil {
		return nil, err
	}
	if err := writeFrame(c.stdin, data); err != nil {
		return nil, err
	}
	for {
		reply, err := readFrame(c.stdout)
		if err != nil {
			return nil, err
		}
		var doc struct {
			ID    int             `json:"id"`
			OK    bool            `json:"ok"`
			Value json.RawMessage `json:"value"`
			Event string          `json:"event"`
			Error *struct {
				Name    string `json:"name"`
				Message string `json:"message"`
				Stack   string `json:"stack"`
			} `json:"error"`
		}
		if err := json.Unmarshal(reply, &doc); err != nil {
			return nil, fmt.Errorf("ghostfox: bad reply: %w", err)
		}
		if doc.Event == "callback" {
			// Reject host callbacks cleanly instead of hanging.
			ack, _ := json.Marshal(map[string]interface{}{
				"ok": false, "error": "callbacks unsupported"})
			if err := writeFrame(c.stdin, ack); err != nil {
				return nil, err
			}
			continue
		}
		if doc.ID != c.nextID {
			return nil, errors.New("ghostfox: mismatched response id")
		}
		if !doc.OK {
			if doc.Error == nil {
				return nil, errors.New("ghostfox: invalid error reply")
			}
			return nil, &JSError{Name: doc.Error.Name, Message: doc.Error.Message, Stack: doc.Error.Stack}
		}
		return decodeValue(doc.Value)
	}
}

func writeFrame(w io.Writer, data []byte) error {
	if len(data) > maxFrameBytes {
		return errors.New("ghostfox: frame exceeds 64 MiB")
	}
	head := make([]byte, 4)
	binary.LittleEndian.PutUint32(head, uint32(len(data)))
	if _, err := w.Write(head); err != nil {
		return err
	}
	_, err := w.Write(data)
	return err
}

func readFrame(r io.Reader) ([]byte, error) {
	head := make([]byte, 4)
	if _, err := io.ReadFull(r, head); err != nil {
		return nil, err
	}
	size := binary.LittleEndian.Uint32(head)
	if size == 0 || size > maxFrameBytes {
		return nil, fmt.Errorf("ghostfox: bad frame length %d", size)
	}
	body := make([]byte, size)
	if _, err := io.ReadFull(r, body); err != nil {
		return nil, err
	}
	return body, nil
}

// Close terminates the engine.
func (c *Context) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.closed {
		return nil
	}
	_, _ = c.do("close", nil)
	c.closed = true
	c.stdin.Close()
	done := make(chan struct{})
	go func() { c.process.Wait(); close(done) }()
	select {
	case <-done:
	case <-time.After(2 * time.Second):
		c.process.Kill()
	}
	return nil
}
