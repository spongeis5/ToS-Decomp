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


SENTINEL = ROOT / "build/.verify_restore.json"


def restore_if_interrupted():
    """Put back anything a killed run left corrupted.

    `negative()` corrupts a real source file, runs the build, and restores it
    in a `finally`. A `finally` does not run when the process is KILLED --
    and a two-minute command timeout killed one of these mid-control, leaving
    `src/manifest.txt` holding 821636AC instead of 821636A8. The next run
    then reported four failures, one of them a negative control reading
    "pattern absent, test is invalid", which is a confusing way to be told
    the tree is dirty.

    So the original text goes to disk BEFORE the corruption and is removed
    only after the restore. If it is still there at startup, the previous run
    died and this puts the tree back.
    """
    if not SENTINEL.exists():
        return
    import json
    try:
        saved = json.loads(SENTINEL.read_text(encoding="utf-8"))
    except ValueError:
        print("build/.verify_restore.json is unreadable; remove it by hand.")
        sys.exit(1)
    print("A PREVIOUS RUN WAS KILLED while a negative control had a file")
    print("corrupted. Restoring before doing anything else:")
    for rel, text in saved.items():
        (ROOT / rel).write_text(text, encoding="utf-8")
        print("  restored %s" % rel)
    SENTINEL.unlink()
    print("")


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
    import json
    SENTINEL.parent.mkdir(parents=True, exist_ok=True)
    SENTINEL.write_text(json.dumps({path: orig}), encoding="utf-8")
    p.write_text(text)
    try:
        rc, out = run(["tools/build.py"])
    finally:
        p.write_text(orig)
        if SENTINEL.exists():
            SENTINEL.unlink()
    ok = rc != 0
    if ok and expect_substr:
        ok = expect_substr in out
    print("  %-42s %s" % (label, "ok" if ok else "FAIL -- NOT CAUGHT"))
    return ok


def load_matches():
    """Read the build manifest rather than keeping a second copy of it.

    verify.py used to carry its own hardcoded list, which is the same drift
    that let three compile harnesses fall out of step with build.py. One
    source of truth.
    """
    out = []
    for line in (ROOT / "src/manifest.txt").read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        f = line.split()
        sym, flags = None, None
        for extra in f[2:]:
            if extra.startswith("flags="):
                flags = extra[len("flags="):]
            elif extra != "-":
                sym = extra
        out.append((f[0], f[1], sym, flags))
    return out


MATCHES = load_matches()


def main():
    restore_if_interrupted()
    results = []

    print("TOOLS -- each must run and exit 0\n")
    results.append(check("xdkcc self-test (headers + assertions)",
                         ["tools/xdkcc.py"]))
    results.append(check("coffreloc relocation semantics, 9 cases",
                         ["tools/test_coffreloc.py"]))
    results.append(check("match.py size reconciliation, 12 cases (8 must refuse)",
                         ["tools/test_shrink.py"]))
    results.append(check("MATCHED.md table matches the manifest",
                         ["tools/matched_table.py", "--check"]))
    results.append(check("backslash-heredoc hook, 7 cases",
                         [".claude/hooks/test_no_backslash_heredoc.py"]))
    results.append(check("reconstructing build (.text reproduces)",
                         ["tools/build.py"]))

    # No inventory entry may be a switch case body. Case bodies are labels
    # inside a function, and one listed as a function is a real defect.
    rc, out = run(["tools/switches.py"])
    clean = rc == 0 and "case bodies  %6d" % 0 not in out
    ok = rc == 0 and "switch case bodies       0" in out.replace("  ", " ")
    line = [l for l in out.splitlines() if "case bodies" in l]
    ok = bool(line) and line[0].split()[-1] == "0"
    print("  %-42s %s" % ("no inventory entry is a switch case body",
                          "ok" if ok else "FAIL"))
    results.append(ok)

    # A symbol resolving to two addresses verifies byte for byte and could
    # never link. build.py reports it; nothing should be reporting it.
    rc, out = run(["tools/build.py"])
    linkable = "WOULD NOT LINK" not in out
    print("  %-42s %s" % ("no symbol resolves to two addresses",
                          "ok" if linkable else "FAIL -- see build.py output"))
    results.append(linkable)

    print("")
    print("MATCHES -- %d function(s)\n" % len(MATCHES))
    ok_n = 0
    for src, addr, sym, flags in MATCHES:
        args = ["tools/match.py", src, addr] + (["--sym", sym] if sym else [])
        if flags:
            args += ["--flags", "/c /nologo " + " ".join(flags.split(","))]
        rc, _out = run(args)
        if rc == 0:
            ok_n += 1
        else:
            print("  FAIL  %-28s %s %s %s"
                  % (Path(src).name, addr, sym or "", flags or ""))
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
    # The image itself: every number here is a claim about one specific
    # image, so a different one must be refused rather than silently used.
    import shutil
    img = ROOT / "build/default.pe.exe"
    bak = ROOT / "build/default.pe.exe.verify"
    shutil.copy(str(img), str(bak))
    d = bytearray(img.read_bytes())
    d[0x500000] ^= 0xFF
    img.write_bytes(bytes(d))
    try:
        rc, out = run(["tools/build.py"])
    finally:
        shutil.move(str(bak), str(img))
    caught = rc != 0 and "not the image" in out
    print("  %-42s %s" % ("corrupted image -> refused",
                          "ok" if caught else "FAIL -- NOT CAUGHT"))
    results.append(caught)

    # The jump table of a switch is 25 whole-word relocations. If build.py
    # copied them from the image, a wrong CASE MAPPING would verify clean --
    # the bodies are identical and only the table says which case reaches
    # which. It predicts them from our own labels instead, so moving one case
    # to the wrong arm must fail.
    results.append(negative(
        "wrong switch case mapping -> caught",
        "src/i_canon_switch.cpp",
        "        return 27;",
        "        return 29;",
        "the case mapping"))

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
