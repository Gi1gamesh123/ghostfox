# -*- coding: utf-8 -*-
"""ghostfox Google 端到端 —— Linux 引擎 + stdio RPC 版
  curl_cffi(Firefox TLS 指纹) = Necko 网络进程
  ghostfox js(Linux 自编译 SpiderMonkey) = 解算器
流程: navigate -> SG_SS interstitial -> 引擎解 cookie -> 带 SG_SS+sei 重访 -> SERP
用法: python3 google_min_linux.py [查询词]
"""
import json, re, sys, time, struct, subprocess
from urllib.parse import urlsplit, urlunsplit, parse_qsl, urlencode

from curl_cffi import requests as cr

JS = "/root/ghostfox/firefox-158.0/obj-x86_64-pc-linux-gnu/dist/bin/js"
PROXY = "http://172.31.32.1:7897"
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


# ---------------- js stdio RPC ----------------
class Engine:
    def __init__(self):
        self.proc = subprocess.Popen([JS, "--rpc", "stdio", "--mode", "default"],
                                     stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.DEVNULL)

    def _send(self, data: bytes):
        frame = struct.pack("<I", len(data)) + data
        self.proc.stdin.write(frame)
        self.proc.stdin.flush()

    def _recv(self) -> bytes:
        def rx(n):
            buf = b""
            while len(buf) < n:
                piece = self.proc.stdout.read(n - len(buf))
                if not piece:
                    raise EOFError("engine closed")
                buf += piece
            return buf
        return rx(struct.unpack("<I", rx(4))[0])

    def rpc(self, op, **fields):
        self.n = getattr(self, "n", 0) + 1
        self._send(json.dumps({"id": self.n, "op": op, **fields}).encode())
        return json.loads(self._recv())

    def close(self):
        try:
            self.proc.kill()
        except Exception:
            pass


def decode(v):
    if isinstance(v, dict):
        t = v.get("type")
        if t in ("null", "undefined"):
            return None
        if t == "object":
            val = v.get("value", [])
            if val and isinstance(val[0], dict):
                return {kv["key"]: decode(kv["value"]) for kv in val}
            return {k: decode(x) for k, x in val}
        if t == "array":
            return [decode(x) for x in v.get("value", [])]
        if "value" in v:
            return decode(v["value"]) if isinstance(v["value"], dict) else v["value"]
    return v


def ev(engine, code):
    r = engine.rpc("eval", code=code)
    if not r.get("ok"):
        raise RuntimeError("eval: " + r.get("error", {}).get("message", "?"))
    return decode(r["value"])


# ---------------- 原版辅助 ----------------
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


def compute_sg_ss(url, html, resp_headers=None):
    """put_resource 保留 set-cookie → 引擎 jar; VM 解 SG_SS cookie."""
    sys.path.insert(0, "/mnt/e/ruyi-clone/reference/examples/google")
    from google_search import PROFILE
    engine = Engine()
    try:
        engine.rpc("event_loop", action="configure", mode="system")
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
                ev(engine, "document.cookie=%s" % json.dumps(
                    "%s=%s; path=/" % (k, morsel.value)))
        r = engine.rpc("configure", environment=json.loads(json.dumps(PROFILE)))
        if not r.get("ok"):
            print("configure 失败:", json.dumps(r.get("error", {}))[:160])
        lr = engine.rpc("page", action="load", url=url, html=html)
        info = decode(lr.get("value"))
        deadline = time.time() + 45
        i = 0
        while time.time() < deadline:
            i += 1
            engine.rpc("event_loop", action="advance", ms=10, max_tasks=1)
            time.sleep(0.012)
            c = ev(engine, "document.cookie") or ""
            m = re.search(r"(?:^|;\s*)SG_SS=([^;]+)", c)
            if m:
                return m.group(1), i, info
        return None, i, info
    finally:
        engine.close()


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
