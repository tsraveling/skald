#!/usr/bin/env python3
# Smoke-test the Skald LSP on a file: prints diagnostics.
# Usage: python3 lsp/diag.py /abs/path/to/file.ska [project_root]
import subprocess, json, sys, os
LSP = os.path.expanduser("~/repos/skald/build/skald_lsp")
path = os.path.abspath(sys.argv[1])
root = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.dirname(path)
def frame(o):
    b = json.dumps(o).encode(); return b"Content-Length: %d\r\n\r\n%b" % (len(b), b)
uri = "file://" + path
msgs = [
  {"jsonrpc":"2.0","id":1,"method":"initialize","params":{"rootUri":"file://"+root}},
  {"jsonrpc":"2.0","method":"initialized","params":{}},
  {"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{
     "uri":uri,"languageId":"skald","version":1,"text":open(path).read()}}},
  {"jsonrpc":"2.0","id":99,"method":"shutdown","params":{}},
  {"jsonrpc":"2.0","method":"exit","params":{}},
]
out = subprocess.run([LSP], input=b"".join(frame(m) for m in msgs), capture_output=True).stdout
i = 0
while i < len(out):
    j = out.find(b"\r\n\r\n", i)
    if j < 0: break
    hs = [h for h in out[i:j].decode().split("\r\n") if h.startswith("Content-Length")]
    if not hs: break
    cl = int(hs[0].split(":")[1]); body = out[j+4:j+4+cl]; i = j+4+cl
    r = json.loads(body)
    if r.get("method") == "textDocument/publishDiagnostics":
        p = r["params"]; f = p["uri"].split("/")[-1]
        if not p["diagnostics"]: print(f"[{f}] (no diagnostics)")
        for d in p["diagnostics"]:
            sev = {1:"ERROR",2:"WARN",3:"INFO",4:"HINT"}[d["severity"]]
            s = d["range"]["start"]
            print(f"[{f}] L{s['line']+1}:{s['character']} {sev}: {d['message']}")
