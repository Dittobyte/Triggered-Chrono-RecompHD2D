#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 jerbanpls
# SPDX-License-Identifier: GPL-3.0-or-later
"""tools/progress_svg.py: the README progress picture is valid SVG, shows exactly the numbers in the data,
is deterministic, and the committed docs/progress.svg is up to date with game/ct/progress_runs.toml.

usage: test_progress_svg.py SOURCE_DIR
"""
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

src = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tool = os.path.join(src, "tools", "progress_svg.py")

FAKE_RUNS = """[meta]
title = "Fake title"
replays_note = "Fake replays note."
updated = "2000-01-02"
commit = "abc1234"
release = "v9.9.9"
[[run]]
name = "Fake run one"
native = 91.25
testing = 99.50
[[run]]
name = "Fake run two"
native = 50.00
[[step]]
name = "Step done"
state = "done"
[[step]]
name = "Step testing"
state = "testing"
[[step]]
name = "Step todo"
state = "todo"
"""
FAKE_PROGRESS = "| Metric | Value |\n|---|---|\n| Routines recompiled | 300 |\n| Functions known total (validated + pending) | 400 |\n"


def run(runs, prog, out):
    r = subprocess.run([sys.executable, tool, runs, prog, out], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return open(out, encoding="utf-8").read()


with tempfile.TemporaryDirectory() as tmp:
    runs, prog = os.path.join(tmp, "r.toml"), os.path.join(tmp, "p.md")
    open(runs, "w").write(FAKE_RUNS)
    open(prog, "w").write(FAKE_PROGRESS)
    a = run(runs, prog, os.path.join(tmp, "a.svg"))
    b = run(runs, prog, os.path.join(tmp, "b.svg"))
    assert a == b, "output is not deterministic"
    root = ET.fromstring(a)   # well-formed XML
    assert root.tag.endswith("svg")
    text = " ".join("".join(e.itertext()) for e in root.iter() if e.tag.endswith("text"))
    for want in ("75.0%", "300 of 400", "91.25%", "99.50%", "50.00%", "Fake run one", "Step todo",
                 "2000-01-02", "abc1234", "v9.9.9", "not measured yet", "Fake title", "Fake replays note."):
        assert want in text, f"missing {want!r}"
    assert "url(#hatch)" in a and "<script" not in a and "http://www.w3.org/2000/svg" in a
    print("progress_svg: fake data ok")

# the committed picture matches the committed data
real = os.path.join(src, "docs", "progress.svg")
if os.path.exists(real):
    with tempfile.TemporaryDirectory() as tmp:
        fresh = run(os.path.join(src, "game/ct/progress_runs.toml"), os.path.join(src, "game/ct/PROGRESS.md"),
                    os.path.join(tmp, "x.svg"))
    assert fresh == open(real, encoding="utf-8").read(), \
        "docs/progress.svg is out of date: run python3 tools/progress_svg.py and commit it"
    print("progress_svg: docs/progress.svg is up to date")
