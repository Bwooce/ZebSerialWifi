#!/usr/bin/env python3
"""Syntax-check the page JavaScript with node --check.

Checks:
  - every `static const char ...SCRIPT...[] =` array in firmware/main/web.c: the adjacent C string literals are
    joined, their escapes decoded and a surrounding <script>...</script> stripped;
  - any other run of adjacent string literals in web.c that holds a whole <script>...</script> (such as the OTA
    upload script), as long as it has no printf conversion in it;
  - the inline <script> body of tools/cable-labels.html.

Scripts built at run time with snprintf (send_ui_script) cannot be checked here.

Usage: tools/check_page_js.py [--web-c PATH] [--html PATH]
Exits 1 if node reports a syntax error, or if fewer scripts are found than expected (a parsing fault must not pass).
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MIN_WEB_C_ARRAYS = 1  # STATUS_SCRIPT
MIN_HTML_SCRIPTS = 1

SIMPLE_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", "'": "'", '"': '"', "?": "?",
                  "a": "\a", "b": "\b", "f": "\f", "v": "\v"}


def decode_c_string(body, where):
    out = []
    i = 0
    while i < len(body):
        c = body[i]
        if c != "\\":
            out.append(c)
            i += 1
            continue
        e = body[i + 1]
        if e == "x":
            m = re.match(r"[0-9A-Fa-f]+", body[i + 2:])
            out.append(chr(int(m.group(0), 16)))
            i += 2 + len(m.group(0))
        elif e in "01234567":
            m = re.match(r"[0-7]{1,3}", body[i + 1:])
            out.append(chr(int(m.group(0), 8)))
            i += 1 + len(m.group(0))
        elif e in SIMPLE_ESCAPES:
            out.append(SIMPLE_ESCAPES[e])
            i += 2
        else:
            sys.exit(f"{where}: unknown escape \\{e}")
    return "".join(out)


def tokens(src):
    """Yield (kind, text, line) for C source: 'str' for a string literal's raw body, 'other' for anything else.
    Comments and character constants are skipped."""
    i, line, n = 0, 1, len(src)
    while i < n:
        c = src[i]
        if src.startswith("/*", i):
            j = src.index("*/", i + 2) + 2
        elif src.startswith("//", i):
            j = src.find("\n", i)
            j = n if j < 0 else j
        elif c == '"' or c == "'":
            j = i + 1
            while src[j] != c:
                j += 2 if src[j] == "\\" else 1
            j += 1
            if c == '"':
                yield "str", src[i + 1:j - 1], line
        elif c.isspace():
            j = i + 1
        else:
            j = i + 1
            yield "other", c, line
        line += src.count("\n", i, j)
        i = j


def literal_runs(src):
    """Return [(first line, decoded text, preceding code)] for each run of adjacent string literals."""
    runs, cur, start, before, code = [], [], 0, "", []
    for kind, text, line in tokens(src):
        if kind == "str":
            if not cur:
                start, before = line, "".join(code[-200:])
            cur.append(text)
        else:
            if cur:
                runs.append((start, "".join(cur), before))
                cur = []
            code.append(text)
    if cur:
        runs.append((start, "".join(cur), before))
    return [(ln, decode_c_string(raw, f"web.c:{ln}"), before) for ln, raw, before in runs]


def strip_script_tags(js, where):
    m = re.fullmatch(r"\s*<script>(.*)</script>\s*", js, re.S)
    if m:
        return m.group(1)
    if "<script" in js or "</script" in js:
        sys.exit(f"{where}: <script> tags not round the whole literal")
    return js


def web_c_scripts(path):
    src = open(path, encoding="utf-8").read()
    found, arrays = [], 0
    for ln, text, before in literal_runs(src):
        m = re.search(r"static\s*const\s*char\s*\*?\s*(\w*SCRIPT\w*)\s*\[\s*\]\s*=$", before)
        if m:
            arrays += 1
            found.append((f"{show(path)}:{ln} {m.group(1)}", strip_script_tags(text, f"web.c:{ln}")))
            continue
        for sm in re.finditer(r"<script>(.*?)</script>", text, re.S):
            if re.search(r"%[-+ #0-9.]*[sdiuxXcpfgl]", sm.group(1)):
                continue  # a printf format: only the formatted output is JavaScript
            found.append((f"{show(path)}:{ln} inline script", sm.group(1)))
    if arrays < MIN_WEB_C_ARRAYS:
        sys.exit(f"{path}: found {arrays} SCRIPT arrays, expected at least {MIN_WEB_C_ARRAYS}")
    return found


def html_scripts(path):
    src = open(path, encoding="utf-8").read()
    bodies = re.findall(r"<script>(.*?)</script>", src, re.S)
    if len(bodies) < MIN_HTML_SCRIPTS:
        sys.exit(f"{path}: found {len(bodies)} inline scripts, expected at least {MIN_HTML_SCRIPTS}")
    return [(f"{show(path)} script {k + 1}", b) for k, b in enumerate(bodies)]


def show(path):
    rel = os.path.relpath(os.path.realpath(path), ROOT)
    return path if rel.startswith("..") else rel


def node_error(stderr):
    """node prints the whole offending line, which is the whole script here: keep 60 characters each side."""
    lines = stderr.splitlines()
    for k in range(1, len(lines) - 1):
        col = lines[k + 1].find("^")
        if col >= 0 and set(lines[k + 1].strip()) == {"^"}:
            lo = max(0, col - 60)
            err = next((x for x in lines[k + 2:] if re.match(r"\w*Error", x)), "")
            return f"      ...{lines[k][lo:col + 60]}...\n         {' ' * (col - lo)}^\n      {err}"
    return stderr


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--web-c", default=os.path.join(ROOT, "firmware/main/web.c"))
    ap.add_argument("--html", default=os.path.join(ROOT, "tools/cable-labels.html"))
    args = ap.parse_args()
    scripts = web_c_scripts(args.web_c) + html_scripts(args.html)
    fails = 0
    with tempfile.TemporaryDirectory() as tmp:
        for k, (name, js) in enumerate(scripts):
            f = os.path.join(tmp, f"script{k}.js")
            with open(f, "w", encoding="utf-8") as fh:
                fh.write(js)
            r = subprocess.run(["node", "--check", f], capture_output=True, text=True)
            if r.returncode == 0:
                print(f"ok    {name} ({len(js)} chars)")
            else:
                fails += 1
                print(f"FAIL  {name}\n{node_error(r.stderr)}")
    print(f"{len(scripts)} scripts, {fails} failures")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
