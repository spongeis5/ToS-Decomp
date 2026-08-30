"""Turn identified Ogg/Vorbis sites into VERIFIED manifest rows.

    python tools/ogg_rows.py            check every site, print the rows
    python tools/ogg_rows.py --write    append the ones that pass

`tools/oggmatch.py` IDENTIFIES: a masked scan says these image bytes came from
this upstream code. That is not the same claim as `src/manifest.txt` makes,
which is that a source in this repository compiles to those bytes exactly.
So every site is put through `tools/match.py` -- the tool that owns the
question -- and only the ones it accepts are written.

The sources are the vendored `thirdparty/ogg_vorbis/` tree, libogg 1.1.3 and
libvorbis 1.2.0, the pair decided by measurement in FINDINGS 8a.

These are UPSTREAM, not decompiled, and the report keeps them in their own
progress category for that reason. They genuinely reproduce the image -- the
compiler and the bytes do not care where the source came from -- but they say
nothing about how much of this game has been read, which is exactly the
distinction the hand-written/generated split already makes.
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SITES = ROOT / "build/ogg_sites.txt"
MANIFEST = ROOT / "src/manifest.txt"
VENDOR = "thirdparty/ogg_vorbis"

# Kept identical to what tools/oggmatch.py compiled with, because a row that
# builds differently from the way the identification was made is a row that
# was never actually tested.
FLAGS = ("/O2,/Gy,/GS-,/fp:fast,/D__BORLANDC__"
         ",/I" + VENDOR + "/ogg/include"
         ",/I" + VENDOR + "/vorbis/include"
         ",/I" + VENDOR + "/vorbis/lib")


def manifest_addrs():
    out = {}
    for line in MANIFEST.read_text(encoding="utf-8").splitlines():
        s = line.split("#")[0].strip()
        if not s:
            continue
        f = s.split()
        if len(f) >= 2:
            try:
                out[int(f[1], 16)] = f[0]
            except ValueError:
                pass
    return out


def main(argv):
    if not SITES.exists():
        print("%s is missing -- run `python tools/oggmatch.py` first."
              % SITES.relative_to(ROOT).as_posix())
        return 1
    have = manifest_addrs()

    sites = []
    for line in SITES.read_text().splitlines():
        if line.startswith("#"):
            continue
        f = line.split(None, 3)
        if len(f) >= 4:
            sites.append((int(f[0], 16), int(f[1]), f[2], f[3].strip()))

    print("%d identified site(s); %d already in the manifest"
          % (len(sites), sum(1 for a, _s, _f, _y in sites if a in have)))
    print("")

    rows, failed, skipped = [], [], []
    for addr, size, lbl, sym in sorted(sites):
        if addr in have:
            skipped.append((addr, sym, have[addr]))
            continue
        src = "%s/%s" % (VENDOR, lbl)
        if not (ROOT / src).exists():
            failed.append((addr, sym, "not vendored: %s" % src))
            continue
        r = subprocess.run(
            [sys.executable, "tools/match.py", src, "%08X" % addr,
             "--sym", sym, "--flags", FLAGS],
            cwd=str(ROOT), capture_output=True, text=True)
        if r.returncode == 0:
            rows.append((src, addr, sym, size))
            print("  %08X  %-28s %-34s MATCH   %d B" % (addr, sym, lbl, size))
        else:
            tail = [l for l in r.stdout.splitlines() if "word(s) compared" in l]
            failed.append((addr, sym, tail[-1].strip() if tail else
                           "match.py exit %d" % r.returncode))
            print("  %08X  %-28s %-34s no      %s"
                  % (addr, sym, lbl, tail[-1].strip() if tail else ""))

    total = sum(s for _src, _a, _y, s in rows)
    print("")
    print("%d of %d new site(s) VERIFIED by match.py, %s byte(s)"
          % (len(rows), len(sites) - len(skipped), "{:,}".format(total)))
    print("%d already matched under an invented name:" % len(skipped))
    for addr, sym, f in skipped:
        print("    %08X  %-28s is %s" % (addr, sym, f))
    if failed:
        print("%d identified but REFUSED by match.py -- identification is a"
              % len(failed))
        print("weaker claim than a match, and this is where that shows:")
        for addr, sym, why in failed:
            print("    %08X  %-28s %s" % (addr, sym, why))

    if "--write" not in argv:
        print("")
        print("nothing written; pass --write to append these rows")
        return 0

    with MANIFEST.open("a", encoding="utf-8", newline="") as f:
        for src, addr, sym, _size in rows:
            f.write("%-31s %08X  %-22s flags=%s\n" % (src, addr, sym, FLAGS))
    print("")
    print("appended %d row(s) to %s"
          % (len(rows), MANIFEST.relative_to(ROOT).as_posix()))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
