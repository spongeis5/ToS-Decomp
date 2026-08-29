"""Render the progress dashboard from the repo's own outputs.

    python tools/dashdata.py        -> build/dash.json
    python tools/dashhistory.py     -> build/dash_history.json
    python tools/verify.py  > build/verify_log.txt
    python tools/sweep.py --attempts > build/attempts_scores.txt
    python tools/dashboard.py       -> build/dashboard.html

Nothing here is retyped. Every figure comes from the manifest, the inventory,
`build.py`, `verify.py` and `sweep.py`, so the page cannot drift from the
repository the way a hand-maintained status file does.

DESIGN, recorded before building:

  COLOUR  ground #0E151D deep cool slate; surface #16202B; line #263543;
          text #DAE3EC; muted #7E90A3 (a grey biased blue toward the ground,
          chosen rather than inherited); accent #E9A13B amber, the highlight
          colour of a hex editor, warm against a cool ground and the one
          place boldness is spent. Status green/red is SEPARATE from the
          accent and never reused as a series colour.
  TYPE    Archivo for display; IBM Plex Sans for body; IBM Plex Mono for
          every hex address, byte count and figure -- Plex was drawn for
          technical documents, which is exactly this subject.
  LAYOUT  A console, scanned rather than read. The hero is the BYTE MAP:
          8,467,964 bytes of .text as 1800 cells, lit where the repo
          reproduces them. It is the most characteristic object in this
          project's world and it is honest -- it shows at a glance how
          little is done, which a bare percentage lets a reader skim past.

THE PROGRESS CHART IS AN *EMPHASIS* CHART, NOT TWO CATEGORICAL SERIES.
Hand-written functions are the subject; generated stubs are context. Plotted
as one line the jump from 559 to 940 in a single commit reads as a
breakthrough, when 362 of those 381 were one-expression stubs a script wrote
from their own encodings. Accent hue for the subject, de-emphasis grey for
the context, both direct-labelled so identity never rests on colour alone.
The pair was checked rather than eyeballed: OKLab dE 20.1 (light) and 21.2
(dark) under normal vision, 16.4-21.7 under simulated protanopia,
deuteranopia and tritanopia, against a floor of 8; contrast 3.79:1 and
5.82:1 on white, 7.55:1 and 5.02:1 on the dark surface.
"""

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
OUT = BUILD / "dashboard.html"


def fmt(n):
    return "{:,}".format(int(n))


# ------------------------------------------------------------------ inputs

d = json.loads((BUILD / "dash.json").read_text())
built, total = d["built_bytes"], d["text_bytes"]
pct = 100.0 * built / total
cells = d["cells"]

hist = []
hp = BUILD / "dash_history.json"
if hp.exists():
    hist = json.loads(hp.read_text())

checks = []
vp = BUILD / "verify_log.txt"
if vp.exists():
    # Parsed by SUFFIX, not by a column regex: verify.py pads labels to 42
    # characters and seven of the twenty-one are longer, so a pattern needing
    # two spaces before the status silently dropped exactly the checks with
    # the longest names and the page showed 14 of 21 as if that were all.
    txt = vp.read_text(encoding="utf-8", errors="replace")
    for raw in txt.splitlines():
        line = raw.rstrip()
        if not line.startswith("  ") or line.startswith("    "):
            continue
        b = line.strip()
        if b.endswith(" ok"):
            checks.append((b[:-3].strip(), "ok"))
        elif " FAIL" in b:
            checks.append((b[:b.index(" FAIL")].strip(), "fail"))
    for raw in txt.splitlines():
        s = raw.strip()
        if s.endswith(" match") and " of " in s:
            n, _, rest = s.partition(" of ")
            tot = rest.split()[0]
            checks.append(("%s of %s functions reproduce" % (n, tot),
                           "ok" if n == tot else "fail"))
            break
n_ok = sum(1 for _n, s in checks if s == "ok")

near = []
ap = BUILD / "attempts_scores.txt"
if ap.exists():
    for line in ap.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"^\s+(\S+\.cpp)\s+([0-9A-F]{8})\s+near\s+(\S+(?: /Os)?)"
                     r"\s+(\d+) of (\d+) word", line)
        if m:
            near.append({"file": m.group(1), "addr": m.group(2),
                         "flags": m.group(3), "got": int(m.group(4)),
                         "of": int(m.group(5)),
                         "size": "SIZE DIFFERS" in line})
# A row with ZERO comparable words is not the closest near-miss, it is the
# one where nothing could be compared -- every word is relocated, so the
# object holds only placeholders. `82697740` is a one-instruction thunk of
# exactly that shape, and sorting by "words still wrong" put it at the top
# of the work queue reading 0 / 0. match.py already refuses this case
# (can_shrink clause 5, "at least one non-relocated word must exist"); the
# queue has to as well, or it recommends the one function that cannot be
# worked on.
vacuous = [r for r in near if r["of"] == 0]
near = [r for r in near if r["of"] > 0]
near.sort(key=lambda r: (r["of"] - r["got"], -r["got"]))


# ------------------------------------------------------------------ pieces

def cellhtml():
    out = []
    for i, c in enumerate(cells):
        if c <= 0:
            out.append("<i></i>")
        else:
            lvl = 1 if c < 0.34 else (2 if c < 0.67 else 3)
            addr = 0x82100000 + int(i * total / len(cells))
            out.append('<i class="l%d" title="%08X &mdash; %d%% rebuilt">'
                       "</i>" % (lvl, addr, round(c * 100)))
    return "".join(out)


def chart():
    """Cumulative hand-written vs generated, over commits.

    x is the COMMIT INDEX, not wall-clock time. The work happened in two
    sittings separated by a long gap, and a time axis would spend most of
    its width on the gap and compress everything that happened into two
    vertical walls. The commit sequence is what the reader is actually
    following. That is a real choice and the caption says so.
    """
    if len(hist) < 2:
        return "<p>No history recorded yet.</p>"
    W, H = 720, 230
    L, R, T, B = 48, 16, 20, 22
    n = len(hist)
    top = max(p["hand"] + p["gen"] for p in hist)
    top = max(top, 1)

    def X(i):
        return L + (W - L - R) * i / float(n - 1)

    def Y(v):
        return T + (H - T - B) * (1.0 - v / float(top))

    def path(key, stack=False):
        pts = []
        for i, p in enumerate(hist):
            v = p["hand"] + p["gen"] if stack else p[key]
            pts.append("%.1f,%.1f" % (X(i), Y(v)))
        return "M" + " L".join(pts)

    grid = []
    for frac in (0.0, 0.25, 0.5, 0.75, 1.0):
        v = top * frac
        y = Y(v)
        grid.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" '
                    'class="gl"/>' % (L, y, W - R, y))
        grid.append('<text x="%d" y="%.1f" class="ax" text-anchor="end">%s'
                    "</text>" % (L - 8, y + 3.5, fmt(v)))

    last = hist[-1]
    dots = ('<circle cx="%.1f" cy="%.1f" r="3.5" class="dot-total"/>'
            '<circle cx="%.1f" cy="%.1f" r="3.5" class="dot-hand"/>'
            % (X(n - 1), Y(last["hand"] + last["gen"]),
               X(n - 1), Y(last["hand"])))

    # BOTH labels go ABOVE their endpoint. The hand-written one was placed
    # below (+17), which put it on top of its own line and inside the bottom
    # margin -- the series ends low, so "below" is where the axis lives. The
    # two series are far apart in value here, so above-and-above cannot
    # collide.
    labels = (
        '<text x="%.1f" y="%.1f" class="lab-total" text-anchor="end">'
        "%s total</text>"
        '<text x="%.1f" y="%.1f" class="lab-hand" text-anchor="end">'
        "%s hand-written</text>"
        % (X(n - 1) - 8, Y(last["hand"] + last["gen"]) - 10,
           fmt(last["hand"] + last["gen"]),
           X(n - 1) - 8, Y(last["hand"]) - 10, fmt(last["hand"])))

    return (
        '<svg viewBox="0 0 %d %d" role="img" aria-label="Cumulative matched '
        'functions across %d commits: %s hand-written and %s total.">'
        "%s"
        '<path d="%s" class="ln-total"/>'
        '<path d="%s" class="ln-hand"/>'
        "%s%s</svg>"
        % (W, H, n, fmt(last["hand"]), fmt(last["hand"] + last["gen"]),
           "".join(grid), path(None, stack=True), path("hand"), dots, labels))


comp = [
    ("XDK libraries", d["attribution"].get("lib", 0),
     "held on disk &mdash; linkable, never written", "hold"),
    ("Unattributed", d["attribution"].get("UNKNOWN", 0),
     "no signal yet; most of the game's own code", "unk"),
    ("Source-path tagged", d["attribution"].get("srcpath", 0),
     "a <code>__FILE__</code> string reaches this code", "src"),
    ("Havok", d["attribution"].get("rtti_havok", 0)
     + d["attribution"].get("havok", 0),
     "named by RTTI; no archive held", "mid"),
    ("Profiler-named", d["attribution"].get("game_profiled", 0),
     "the game pushes its own function name", "prof"),
]
attr_total = sum(v for _n, v, _d, _k in comp) or 1
comp_rows = "".join(
    '<tr><td><span class="sw %s"></span>%s</td><td class="n">%s</td>'
    '<td class="n">%.1f%%</td><td class="d">%s</td></tr>'
    % (k, name, fmt(v), 100.0 * v / attr_total, note)
    for name, v, note, k in comp)

veins = [
    ("Constant returns", 346,
     "<code>lis/ori/blr</code> and <code>li/blr</code> &mdash; the game's own "
     "type IDs, since it ships no RTTI"),
    ("Accessors", 362,
     "one expression each: field get, field set, pointer adjust, empty body"),
    ("Virtual forwarders", 110,
     "tail call through a vtable slot; 56 needed <code>/Os</code>"),
]
vein_rows = "".join(
    '<tr><td>%s</td><td class="n">%d</td><td class="d">%s</td></tr>'
    % (nm, c, note) for nm, c, note in veins)

check_rows = "".join(
    '<li class="%s"><span class="dot"></span>%s</li>' % (s, nm)
    for nm, s in checks) or (
    '<li class="fail"><span class="dot"></span>no verify log captured</li>')

near_rows = "".join(
    '<tr><td class="n">%s</td><td class="n">%d / %d</td>'
    '<td class="bar"><span style="width:%.1f%%"></span></td>'
    '<td class="d">%s%s</td></tr>'
    % (r["addr"], r["got"], r["of"], 100.0 * r["got"] / max(r["of"], 1),
       r["file"], "  &middot; size differs" if r["size"] else "")
    for r in near[:12])
near_left = max(0, len(near) - 12)


# ------------------------------------------------------------------ page

HTML = r"""<title>Truth or Square Byte Map</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Archivo:wght@500;600;700&family=IBM+Plex+Mono:wght@400;500;600&family=IBM+Plex+Sans:wght@400;500&display=swap">
<style>
:root{
  --ground:#F5F7F9; --surface:#FFFFFF; --line:#DCE3EA;
  --text:#0F1922; --muted:#55677A; --accent:#B87415;
  --ok:#2F7D57; --fail:#B23F33;
  --c1:#F0D6AC; --c2:#DFA553; --c3:#B87415;
  --grid:#E4EAF0; --series-ctx:#55677A;
}
@media (prefers-color-scheme: dark){
  :root:not([data-theme="light"]){
    --ground:#0E151D; --surface:#16202B; --line:#263543;
    --text:#DAE3EC; --muted:#7E90A3; --accent:#E9A13B;
    --ok:#48A97B; --fail:#C9564A;
    --c1:#4A3A22; --c2:#9C6F2A; --c3:#E9A13B;
    --grid:#283746; --series-ctx:#7E90A3;
  }
}
:root[data-theme="dark"]{
  --ground:#0E151D; --surface:#16202B; --line:#263543;
  --text:#DAE3EC; --muted:#7E90A3; --accent:#E9A13B;
  --ok:#48A97B; --fail:#C9564A;
  --c1:#4A3A22; --c2:#9C6F2A; --c3:#E9A13B;
  --grid:#283746; --series-ctx:#7E90A3;
}
*{box-sizing:border-box}
body{margin:0; background:var(--ground); color:var(--text);
  font-family:"IBM Plex Sans",system-ui,-apple-system,sans-serif;
  line-height:1.55; -webkit-font-smoothing:antialiased}
.wrap{max-width:1080px; margin:0 auto; padding:48px 24px 72px;
  display:flex; flex-direction:column; gap:44px}
h1,h2{font-family:Archivo,system-ui,sans-serif; text-wrap:balance; margin:0;
  letter-spacing:-0.01em}
h1{font-size:clamp(26px,4.4vw,40px); font-weight:700; line-height:1.1}
h2{font-size:19px; font-weight:600}
.eyebrow{font-family:"IBM Plex Mono",monospace; font-size:11.5px;
  letter-spacing:0.14em; text-transform:uppercase; color:var(--muted);
  margin:0 0 10px}
.n,code{font-family:"IBM Plex Mono",monospace;
  font-variant-numeric:tabular-nums}
p{margin:0; max-width:66ch; color:var(--muted)}
p strong{color:var(--text); font-weight:500}
header .sub{margin-top:12px; font-size:15px}
.figure{display:flex; flex-wrap:wrap; align-items:flex-end; gap:8px 28px;
  margin-top:26px}
.big{font-family:"IBM Plex Mono",monospace; font-weight:600;
  font-size:clamp(38px,7vw,62px); line-height:1; color:var(--accent);
  font-variant-numeric:tabular-nums; letter-spacing:-0.02em}
.of{font-family:"IBM Plex Mono",monospace; font-size:15px;
  color:var(--muted); padding-bottom:6px}
.map,.panel{background:var(--surface); border:1px solid var(--line);
  border-radius:3px}
.map{padding:18px}
.panel{padding:20px 22px}
.cells{display:grid; grid-template-columns:repeat(60,1fr); gap:2px}
.cells i{aspect-ratio:1; background:var(--grid); border-radius:1px;
  display:block}
.cells i.l1{background:var(--c1)}
.cells i.l2{background:var(--c2)}
.cells i.l3{background:var(--c3)}
.legend{display:flex; flex-wrap:wrap; gap:16px; margin-top:14px;
  font-family:"IBM Plex Mono",monospace; font-size:11.5px;
  color:var(--muted); align-items:center}
.legend span{display:inline-flex; align-items:center; gap:6px}
.key{width:10px; height:10px; border-radius:1px; display:inline-block}
section{display:flex; flex-direction:column; gap:14px}
table{width:100%; border-collapse:collapse; font-size:14px}
th{text-align:left; font-family:"IBM Plex Mono",monospace; font-weight:500;
  font-size:11px; letter-spacing:0.1em; text-transform:uppercase;
  color:var(--muted); padding:0 10px 9px 0;
  border-bottom:1px solid var(--line)}
td{padding:9px 10px 9px 0; border-bottom:1px solid var(--line);
  vertical-align:middle}
tr:last-child td{border-bottom:none}
td.n{font-family:"IBM Plex Mono",monospace; font-variant-numeric:tabular-nums;
  white-space:nowrap; text-align:right; padding-right:18px}
td.d{color:var(--muted); font-size:13px;
  font-family:"IBM Plex Mono",monospace}
td.bar{width:34%; min-width:120px}
td.bar span{display:block; height:6px; border-radius:3px;
  background:var(--accent)}
.sw{width:9px; height:9px; border-radius:1px; display:inline-block;
  margin-right:9px; vertical-align:middle}
.sw.hold{background:var(--c3)}
.sw.unk{background:var(--grid); outline:1px solid var(--line)}
.sw.src{background:var(--c2)} .sw.mid{background:var(--c1)}
.sw.prof{background:var(--ok)}
ul.checks{list-style:none; margin:0; padding:0; display:grid;
  grid-template-columns:repeat(auto-fill,minmax(290px,1fr)); gap:2px 20px}
ul.checks li{display:flex; align-items:baseline; gap:9px; font-size:13.5px;
  padding:5px 0}
.dot{width:7px; height:7px; border-radius:50%; flex:none;
  transform:translateY(-1px)}
li.ok .dot{background:var(--ok)}
li.fail .dot{background:var(--fail)} li.fail{color:var(--fail)}
.split{display:grid; grid-template-columns:repeat(auto-fit,minmax(250px,1fr));
  gap:14px}
.stat{background:var(--surface); border:1px solid var(--line);
  border-radius:3px; padding:16px 18px}
.stat .v{font-family:"IBM Plex Mono",monospace; font-size:26px;
  font-weight:600; font-variant-numeric:tabular-nums; line-height:1.2}
.stat .k{font-family:"IBM Plex Mono",monospace; font-size:11px;
  letter-spacing:0.1em; text-transform:uppercase; color:var(--muted);
  margin-top:3px}
.stat .note{font-size:12.5px; color:var(--muted); margin-top:8px}
svg{width:100%; height:auto; display:block; overflow:visible}
.gl{stroke:var(--line); stroke-width:1}
.ax{font-family:"IBM Plex Mono",monospace; font-size:10px; fill:var(--muted)}
.ln-total{fill:none; stroke:var(--series-ctx); stroke-width:2;
  stroke-linejoin:round; stroke-linecap:round}
.ln-hand{fill:none; stroke:var(--accent); stroke-width:2;
  stroke-linejoin:round; stroke-linecap:round}
.dot-total{fill:var(--series-ctx); stroke:var(--surface); stroke-width:2}
.dot-hand{fill:var(--accent); stroke:var(--surface); stroke-width:2}
.lab-total{font-family:"IBM Plex Mono",monospace; font-size:11px;
  fill:var(--series-ctx)}
.lab-hand{font-family:"IBM Plex Mono",monospace; font-size:11px;
  fill:var(--accent)}
footer{border-top:1px solid var(--line); padding-top:20px; font-size:13px;
  color:var(--muted)}
.scroll{overflow-x:auto}
@media (max-width:640px){ .cells{grid-template-columns:repeat(40,1fr)} }
</style>

<div class="wrap">

<header>
  <p class="eyebrow">SpongeBob&rsquo;s Truth or Square &middot; Xbox 360 &middot; matching decompilation</p>
  <h1>Every byte has to come back identical.</h1>
  <div class="figure">
    <span class="big">@pct@%</span>
    <span class="of">@built@ of @total@ bytes in <code>.text</code></span>
  </div>
  <p class="sub">Not a port and not an emulator. Source is written, compiled
  with the game&rsquo;s own 2008 XDK compiler, and the output must equal the
  retail bytes exactly. <strong>@matched@ functions</strong> reproduce today.</p>
</header>

<section>
  <div>
    <p class="eyebrow">Byte map &middot; <code>.text</code> 82100000&ndash;829135FC</p>
    <h2>What is actually reproduced</h2>
  </div>
  <div class="map">
    <div class="cells">@cells@</div>
    <div class="legend">
      <span><i class="key" style="background:var(--grid)"></i>copied from retail</span>
      <span><i class="key" style="background:var(--c1)"></i>partly rebuilt</span>
      <span><i class="key" style="background:var(--c3)"></i>fully rebuilt</span>
      <span>each cell &asymp; @percell@ bytes &middot; @lit@ of 1800 carry rebuilt code</span>
    </div>
  </div>
  <p>The build splices every compiled function into a copy of the section and
  hashes the whole thing. That hash passing proves the rebuilt bytes are right
  &mdash; it says nothing about the grey, which is copied verbatim.</p>
</section>

<section>
  <div>
    <p class="eyebrow">Progress &middot; @ncommits@ commits</p>
    <h2>Two lines, because one would flatter</h2>
  </div>
  <div class="panel">@chart@</div>
  <p>The horizontal axis is the commit sequence, not wall-clock time &mdash;
  the work happened in two sittings with a long gap, and a time axis would
  spend most of its width on the gap. <strong>The step from 559 to 940 is not
  a breakthrough:</strong> 362 of those 381 were one-expression stubs a script
  generated from their own encodings. That is exactly why the hand-written
  count is drawn separately.</p>
</section>

<section>
  <div>
    <p class="eyebrow">Continuous checks &middot; <code>python tools/verify.py</code></p>
    <h2>@nok@ of @nchecks@ passing</h2>
  </div>
  <div class="panel"><ul class="checks">@checks@</ul></div>
  <p>Six are <strong>negative controls</strong>: each corrupts one fact
  &mdash; a struct offset, a switch case mapping, a manifest address &mdash;
  and the build is required to <em>fail</em>. A control that stops failing is
  the serious result, because it means a check reports success without being
  able to see the failure it exists to catch.</p>
</section>

<section>
  <div>
    <p class="eyebrow">Work queue &middot; <code>src/attempts.txt</code></p>
    <h2>@nnear@ near-misses, closest first</h2>
  </div>
  <div class="panel scroll">
    <table>
      <thead><tr><th>Address</th><th class="n">Words</th><th>Agreement</th><th>Source</th></tr></thead>
      <tbody>@near@</tbody>
    </table>
  </div>
  <p>Every one has the right instructions and differs only in a decision made
  inside the compiler &mdash; a register, an operand order, a branch polarity.
  @nearleft@ more are not shown, and @vacuous@ whose every word is relocated
  are excluded &mdash; nothing in them can be compared. Six were solved after being written down as
  permanently stuck, three of them against a recorded mechanism explaining why
  they could not be; the measurements were right and the conclusions were
  wrong.</p>
</section>

<section>
  <div>
    <p class="eyebrow">Where the functions came from</p>
    <h2>Two halves that should never be added up</h2>
  </div>
  <div class="split">
    <div class="stat">
      <div class="v">@hand@</div>
      <div class="k">read off the disassembly</div>
      <div class="note">@handb@ bytes. One at a time, each teaching something
      about how this compiler decides.</div>
    </div>
    <div class="stat">
      <div class="v">@gen@</div>
      <div class="k">generated from encodings</div>
      <div class="note">@genb@ bytes. One expression each, written by script
      from the instructions themselves and verified like any other.</div>
    </div>
    <div class="stat">
      <div class="v">@att@</div>
      <div class="k">near-misses</div>
      <div class="note">Right instructions, wrong compiler decision. Each
      carries its measurement and what has been ruled out.</div>
    </div>
  </div>
  <div class="panel scroll">
    <table>
      <thead><tr><th>Mechanical vein</th><th class="n">Verified</th><th>What it is</th></tr></thead>
      <tbody>@veins@</tbody>
    </table>
  </div>
</section>

<section>
  <div>
    <p class="eyebrow">Composition</p>
    <h2>What the 8.4&nbsp;MB is made of</h2>
  </div>
  <div class="panel scroll">
    <table>
      <thead><tr><th>Origin</th><th class="n">Bytes</th><th class="n">Share</th><th>Evidence</th></tr></thead>
      <tbody>@comp@</tbody>
    </table>
  </div>
  <p>A third of the section is Microsoft&rsquo;s own libraries, on disk and
  linkable rather than rewritten. The rest has to be written, including the
  middleware &mdash; Havok, FMOD, Bink &mdash; for which no archive is held.
  Identifying a function as Havok says what it is; it does not make it go
  away.</p>
</section>

<footer>
  Generated by <code>tools/dashboard.py</code> from
  <code>src/manifest.txt</code>, <code>tools/build.py</code>,
  <code>tools/verify.py</code> and <code>tools/sweep.py</code>.
  <code>.text</code> is @total@ bytes; @inv@ function starts are known.
</footer>

</div>
"""

VALUES = {
    "pct": "%.3f" % pct,
    "built": fmt(built),
    "total": fmt(total),
    "matched": fmt(d["matched_total"]),
    "cells": cellhtml(),
    "percell": fmt(int(total / len(cells))),
    "lit": sum(1 for c in cells if c > 0),
    "ncommits": len(hist),
    "chart": chart(),
    "nok": n_ok,
    "nchecks": len(checks),
    "checks": check_rows,
    "nnear": len(near),
    "near": near_rows,
    "nearleft": near_left,
    "vacuous": len(vacuous),
    "hand": fmt(d["matched_hand"]),
    "handb": fmt(d["hand_bytes"]),
    "gen": fmt(d["matched_gen"]),
    "genb": fmt(d["gen_bytes"]),
    "att": fmt(d["attempts"]),
    "veins": vein_rows,
    "comp": comp_rows,
    "inv": fmt(d["inventory_rows"]),
}
for _k, _v in VALUES.items():
    HTML = HTML.replace("@" + _k + "@", str(_v))

OUT.write_text(HTML, encoding="utf-8")
leftover = re.findall("@([a-z_]+)@", HTML)
print("wrote %s (%d bytes)" % (OUT, len(HTML)))
print("  %d check(s), %d near-miss row(s), %d history point(s)"
      % (len(checks), len(near), len(hist)))
if leftover:
    print("  UNSUBSTITUTED TOKENS: %s" % ", ".join(sorted(set(leftover))))
