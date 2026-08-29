"""Run every check this project has, and say which ones can actually fail.

    python tools/verify.py

This exists because an audit found six real defects in tooling that had been
committed as working, and four of them were invisible from any single tool's
own output:

  * the .text hash could not fail -- only functions already proven equal were
    spliced in, so `rebuilt == original` was true by construction
  * `discover.py --compare` compared discovery against itself once the
    inventory default was switched to discovery
  * three of the four compile harnesses lacked `include/` on their search
    path, so seven matches broke while build.py still passed
  * segment.py fabricated a 4-byte size for 88 functions, inflating a
    reported precision from 55% to 72%

Every one was a check that reported success without exercising anything. So
this runner does not just run the checks -- where a check has a NEGATIVE
CONTROL available, it runs that too, and a check whose control does not fail
is reported as broken even if the check itself passes.
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PY = sys.executable


def run(args, cwd=ROOT):
    r = subprocess.run([PY] + args, capture_output=True, text=True,
                       cwd=str(cwd))
    return r.returncode, r.stdout + r.stderr


def check(label, args, want_zero=True):
    rc, out = run(args)
    ok = (rc == 0) if want_zero else (rc != 0)
    print("  %-42s %s" % (label, "ok" if ok else "FAIL (exit %d)" % rc))
    if not ok:
        for line in out.strip().splitlines()[-4:]:
            print("      %s" % line)
    return ok


def negative(label, path, old, new, expect_substr=None, also=None):
    """Corrupt one thing, require the BUILD to fail, then restore.

    `also` is a second (old, new) applied at the same time -- used when a
    layout change must be accompanied by its ASSERT_OFFSET so the failure is
    forced through the byte comparison instead of tripping C2118 first.
    """
    p = ROOT / path
    orig = p.read_text()
    if old not in orig or (also and also[0] not in orig):
        print("  %-42s FAIL (pattern absent, test is invalid)" % label)
        return False
    text = orig.replace(old, new, 1)
    if also:
        text = text.replace(also[0], also[1], 1)
    p.write_text(text)
    try:
        rc, out = run(["tools/build.py"])
    finally:
        p.write_text(orig)
    ok = rc != 0
    if ok and expect_substr:
        ok = expect_substr in out
    print("  %-42s %s" % (label, "ok" if ok else "FAIL -- NOT CAUGHT"))
    return ok


MATCHES = [
    ("src/grid_indices.cpp", "822607F0", None),
    ("src/guard_tailcall.cpp", "82807B38", None),
    ("src/array_add.cpp", "8253FD70", None),
    ("src/table_index.cpp", "822D2450", None),
    ("src/chain5.cpp", "821636A8", None),
    ("src/null_tailcall.cpp", "826A3350", None),
    ("src/vcall_arg2.cpp", "82600BB0", None),
    ("src/global_field.cpp", "82600BD8", None),
    ("src/ctor_vt.cpp", "821A4628", None),
    ("src/set_vtable.cpp", "826FE5C8", None),
    ("src/set_vtable.cpp", "826FE5B8", None),
    ("src/string_utils.cpp", "82540728", "StrLen"),
    ("src/string_utils.cpp", "82540750", "StrCopy"),
    ("src/owner_clear.cpp", "82677028", "ClearAndHandle"),
    ("src/owner_clear.cpp", "82677040", "ClearAndHandleOther"),
]


def main():
    results = []

    print("TOOLS -- each must run and exit 0\n")
    results.append(check("xdkcc self-test (headers + assertions)",
                         ["tools/xdkcc.py"]))
    results.append(check("coffreloc relocation semantics, 5 cases",
                         ["tools/test_coffreloc.py"]))
    results.append(check("backslash-heredoc hook, 7 cases",
                         [".claude/hooks/test_no_backslash_heredoc.py"]))
    results.append(check("reconstructing build (.text reproduces)",
                         ["tools/build.py"]))

    print("")
    print("MATCHES -- %d function(s)\n" % len(MATCHES))
    ok_n = 0
    for src, addr, sym in MATCHES:
        args = ["tools/match.py", src, addr] + (["--sym", sym] if sym else [])
        rc, _out = run(args)
        if rc == 0:
            ok_n += 1
        else:
            print("  FAIL  %-28s %s %s" % (Path(src).name, addr, sym or ""))
    print("  %d of %d match" % (ok_n, len(MATCHES)))
    results.append(ok_n == len(MATCHES))

    print("")
    print("NEGATIVE CONTROLS -- each corrupts one fact; the build MUST fail\n")
    results.append(negative("wrong struct offset -> compile error",
                            "src/chain5.cpp",
                            "char unk0000[0x38]; B*    b;",
                            "char unk0000[0x3C]; B*    b;", "C2118"))
    results.append(negative("wrong ASSERT_SIZE -> compile error",
                            "src/table_index.cpp",
                            "ASSERT_SIZE(Entry, 1856);",
                            "ASSERT_SIZE(Entry, 1857);", "C2118"))
    # Move E.v AND its assertion together, so the layout assert stays
    # satisfied and the failure must be caught by the BYTES rather than by
    # C2118. That is what makes this a test of the hash and not of the header.
    results.append(negative(
        "wrong codegen -> .text hash differs",
        "src/chain5.cpp",
        "struct E { /* 0x18 */ char unk0000[0x18]; void* v; };",
        "struct E { /* 0x1C */ char unk0000[0x1C]; void* v; };",
        "DOES NOT REPRODUCE",
        also=("ASSERT_OFFSET(E, v, 0x18);", "ASSERT_OFFSET(E, v, 0x1C);")))
    results.append(negative(
        "wrong manifest address -> caught",
        "src/manifest.txt",
        "src/chain5.cpp                  821636A8",
        "src/chain5.cpp                  821636AC"))

    print("")
    n_ok = sum(1 for r in results if r)
    print("%d of %d check(s) passed." % (n_ok, len(results)))
    if n_ok != len(results):
        print("")
        print("A failing NEGATIVE CONTROL is the serious kind: it means a")
        print("check reports success without being able to detect the failure")
        print("it exists to detect.")
    return 0 if n_ok == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
