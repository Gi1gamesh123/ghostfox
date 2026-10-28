/* Protocol code runs only in the controller realm; never installed on page. */
(() => {
  "use strict";
  const maxDepth = 64;
  const maxNodes = 100000;
  function encode(input, extra = 0) {
    let nodes = 0;
    // Reserve envelope space before creating an encoded tree/JSON string.
    let remaining = 16 * 1024 * 1024 - 65536 - extra;
    function charge(size) {
      remaining -= size;
      if (remaining < 0) throw new RangeError("Value exceeds protocol byte budget");
    }
    function stringCost(text) {
      if (text.length > remaining) charge(text.length);
      charge(2);
      for (let i = 0; i < text.length; i++) {
        const code = text.charCodeAt(i);
        if (code < 32) charge(6);
        else if (code === 34 || code === 92) charge(2);
        else if (code < 128) charge(1);
        else if (code < 2048) charge(2);
        else if (code >= 0xd800 && code <= 0xdbff && i + 1 < text.length &&
                 text.charCodeAt(i + 1) >= 0xdc00 && text.charCodeAt(i + 1) <= 0xdfff) {
          charge(4); i++;
        } else charge(code >= 0xd800 && code <= 0xdfff ? 6 : 3);
      }
    }
    const active = new Set();
    function visit(value, depth) {
      if (++nodes > maxNodes || depth > maxDepth)
        throw new RangeError("Value exceeds protocol traversal limit");
      charge(64); // Each node's tag, delimiters and scalar encoding.
      if (value === null) return {type: "null"};
      const type = typeof value;
      switch (type) {
        case "undefined": return {type};
        case "boolean": return {type, value};
        case "string": stringCost(value); return {type, value};
        case "number":
          return {type, value: Number.isNaN(value) ? "NaN" :
            Object.is(value, -0) ? "-0" :
            value === Infinity ? "Infinity" :
            value === -Infinity ? "-Infinity" : value};
        case "bigint": {
          const decimal = String(value);
          stringCost(decimal);
          return {type, value: decimal};
        }
        case "function":
        case "symbol": throw new TypeError(`Unsupported result type: ${type}`);
      }
      if (active.has(value)) throw new TypeError("Cyclic result value");
      active.add(value);
      let result;
      if (Array.isArray(value)) {
        if (value.length > maxNodes) throw new RangeError("Array exceeds protocol limit");
        const items = [];
        for (let i = 0; i < value.length; i++) items.push(visit(value[i], depth + 1));
        result = {type: "array", value: items};
      } else {
        const pairs = [];
        const keys = Object.keys(value);
        if (keys.length > maxNodes) throw new RangeError("Object exceeds protocol limit");
        for (const key of keys) {
          stringCost(key);
          pairs.push([key, visit(value[key], depth + 1)]);
        }
        result = {type: "object", value: pairs};
      }
      active.delete(value);
      return result;
    }
    return visit(input, 0);
  }
  return {
    callback(callId, name, args) {
      return JSON.stringify({event: "callback", callId, name,
        args: encode(args, name.length * 6)});
    },
    reply(id, ok, value) {
      return JSON.stringify(ok ? {id, ok, value: encode(value,
        typeof id === "string" ? id.length * 6 : 0)} :
        {id, ok, error: value});
    }
  };
})()
