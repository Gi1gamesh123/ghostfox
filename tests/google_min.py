# -*- coding: utf-8 -*-
"""ruyi-clone M5: Google 端到端 —— 完全对齐 ruyijs 双进程架构
  Python(curl_cffi Firefox TLS 指纹) = ruyijs 的 necko 进程
  ruyi-clone js.exe(自编译 SpiderMonkey) = ruyijs 的 js.exe 解算器
流程照抄 reference/examples/google/google_search.py:
  navigate -> SG_SS interstitial -> put_resource+assign+advance 解 cookie
  -> necko 带 SG_SS+sei 重访 -> SERP
"""
import json, re, sys, time, uuid, subprocess, struct, ctypes
from ctypes import wintypes as W
from urllib.parse import urlsplit, urlunsplit, parse_qsl, urlencode

from curl_cffi import requests as cr

JS = r"E:\ruyi-clone\firefox-158.0\obj-x86_64-pc-windows-msvc\dist\bin\js.exe"
PROXY = "http://127.0.0.1:7897"
IMP = "firefox135"
NAVIGATION_ACCEPT = ("text/html,application/xhtml+xml,application/xml;"
                     "q=0.9,*/*;q=0.8")
UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:158.0) Gecko/20100101 "
      "Firefox/158.0")


def ff_headers(referer=None, site="none"):
    h = {
        "User-Agent": UA,
        "Accept-Language": "en-US,en;q=0.5",
        "Upgrade-Insecure-Requests": "1",
        "Accept": NAVIGATION_ACCEPT,
        "Connection": "keep-alive",
        "Priority": "u=0, i",
        "Sec-Fetch-Dest": "document",
        "Sec-Fetch-Mode": "navigate",
        "Sec-Fetch-Site": site,
        "Sec-Fetch-User": "?1",
    }
    if referer:
        h["Referer"] = referer
    return h


# ---------------- js.exe RPC (ruyijs 协议) ----------------
k32 = ctypes.windll.kernel32

def pipe_open(name):
    h = k32.CreateFileW(name, 0xC0000000, 0, None, 3, 0, None)
    if h in (-1, 0xFFFFFFFFFFFFFFFF):
        raise RuntimeError(f"connect failed: {k32.GetLastError()}")
    return h

def pipe_send(h, data: bytes):
    w = W.DWORD(0)
    frame = struct.pack("<I", len(data)) + data
    if not k32.WriteFile(h, frame, len(frame), ctypes.byref(w), None):
        raise RuntimeError("write failed")

def pipe_recv(h) -> bytes:
    def rx(n):
        buf = ctypes.create_string_buffer(n)
        got = 0
        while got < n:
            r = W.DWORD(0)
            if not k32.ReadFile(h, ctypes.byref(buf, got), n - got,
                                ctypes.byref(r), None) or r.value == 0:
                raise EOFError("pipe closed")
            got += r.value
        return buf.raw[:n]
    return rx(struct.unpack("<I", rx(4))[0])

def rpc(h, op, **fields):
    rid = rpc.n = getattr(rpc, "n", 0) + 1
    pipe_send(h, json.dumps({"id": rid, "op": op, **fields}).encode())
    return json.loads(pipe_recv(h))

def decode(v):
    if isinstance(v, dict):
        t = v.get("type")
        if t in ("null", "undefined"):
            return None
        if t == "object":
            return {kv["key"]: decode(kv["value"]) for kv in v.get("value", [])}
        if t == "array":
            return [decode(x) for x in v.get("value", [])]
        if "value" in v:
            return decode(v["value"]) if isinstance(v["value"], dict) else v["value"]
    return v

def ev(h, code):
    r = rpc(h, "eval", code=code)
    if not r.get("ok"):
        raise RuntimeError("eval: " + r.get("error", {}).get("message", "?"))
    return decode(r["value"])

# ---------------- ruyijs 原版辅助 ----------------
def with_sei(url, html):
    eid = (re.findall(r"var\s+eid\s*=\s*'([^']*)'", html) or [""])[-1]
    parts = urlsplit(url)
    query = [(k, v) for k, v in parse_qsl(parts.query, keep_blank_values=True)
             if k not in ("sg_ss", "sei", "eid")] + [("sei", eid)]
    return urlunsplit(parts._replace(query=urlencode(query))), eid

def parse_results(html):
    titles = [re.sub(r"<[^>]+>|\s+", " ", t).strip()
              for t in re.findall(r"<h3[^>]*>(.*?)</h3>", html, re.S)]
    cites = [re.sub(r"<[^>]+>", "", c).split("›")[0].strip()
             for c in re.findall(r"<cite[^>]*>(.*?)</cite>", html, re.S | re.I)]
    return [{"title": t, "host": (cites[i] if i < len(cites) else "")}
            for i, t in enumerate(titles) if t][:10]

# ---------------- compute_sg_ss (照抄原版, 引擎换成 ruyi-clone) ----------------
def compute_sg_ss(url, html, resp_headers=None):
    """原版语义: put_resource 保留 content-type + set-cookie → 引擎 jar。
    响应 Set-Cookie 注入 jar 后, VM 读 document.cookie 才与原版一致."""
    name = "\\\\.\\pipe\\ruyijs-" + uuid.uuid4().hex
    proc = subprocess.Popen([JS, "--ruyijs-rpc", name],
                            creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        h = None
        for _ in range(100):
            try:
                h = pipe_open(name)
                break
            except (RuntimeError, OSError):
                time.sleep(0.1)
        if not h:
            raise RuntimeError("engine pipe never appeared")
        rpc(h, "event_loop", action="configure", mode="system")
        if resp_headers:
            sc_all = (resp_headers.get("set-cookie") or
                      resp_headers.get("Set-Cookie") or "")
            from http.cookies import SimpleCookie
            sc = SimpleCookie()
            try:
                sc.load(sc_all.replace(", ", "\n").replace(",", "\n"))
            except Exception:
                sc = SimpleCookie()
                for piece in sc_all.split("; "):
                    if "=" in piece and "," not in piece.split("=")[0]:
                        try:
                            sc.load(piece.split(";")[0])
                        except Exception:
                            pass
            for k, morsel in sc.items():
                ev(h, "document.cookie=%s" % json.dumps(
                    "%s=%s; path=/" % (k, morsel.value)))
        # 对齐原版 profile (hardwareConcurrency/language 参与 token 计算)
        rpc(h, "configure", environment={"fingerprint": {"navigator": {
            "hardwareConcurrency": 16,
            "language": "en-US", "languages": ["en-US", "en"],
            "cookieEnabled": True, "onLine": True,
            "doNotTrack": "unspecified"}}})
        lr = rpc(h, "page_load", url=url, html=html)
        info = decode(lr.get("value"))
        deadline = time.time() + 45
        i = 0
        while time.time() < deadline:
            i += 1
            rpc(h, "event_loop", action="advance", ms=10, max_tasks=1)
            time.sleep(0.012)  # 与 advance(10ms) 对齐, VM 测定时器节奏
            c = ev(h, "document.cookie") or ""
            m = re.search(r"(?:^|;\s*)SG_SS=([^;]+)", c)
            if m:
                return m.group(1), i, info
        return None, i, info
    finally:
        try:
            proc.kill()
        except Exception:
            pass

def main():
    query = sys.argv[1] if len(sys.argv) > 1 else "site:github.com golang"
    url = ("https://www.google.com/search?q=" + query.replace(" ", "+") +
           "&udm=14&hl=en")
    sess = cr.Session()
    verdict = None
    for attempt in range(3):
        r = sess.get(url, headers=ff_headers(), proxy=PROXY,
                     impersonate=IMP, allow_redirects=True, timeout=60)
        html = r.text
        print(f"[attempt{attempt}] {r.status_code} len={len(html)} "
              f"SG_SS页={'SG_SS' in html} sorry={'/sorry/' in str(r.url)}")
        if "/sorry/" in str(r.url):
            verdict = "blocked"
            break
        if "SG_SS" in html:
            sgss, rounds, info = compute_sg_ss(str(r.url), html,
                                               resp_headers=r.headers)
            if not sgss:
                print("解算失败; loadPage info:", json.dumps(info)[:120])
                verdict = "challenge"
                break
            print(f"    ★ SG_SS 解出({rounds}轮): {sgss[:32]}...")
            from urllib.parse import urlsplit as _us
            _host = _us(str(r.url)).hostname or "www.google.com.hk"
            sess.cookies.set("SG_SS", sgss, domain=_host, path="/")
            nxt, eid = with_sei(str(r.url), html)
            r = sess.get(nxt, headers=ff_headers(referer=str(r.url),
                                                 site="same-origin"),
                         proxy=PROXY, impersonate=IMP, timeout=60)
            html = r.text
            print(f"    重访: {r.status_code} len={len(html)}")
        results = parse_results(html)
        if len(results) >= 3:
            print(f"★ SERP 通过: {len(results)} 条结果")
            for i, it in enumerate(results[:6], 1):
                print(f"   {i}. {it['title'][:56]:56s} | {it['host']}")
            verdict = "passed"
            break
        verdict = "no_results"
        break
    print("=== 结果:", verdict, "===")
    return 0 if verdict == "passed" else 1

if __name__ == "__main__":
    sys.exit(main())
