"""HTTP requests through the existing Necko adapter. No browser or cookie state.

`-m ruyijs_necko.oneshot` serves one request. With `--session` the same process
serves requests in order over the same pipes until its input closes, keeping one
adapter so connections are reused per origin, as Firefox's connection manager
reuses them (docs/NECKO_TRANSPORT.md, ARCHITECTURE "Necko 会话传输").
"""
import os
import struct
import sys
from urllib.parse import urlsplit
from necko.adapters import HTTPAdapter
from requests import Request
from requests.structures import CaseInsensitiveDict
from ruyijs_necko import RequestControl, open_response
from ruyijs_necko.timing import snapshot

LIMIT = 8 * 1024 * 1024  # ResourceNetwork Limits::body_bytes, after decompression.
# One request's whole budget. Firefox's connection timeout is 90 s
# (network.http.connection-timeout), so connecting gets no shorter limit of its
# own: TCP, the proxy CONNECT and TLS share this budget with the response, and a
# busy proxy queueing many tunnels no longer fails at an arbitrary 5 s.
BUDGET = 15
READ = 10
# Firefox network.http.request.max-attempts; one request's total deadline still applies.
ATTEMPTS = RequestControl.MAX_ATTEMPTS


class Framing(ValueError):
    """The request stream itself is unusable; the session ends."""


def transport_proxies():
    proxy = os.environ.get("RUYIJS_HTTP_PROXY", "")
    if not proxy:
        return {}
    address = urlsplit(proxy)
    if (len(proxy) > 65536 or address.scheme not in ("http", "https") or
            not address.hostname or address.path not in ("", "/") or
            address.query or address.fragment or any(c in proxy for c in "\r\n\x00")):
        raise ValueError("Invalid HTTP proxy configuration")
    address.port  # Validate a supplied numeric port before opening any socket.
    return {"http": proxy, "https": proxy}


def read_exact(size):
    data = sys.stdin.buffer.read(size)
    if len(data) != size:
        raise Framing("Truncated oneshot request")
    return data


def read_text(head=None):
    size = struct.unpack("<I", head or read_exact(4))[0]
    if size > 65536:
        raise Framing("Oneshot field is too large")
    return read_exact(size).decode("utf-8")


def write_text(value):
    data = value.encode("utf-8")
    sys.stdout.buffer.write(struct.pack("<I", len(data)) + data)


def write_error(message):
    sys.stdout.buffer.write(struct.pack("<I", 0))
    write_text(message[:1024])
    sys.stdout.buffer.flush()


def read_request(head=None):
    method, url = read_text(head), read_text()
    count = struct.unpack("<I", read_exact(4))[0]
    if count > 256:
        raise Framing("Too many request headers")
    headers = [(read_text(), read_text()) for _ in range(count)]
    length = struct.unpack("<I", read_exact(4))[0]
    if length > LIMIT:
        raise Framing("Request body exceeds 8 MiB")
    return method, url, headers, read_exact(length)


def serve(adapter, method, url, headers, body):
    if sum(len(k.encode("utf-8")) + len(v.encode("utf-8")) for k, v in headers) > 65536:
        raise ValueError("Request headers exceed 64 KiB")
    fields = CaseInsensitiveDict()
    for name, value in headers:
        fields[name] = fields[name] + ", " + value if name in fields else value
    prepared = Request(method, url, headers=fields, data=body or None).prepare()
    control = RequestControl(BUDGET, max_attempts=ATTEMPTS)
    reader = open_response(adapter, prepared, control,
                           timeout=(BUDGET, READ), verify=True, proxies=transport_proxies())
    chunks, total = [], 0
    try:
        for chunk in reader:
            total += len(chunk)
            if total > LIMIT:
                raise ValueError("Response body exceeds 8 MiB")
            chunks.append(chunk)
    finally:
        reader.close()
    payload = b"".join(chunks)
    if len(reader.headers) > 256 or sum(
            len(k.encode("utf-8")) + len(v.encode("utf-8")) for k, v in reader.headers) > 65536:
        raise ValueError("Response headers exceed the transport limit")
    sys.stdout.buffer.write(struct.pack("<I", reader.status))
    sys.stdout.buffer.write(struct.pack("<I", len(reader.headers)))
    for name, value in reader.headers:
        write_text(name)
        write_text(value)
    sys.stdout.buffer.write(struct.pack("<I", len(payload)))
    sys.stdout.buffer.write(payload)
    # A versioned observation trailer accompanies the existing response.
    # Counts come from the one strict decoder, including compressed bodies.
    measured = '--timing-v2' in sys.argv
    sys.stdout.buffer.write(struct.pack('<IQQ', 0x32545252 if measured else 0x31545252,
                                       reader.decoder.encoded, reader.decoder.decoded))
    write_text({10: 'http/1.0', 11: 'http/1.1', 20: 'h2', 30: 'h3'}.get(reader.version, ''))
    if measured:
        sys.stdout.buffer.write(struct.pack('<ddd', *snapshot(control)))
    sys.stdout.buffer.flush()


def main():
    request = read_request()
    adapter = HTTPAdapter()
    try:
        serve(adapter, *request)
    finally:
        adapter.close()


def session():
    adapter = HTTPAdapter()
    try:
        while True:
            head = sys.stdin.buffer.read(4)
            if not head:
                return
            if len(head) != 4:
                raise Framing("Truncated session request")
            request = read_request(head)
            # A failed request leaves the stream framed: report it and go on.
            try:
                serve(adapter, *request)
            except Exception as error:
                write_error(str(error))
    finally:
        adapter.close()


if __name__ == "__main__":
    try:
        if "--webtransport" in sys.argv[1:]:
            from ruyijs_necko.webtransport_worker import main as webtransport
            webtransport()
        else:
            session() if "--session" in sys.argv[1:] else main()
    except Exception as error:
        write_error(str(error))
        raise SystemExit(1)
