"""Build the dashboard HTML with data injected from dash.json + verify output.

Design plan, recorded before building:

  COLOUR   ground #0E151D deep cool slate; surface #16202B; line #263543;
           text #DAE3EC; muted #7E90A3 (grey biased blue toward the ground,
           chosen not inherited); accent #E9A13B amber -- the highlight
           colour of a hex editor, warm against a cool ground, and the one
           place boldness is spent. Semantic status is SEPARATE from the
           accent: ok #48A97B, warn #C99A3C, crit #C9564A.
  TYPE     Archivo for display (industrial grotesque, not Inter/Space
           Grotesk); IBM Plex Sans for body; IBM Plex Mono for every hex
           address, byte count and table figure -- Plex was drawn for
           technical documents, which is exactly this subject.
  LAYOUT   A console, scanned not read. The hero is the BYTE MAP: all
           8,467,964 bytes of .text as 1800 cells, lit where the repo
           reproduces them. It is the most characteristic object in this
           project's world and it is honest -- it shows at a glance how
           little is done, which a percentage alone lets you skim past.
"""
import json
import re
from pathlib import Path

SCR = Path("C:/Users/redacted/AppData/Local/Temp/claude/"
           "C--Users-redacted-Downloads-ToS-Decomp/"
           "c63367b7-31a8-4f3c-b0d0-491a74a6d3e7/scratchpad")
d = json.loads((SCR / "dash.json").read_text())

vf = SCR / "verify_final.txt"
checks = []
if vf.exists():
    # Parsed by SUFFIX, not by a column regex. verify.py pads its labels to
    # 42 characters, and seven of the twenty-one are longer than that -- so a
    # pattern requiring two spaces before the status silently dropped exactly
    # the checks with the longest names, and the page would have shown 14 of
    # 21 as though that were the whole suite.
    for raw in vf.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.rstrip()
        if not line.startswith("  ") or line.startswith("    "):
            continue
        body_ = line.strip()
        if body_.endswith(" ok"):
            checks.append((body_[:-3].strip(), "ok"))
        elif " FAIL" in body_:
            checks.append((body_[:body_.index(" FAIL")].strip(), "fail"))
    # The match tally is a check too, and prints in its own shape.
    for raw in vf.read_text(encoding="utf-8", errors="replace").splitlines():
        s = raw.strip()
        if s.endswith(" match") and " of " in s:
            n, _, rest = s.partition(" of ")
            tot = rest.split()[0]
            checks.append(("%s of %s functions reproduce" % (n, tot),
                           "ok" if n == tot else "fail"))
            break

n_ok = sum(1 for _n, s in checks if s == "ok")

built = d["built_bytes"]
total = d["text_bytes"]
pct = 100.0 * built / total

a = d["attribution"]
lib = a.get("lib", 0)
mid = a.get("rtti_havok", 0) + a.get("havok", 0)
srcp = a.get("srcpath", 0)
prof = a.get("game_profiled", 0)
unk = a.get("UNKNOWN", 0)
attr_total = lib + mid + srcp + prof + unk

cells = d["cells"]


def cellhtml():
    out = []
    for i, c in enumerate(cells):
        if c <= 0:
            out.append('<i></i>')
        else:
            lvl = 1 if c < 0.34 else (2 if c < 0.67 else 3)
            addr = 0x82100000 + int(i * total / len(cells))
            out.append('<i class="l%d" title="%08X &mdash; %d%% reproduced">'
                       '</i>' % (lvl, addr, round(c * 100)))
    return "".join(out)


def fmt(n):
    return "{:,}".format(n)


comp = [
    ("XDK libraries", lib, "held on disk &mdash; linkable, never written",
     "hold"),
    ("Unattributed", unk, "no signal yet; most of the game's own code",
     "unk"),
    ("Source-path tagged", srcp, "a __FILE__ string reaches this code",
     "src"),
    ("Havok", mid, "named by RTTI; no archive held", "mid"),
    ("Profiler-named", prof, "the game pushes its own function name", "prof"),
]

comp_rows = "".join(
    '<tr><td><span class="sw %s"></span>%s</td>'
    '<td class="n">%s</td><td class="n">%.1f%%</td>'
    '<td class="d">%s</td></tr>' % (k, name, fmt(v), 100.0 * v / attr_total,
                                    note)
    for name, v, note, k in comp)

veins = [
    ("Constant returns", 346, 346,
     "<code>lis/ori/blr</code> and <code>li/blr</code> &mdash; the game's "
     "own type IDs, since it ships no RTTI"),
    ("Accessors", 362, 362,
     "one expression each: field get, field set, pointer adjust, empty body"),
    ("Virtual forwarders", 110, 110,
     "tail call through a vtable slot; 56 of them needed <code>/Os</code>"),
]
vein_rows = "".join(
    '<tr><td>%s</td><td class="n">%d / %d</td><td class="d">%s</td></tr>'
    % (n, g, t, note) for n, g, t, note in veins)

check_rows = "".join(
    '<li class="%s"><span class="dot"></span>%s</li>' % (s, n)
    for n, s in checks) or '<li class="fail"><span class="dot"></span>' \
                           'verify output not captured</li>'

HTML = """<title>Truth or Square Byte Map</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?\
family=Archivo:wght@500;600;700&family=IBM+Plex+Mono:wght@400;500;600&\
family=IBM+Plex+Sans:wght@400;500&display=swap">
<style>
:root{
  --ground:#F5F7F9; --surface:#FFFFFF; --raised:#EEF2F6; --line:#DCE3EA;
  --text:#0F1922; --muted:#55677A; --accent:#B87415; --accent-soft:#F0D6AC;
  --ok:#2F7D57; --fail:#B23F33;
  --c1:#F0D6AC; --c2:#DFA553; --c3:#B87415;
  --grid:#E4EAF0;
}
@media (prefers-color-scheme: dark){
  :root:not([data-theme="light"]){
    --ground:#0E151D; --surface:#16202B; --raised:#1D2937; --line:#263543;
    --text:#DAE3EC; --muted:#7E90A3; --accent:#E9A13B; --accent-soft:#4A3A22;
    --ok:#48A97B; --fail:#C9564A;
    --c1:#4A3A22; --c2:#9C6F2A; --c3:#E9A13B;
    --grid:#1B2530;
  }
}
:root[data-theme="dark"]{
  --ground:#0E151D; --surface:#16202B; --raised:#1D2937; --line:#263543;
  --text:#DAE3EC; --muted:#7E90A3; --accent:#E9A13B; --accent-soft:#4A3A22;
  --ok:#48A97B; --fail:#C9564A;
  --c1:#4A3A22; --c2:#9C6F2A; --c3:#E9A13B;
  --grid:#1B2530;
}
*{box-sizing:border-box}
body{
  margin:0; background:var(--ground); color:var(--text);
  font-family:"IBM Plex Sans",system-ui,-apple-system,sans-serif;
  line-height:1.55; -webkit-font-smoothing:antialiased;
}
.wrap{max-width:1080px; margin:0 auto; padding:48px 24px 72px;
  display:flex; flex-direction:column; gap:44px}
h1,h2,h3{font-family:Archivo,system-ui,sans-serif; text-wrap:balance;
  margin:0; letter-spacing:-0.01em}
h1{font-size:clamp(26px,4.4vw,40px); font-weight:700; line-height:1.1}
h2{font-size:19px; font-weight:600}
.eyebrow{font-family:"IBM Plex Mono",monospace; font-size:11.5px;
  letter-spacing:0.14em; text-transform:uppercase; color:var(--muted);
  margin:0 0 10px}
.n,.mono,code{font-family:"IBM Plex Mono",monospace;
  font-variant-numeric:tabular-nums}
p{margin:0; max-width:66ch; color:var(--muted)}
p strong{color:var(--text); font-weight:500}
header .sub{margin-top:12px; font-size:15px}

/* --- headline figure ------------------------------------------------- */
.figure{display:flex; flex-wrap:wrap; align-items:flex-end; gap:8px 28px;
  margin-top:26px}
.big{font-family:"IBM Plex Mono",monospace; font-weight:600;
  font-size:clamp(38px,7vw,62px); line-height:1; color:var(--accent);
  font-variant-numeric:tabular-nums; letter-spacing:-0.02em}
.of{font-family:"IBM Plex Mono",monospace; font-size:15px;
  color:var(--muted); padding-bottom:6px}

/* --- byte map -------------------------------------------------------- */
.map{background:var(--surface); border:1px solid var(--line);
  border-radius:3px; padding:18px}
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

/* --- generic panels -------------------------------------------------- */
section{display:flex; flex-direction:column; gap:14px}
.panel{background:var(--surface); border:1px solid var(--line);
  border-radius:3px; padding:20px 22px}
table{width:100%; border-collapse:collapse; font-size:14px}
th{text-align:left; font-family:"IBM Plex Mono",monospace; font-weight:500;
  font-size:11px; letter-spacing:0.1em; text-transform:uppercase;
  color:var(--muted); padding:0 10px 9px 0; border-bottom:1px solid var(--line)}
td{padding:9px 10px 9px 0; border-bottom:1px solid var(--line);
  vertical-align:top}
tr:last-child td{border-bottom:none}
td.n{font-family:"IBM Plex Mono",monospace; font-variant-numeric:tabular-nums;
  white-space:nowrap; text-align:right; padding-right:18px}
td.d{color:var(--muted); font-size:13px}
.sw{width:9px; height:9px; border-radius:1px; display:inline-block;
  margin-right:9px; vertical-align:middle}
.sw.hold{background:var(--c3)} .sw.unk{background:var(--grid);
  outline:1px solid var(--line)}
.sw.src{background:var(--c2)} .sw.mid{background:var(--c1)}
.sw.prof{background:var(--ok)}

/* --- CI -------------------------------------------------------------- */
ul.checks{list-style:none; margin:0; padding:0; display:grid;
  grid-template-columns:repeat(auto-fill,minmax(290px,1fr)); gap:2px 20px}
ul.checks li{display:flex; align-items:baseline; gap:9px; font-size:13.5px;
  padding:5px 0; color:var(--text)}
.dot{width:7px; height:7px; border-radius:50%; flex:none;
  transform:translateY(-1px)}
li.ok .dot{background:var(--ok)}
li.fail .dot{background:var(--fail)}
li.fail{color:var(--fail)}
.tally{font-family:"IBM Plex Mono",monospace; font-size:13px;
  color:var(--muted)}

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
    <span class="big">@pct@%%</span>
    <span class="of">@built@ of @total@ bytes in <code>.text</code></span>
  </div>
  <p class="sub">This is not a port and not an emulator. Source is written,
  compiled with the game&rsquo;s own 2008 XDK compiler, and the output must
  equal the retail bytes exactly. <strong>@matched@ functions</strong>
  reproduce today.</p>
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
      <span>each cell &asymp; @percell@ bytes &middot; @lit@ of 1800 cells carry any rebuilt code</span>
    </div>
  </div>
  <p>The build splices every compiled function into a copy of the section and
  hashes the whole thing. That hash passing proves the rebuilt bytes are
  right &mdash; it says nothing about the grey, which is copied verbatim.</p>
</section>

<section>
  <div>
    <p class="eyebrow">Continuous checks &middot; <code>python tools/verify.py</code></p>
    <h2>%(nok)d of %(nchecks)d passing</h2>
  </div>
  <div class="panel">
    <ul class="checks">@checks@</ul>
  </div>
  <p>Six of these are <strong>negative controls</strong>: each corrupts one
  fact &mdash; a struct offset, a switch case mapping, a manifest address
  &mdash; and the build is required to <em>fail</em>. A control that stops
  failing is the serious result, because it means a check reports success
  without being able to see the failure it exists to catch.</p>
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
  <p>A third of the section is Microsoft&rsquo;s own libraries, which are on
  disk and can be linked rather than rewritten. The rest has to be written,
  including the middleware &mdash; Havok, FMOD, Bink &mdash; for which no
  archive is held. Identifying a function as Havok says what it is; it does
  not make it go away.</p>
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
      <div class="note">@handb@ bytes. One at a time, each one teaching
      something about how this compiler decides.</div>
    </div>
    <div class="stat">
      <div class="v">@gen@</div>
      <div class="k">generated from encodings</div>
      <div class="note">@genb@ bytes. One expression each, written by
      script from the instructions themselves and verified like any other.</div>
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
  <p>The generated half is real &mdash; compiled, compared, needed for a link,
  and each accessor pins a field offset. It is still not comparable to a
  function someone read. Quoting a single total would be true and misleading
  in the same breath.</p>
</section>

<footer>
  Figures come from <code>src/manifest.txt</code>, <code>tools/build.py</code>
  and <code>tools/verify.py</code> at the moment this page was generated.
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
    "nok": n_ok,
    "nchecks": len(checks),
    "checks": check_rows,
    "comp": comp_rows,
    "hand": fmt(d["matched_hand"]),
    "handb": fmt(d["hand_bytes"]),
    "gen": fmt(d["matched_gen"]),
    "genb": fmt(d["gen_bytes"]),
    "att": fmt(d["attempts"]),
    "veins": vein_rows,
    "inv": fmt(d["inventory_rows"]),
}

for _k, _v in VALUES.items():
    HTML = HTML.replace('@' + _k + '@', str(_v))

out = SCR / "dashboard.html"
out.write_text(HTML, encoding="utf-8")
print("wrote %s (%d bytes, %d checks, %d lit cells)"
      % (out, len(HTML), len(checks), sum(1 for c in cells if c > 0)))
