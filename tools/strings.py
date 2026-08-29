"""String census over the unpacked image.

Not a grep.  It walks every byte once, extracts every printable run at or
above a minimum length, and then CLASSIFIES them -- so a category that
happens to be empty is visible as "0 of N", and the total is asserted to
account for every string found.  A filter only returns what you already
suspected, which is exactly the set of things that does not need finding.

    python tools/strings.py                 the classified census
    python tools/strings.py --class paths   every string in one class
    python tools/strings.py --all           every string, in address order
"""

import re
import sys
from pathlib import Path

IMAGE = Path("build/default.pe.exe")
IMAGE_BASE = 0x82000000
MIN_LEN = 5

# Classifiers, applied in order; first match wins so the classes partition
# the population rather than overlapping.  Each is (name, compiled regex).
CLASSES = [
    ("source_paths", re.compile(r"^[A-Za-z]:[\\/].*\.(?:c|cpp|cxx|h|hpp|inl|asm|s)$", re.I)),
    ("build_paths", re.compile(r"^[A-Za-z]:[\\/][^\x00]{4,}$")),
    ("source_files", re.compile(r"^[\w.\-]+\.(?:c|cpp|cxx|h|hpp|inl|asm|s)$", re.I)),
    ("versions", re.compile(r"\d+\.\d+\.\d+(?:\.\d+)?")),
    ("tool_banners", re.compile(r"Microsoft|Copyright|\(R\)|\(C\)|RAD Game|Scaleform|FMOD|Bink", re.I)),
    ("xbox_api", re.compile(r"^(?:Xam|Xex|Xe|Ke|Nt|Ob|Rtl|Ex|Io|Vd|Mm|Db|Xbox|D3D|XMA)\w+$")),
    ("format_strings", re.compile(r"%[-+ #0]*[\d.*]*(?:hh|h|ll|l|L|z|j|t)?[diouxXeEfgGaAcspn%]")),
    ("assets", re.compile(r"\.(?:ho|bik|fev|fsb|xpr|dds|swf|gfx|xml|ini|txt|lua)$", re.I)),
    ("identifiers", re.compile(r"^[A-Za-z_][\w:~<>,\s*&]{4,}$")),
]


def extract(data, min_len=MIN_LEN):
    """Every printable ASCII run of at least min_len, as (offset, text)."""
    out = []
    start = None
    for i, b in enumerate(data):
        if 0x20 <= b < 0x7F or b == 0x09:
            if start is None:
                start = i
        else:
            if start is not None and i - start >= min_len:
                out.append((start, data[start:i].decode("latin1")))
            start = None
    if start is not None and len(data) - start >= min_len:
        out.append((start, data[start:].decode("latin1")))
    return out


def classify(text):
    for name, rx in CLASSES:
        if rx.search(text):
            return name
    return "other"


def main(argv):
    if not IMAGE.exists():
        print(f"{IMAGE} not found -- run tools/xex.py first", file=sys.stderr)
        return 1
    data = IMAGE.read_bytes()
    strings = extract(data)

    buckets = {name: [] for name, _ in CLASSES}
    buckets["other"] = []
    for off, text in strings:
        buckets[classify(text)].append((off, text))

    total = sum(len(v) for v in buckets.values())
    assert total == len(strings), (
        f"classification lost strings: {total} filed of {len(strings)} found"
    )

    want = None
    show_all = "--all" in argv
    if "--class" in argv:
        want = argv[argv.index("--class") + 1]
        if want not in buckets:
            print(f"no class {want!r}; have: {', '.join(sorted(buckets))}", file=sys.stderr)
            return 1

    if want is None and not show_all:
        print(f"{len(strings):,} string(s) of >= {MIN_LEN} chars over "
              f"{len(data):,} byte(s)\n")
        for name in [n for n, _ in CLASSES] + ["other"]:
            v = buckets[name]
            print(f"  {name:<16} {len(v):>7,}  of {len(strings):,}")
        print()
        # Show every string in the small, high-value classes in full.
        for name in ("source_paths", "build_paths", "versions", "tool_banners"):
            v = buckets[name]
            print(f"--- {name}: all {len(v)} ---")
            seen = set()
            for off, text in v:
                if text in seen:
                    continue
                seen.add(text)
                print(f"  {IMAGE_BASE + off:08X}  {text}")
            if len(seen) != len(v):
                print(f"  ({len(v)} occurrence(s), {len(seen)} distinct)")
            print()
        return 0

    rows = strings if show_all else buckets[want]
    for off, text in rows:
        print(f"{IMAGE_BASE + off:08X}  {text}")
    print(f"\n{len(rows):,} of {len(strings):,} string(s)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
