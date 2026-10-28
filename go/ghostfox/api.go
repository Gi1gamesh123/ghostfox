package ghostfox

import (
	"encoding/json"
	"fmt"
)

// Eval runs JavaScript in the page realm and converts the result with the
// engine's public value encoding (typed snapshots become Go values; DOM and
// function references degrade to descriptive strings).
func (c *Context) Eval(code string) (interface{}, error) {
	return c.call("eval", map[string]interface{}{
		"code": code, "filename": "<eval>", "line": -1, "col": -1})
}

// EvalFile runs code under a caller-chosen filename (stack traces name it).
func (c *Context) EvalFile(code, filename string) (interface{}, error) {
	return c.call("eval", map[string]interface{}{
		"code": code, "filename": filename, "line": -1, "col": -1})
}

// LoadPage installs an HTML document at url as the current page (about the
// same as navigating to a data: URL, without a network round trip).
func (c *Context) LoadPage(url, html string) error {
	_, err := c.call("page", map[string]interface{}{
		"action": "load", "url": url, "html": html})
	return err
}

// Navigate drives a real navigation through the engine's network transport.
func (c *Context) Navigate(url string) error {
	_, err := c.call("page", map[string]interface{}{
		"action": "navigate", "url": url})
	return err
}

// Checkpoint drains microtasks and pending timers.
func (c *Context) Checkpoint() error {
	_, err := c.call("checkpoint", nil)
	return err
}

// Advance pumps the event loop: macro tasks (network completions, timers,
// messages) plus engine jobs. System clock mode waits; this is how an async
// page's work becomes observable.
func (c *Context) Advance(ms float64) error {
	_, err := c.call("event_loop", map[string]interface{}{
		"action": "advance", "ms": ms, "step_ms": 16.667, "max_tasks": 100000})
	return err
}

// DrainTimers runs currently due timers without advancing the clock.
func (c *Context) DrainTimers() error {
	_, err := c.call("event_loop", map[string]interface{}{
		"action": "drain", "max_tasks": 100000})
	return err
}

// CollectGarbage runs a full GC.
func (c *Context) CollectGarbage() error {
	_, err := c.call("collect", nil)
	return err
}

// Set binds a Go value into the page global.
func (c *Context) Set(name string, value interface{}) error {
	_, err := c.call("set", map[string]interface{}{"name": name, "value": encodeValue(value)})
	return err
}

// Configure applies a fingerprint profile (the same document the upstream
// client passes as `environment`).
func (c *Context) Configure(profile interface{}) error {
	_, err := c.call("configure", map[string]interface{}{"profile": profile})
	return err
}

// Defaults returns the engine's built-in baseline fingerprint document.
func (c *Context) Defaults() (interface{}, error) {
	return c.call("defaults", nil)
}

// encodeValue converts a Go value into the engine's typed-value encoding.
func encodeValue(v interface{}) interface{} {
	switch t := v.(type) {
	case nil:
		return map[string]interface{}{"type": "null"}
	case bool:
		return map[string]interface{}{"type": "boolean", "value": t}
	case string:
		return map[string]interface{}{"type": "string", "value": t}
	case float64, float32, int, int32, int64, uint, uint32:
		return map[string]interface{}{"type": "number", "value": t}
	case []interface{}:
		items := make([]interface{}, len(t))
		for i, item := range t {
			items[i] = encodeValue(item)
		}
		return map[string]interface{}{"type": "array", "value": items}
	case map[string]interface{}:
		pairs := make([]interface{}, 0, len(t))
		for k, item := range t {
			pairs = append(pairs, map[string]interface{}{
				"key": k, "value": encodeValue(item)})
		}
		return map[string]interface{}{"type": "object", "value": pairs}
	default:
		return map[string]interface{}{"type": "string", "value": fmt.Sprint(v)}
	}
}

// decodeValue converts an engine reply into plain Go values.
func decodeValue(raw json.RawMessage) (interface{}, error) {
	if len(raw) == 0 {
		return nil, nil
	}
	var doc struct {
		Type  string          `json:"type"`
		Value json.RawMessage `json:"value"`
	}
	if err := json.Unmarshal(raw, &doc); err != nil || doc.Type == "" {
		var plain interface{}
		return plain, json.Unmarshal(raw, &plain)
	}
	switch doc.Type {
	case "null", "undefined":
		return nil, nil
	case "boolean", "number", "string":
		var v interface{}
		return v, json.Unmarshal(doc.Value, &v)
	case "array":
		var items []json.RawMessage
		if err := json.Unmarshal(doc.Value, &items); err != nil {
			return nil, err
		}
		out := make([]interface{}, len(items))
		for i, item := range items {
			v, err := decodeValue(item)
			if err != nil {
				return nil, err
			}
			out[i] = v
		}
		return out, nil
	case "object":
		var pairs []json.RawMessage
		if err := json.Unmarshal(doc.Value, &pairs); err != nil {
			return nil, err
		}
		out := make(map[string]interface{}, len(pairs))
		for _, pair := range pairs {
			var kv struct {
				Key   string          `json:"key"`
				Value json.RawMessage `json:"value"`
			}
			if err := json.Unmarshal(pair, &kv); err != nil {
				var arr []json.RawMessage
				if e := json.Unmarshal(pair, &arr); e == nil && len(arr) == 2 {
					var key string
					if e := json.Unmarshal(arr[0], &key); e == nil {
						v, e := decodeValue(arr[1])
						if e != nil {
							return nil, e
						}
						out[key] = v
						continue
					}
				}
				return nil, err
			}
			v, err := decodeValue(kv.Value)
			if err != nil {
				return nil, err
			}
			out[kv.Key] = v
		}
		return out, nil
	default:
		var v interface{}
		return v, json.Unmarshal(raw, &v)
	}
}
