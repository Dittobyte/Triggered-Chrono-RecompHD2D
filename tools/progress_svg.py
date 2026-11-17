#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 jerbanpls
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draw the README's progress picture (docs/progress.svg).

usage: progress_svg.py [RUNS_TOML [PROGRESS_MD [OUT_SVG]]]
  defaults: game/ct/progress_runs.toml  game/ct/PROGRESS.md  docs/progress.svg

Bars: known functions compiled (counts read from PROGRESS.md, written by tools/progress.py);
the share of executed instructions that ran as native code on each recorded replay (the
measured values in RUNS_TOML, with a hatched extension to the figure of a branch still in
testing); a hatched empty bar for the whole game, which is not measured yet; and the steps to
an interpreter-free recorded playthrough (done / testing / todo).

Regenerate at every release: edit the values in game/ct/progress_runs.toml (measured ones from
`ct_boot --profile`), run `tools/progress.py game/ct build` if the function counts changed,
then run this script and commit docs/progress.svg. Text only, no fonts embedded, no game data.
"""
import os
import re
import sys
import tomllib
from xml.sax.saxutils import escape

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BG, BORDER, TEXT, MUTED = "#0e1621", "#243447", "#e6edf3", "#8b9bb0"
TRACK, OK, WARN, TODO = "#1d2b3c", "#3fb6a8", "#e0a84a", "#5b6b80"
FONT = "-apple-system, 'Segoe UI', Helvetica, Arial, sans-serif"
W, PAD = 900, 28


def counts(progress_md):
    """(compiled, known) from the PROGRESS.md table."""
    text = open(progress_md, encoding="utf-8").read()
    def cell(label):
        m = re.search(r"\|\s*" + re.escape(label) + r"[^|]*\|\s*(\d+)\s*\|", text)
        if not m:
            raise SystemExit(f"progress_svg: no '{label}' row in {progress_md}")
        return int(m.group(1))
    return cell("Routines recompiled"), cell("Functions known total")


def t(x, y, s, size=13, fill=TEXT, weight="normal", anchor="start"):
    return (f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" fill="{fill}" '
            f'text-anchor="{anchor}">{escape(s)}</text>')


def bar(x, y, w, h, frac, fill):
    frac = max(0.0, min(1.0, frac))
    out = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="4" fill="{TRACK}"/>']
    if frac > 0:
        out.append(f'<rect x="{x}" y="{y}" width="{round(w * frac, 1)}" height="{h}" rx="4" fill="{fill}"/>')
    return out


def render(runs_toml, progress_md):
    data = tomllib.load(open(runs_toml, "rb"))
    meta, runs, steps = data["meta"], data["run"], data["step"]
    done, known = counts(progress_md)
    bx, bw = 250, 440
    el = []
    y = 44
    el.append(t(PAD, y, "Triggered Chrono: road to a fully native Chrono Trigger", 22, TEXT, "bold"))
    y += 22
    el.append(t(PAD, y, f"Measured {meta['updated']} on commit {meta['commit']} ({meta['release']}). "
                        "Native = compiled C; the rest runs in the built-in interpreter.", 12.5, MUTED))

    y += 38
    el.append(t(PAD, y, "Known functions compiled to native C", 14, TEXT, "bold"))
    y += 14
    el += bar(PAD, y, bx + bw - PAD, 22, done / known, OK)
    el.append(t(bx + bw + 16, y + 16, f"{100.0 * done / known:.1f}%  ({done} of {known})", 13))

    y += 56
    el.append(t(PAD, y, "Instructions that run as native code, per recorded replay", 14, TEXT, "bold"))
    y += 10
    for r in runs:
        y += 32
        el.append(t(PAD, y + 4, r["name"], 12.5, MUTED))
        el += bar(bx, y - 12, bw, 18, r["native"] / 100.0, OK)
        label = f"{r['native']:.2f}%"
        if "testing" in r:
            x0 = bx + bw * r["native"] / 100.0
            x1 = bx + bw * r["testing"] / 100.0
            el.append(f'<rect x="{round(x0, 1)}" y="{y - 12}" width="{round(x1 - x0, 1)}" height="18" '
                      f'fill="url(#hatch)" stroke="{WARN}" stroke-width="1"/>')
            label += f"  next: {r['testing']:.2f}%"
        el.append(t(bx + bw + 16, y + 3, label, 12.5))
    y += 22
    el.append(f'<rect x="{PAD}" y="{y}" width="12" height="10" fill="url(#hatch)" stroke="{WARN}" stroke-width="1"/>')
    el.append(t(PAD + 18, y + 9, "figure from the next compiler round, still being tested (not a result yet)", 11.5, MUTED))

    y += 44
    el.append(t(PAD, y, "The whole game, every scene", 14, TEXT, "bold"))
    y += 14
    el.append(f'<rect x="{PAD}" y="{y}" width="{bx + bw - PAD}" height="22" rx="4" fill="url(#hatch2)" '
              f'stroke="{TODO}" stroke-width="1"/>')
    el.append(t(bx + bw + 16, y + 16, "not measured yet", 13, MUTED))
    y += 40
    el.append(t(PAD, y, "These figures cover the replays we have (intro, Leene Square, first battle, world map, menu). "
                        "Whole-game coverage needs recordings of full playthroughs.", 11.5, MUTED))

    y += 40
    el.append(t(PAD, y, "Steps to an interpreter-free recorded playthrough", 14, TEXT, "bold"))
    colors = {"done": OK, "testing": WARN, "todo": TODO}
    marks = {"done": "done", "testing": "in testing", "todo": "to do"}
    for s in steps:
        y += 26
        c = colors[s["state"]]
        if s["state"] == "todo":
            el.append(f'<circle cx="{PAD + 7}" cy="{y - 4}" r="6" fill="none" stroke="{c}" stroke-width="2"/>')
        else:
            el.append(f'<circle cx="{PAD + 7}" cy="{y - 4}" r="6" fill="{c}"/>')
        el.append(t(PAD + 24, y, s["name"], 13, TEXT if s["state"] != "todo" else MUTED))
        el.append(t(W - PAD, y, marks[s["state"]], 12, c, "normal", "end"))
    y += 26
    el.append(t(PAD, y, "The interpreter stays in the program as the test referee and last-resort fallback; "
                        "the goal is that play never reaches it.", 11.5, MUTED))
    H = y + 26

    head = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
            f'font-family="{FONT}" role="img" aria-labelledby="ttl">'
            f'<title id="ttl">Triggered Chrono recompile progress</title>'
            f'<defs><pattern id="hatch" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
            f'<rect width="8" height="8" fill="{TRACK}"/><rect width="4" height="8" fill="{WARN}" fill-opacity="0.55"/></pattern>'
            f'<pattern id="hatch2" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">'
            f'<rect width="8" height="8" fill="{BG}"/><rect width="4" height="8" fill="{TODO}" fill-opacity="0.35"/></pattern></defs>'
            f'<rect x="0.5" y="0.5" width="{W - 1}" height="{H - 1}" rx="10" fill="{BG}" stroke="{BORDER}"/>')
    return head + "\n" + "\n".join(el) + "\n</svg>\n"


def main(argv):
    runs = argv[1] if len(argv) > 1 else os.path.join(ROOT, "game/ct/progress_runs.toml")
    prog = argv[2] if len(argv) > 2 else os.path.join(ROOT, "game/ct/PROGRESS.md")
    out = argv[3] if len(argv) > 3 else os.path.join(ROOT, "docs/progress.svg")
    svg = render(runs, prog)
    with open(out, "w", encoding="utf-8") as f:
        f.write(svg)
    print(f"progress_svg: wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
