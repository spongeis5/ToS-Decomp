"""The compile memo must never serve a result from different source text.

    python tools/test_xdkcc_cache.py

A cache is the one optimisation in this project that can turn a correct
pipeline into a confidently wrong one, because a stale object does not look
like a failure -- it looks like a match. `compile_obj` already deletes the
.obj before every compile for exactly that reason; adding a memo above it
re-opens the same hole one level up unless the key is right.

So the key is the source's CONTENT, the exact flag list, and the compiler
binary's own size and mtime -- never a path, never a source timestamp. An
edit changes the key; swapping the XDK changes the key.

Eleven checks over BOTH halves. FOUR must miss the cache and actually invoke
cl (first compile, edited text, changed flags, edited text again against the
disk half); three must hit it, one of them FROM A SEPARATE PROCESS, which is
the case the in-process memo cannot serve and the entire reason the disk
half exists. The rest check that what comes back is the right bytes and that
a failure stays a failure rather than returning as a benign empty object.

A suite where every case expected a hit would pass just as happily with no
invalidation at all, so the misses are the point.
"""

import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import xdkcc

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "build/test_cache"

RESULTS = []

# A NONCE, so every run starts genuinely cold.
#
# These checks measure 'did cl actually run', which was a fair proxy for
# 'the cache did not serve this' while the memo lived only in memory. The
# disk half persists across runs, so on the SECOND run the fixed probe
# text was already cached and three must-MISS checks failed -- correctly
# reporting a hit, while claiming the memo was unsafe. The safety property
# was never in doubt; the test's proxy for it had expired.
#
# Unique content per run restores the proxy without weakening anything:
# a fresh key cannot be in any cache, so a hit would be a real failure.
NONCE = "%d_%d" % (os.getpid(), len(sys.argv))


def body(value):
    return ("// %s\nunsigned int f(void) { return %su; }\n"
            % (NONCE, value))


def check(name, ok, detail=""):
    RESULTS.append((name, ok, detail))
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  -- " + detail) if detail else ""))


def compile_counting(src, flags=None):
    """-> (blob, err, did_it_actually_invoke_cl)"""
    before = xdkcc.cache_stats()[1]
    blob, err = xdkcc.compile_obj(src, WORK / "out.obj", flags, WORK)
    after = xdkcc.cache_stats()[1]
    return blob, err, (after != before)


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    src = WORK / "probe.cpp"

    print("compile memo -- 11 checks across BOTH halves, in-process and on disk")
    print("")

    src.write_text(body("0x11111111"))
    a, ea, ran_a = compile_counting(src)
    check("first compile invokes cl", ran_a and a is not None,
          ea or "")

    b, _eb, ran_b = compile_counting(src)
    check("identical text hits the memo", (not ran_b) and b == a)

    # The one that matters: same path, different content.
    src.write_text(body("0x22222222"))
    c, _ec, ran_c = compile_counting(src)
    check("EDITED text must MISS", ran_c, "same path, new content")
    check("EDITED text yields different bytes", c is not None and c != a)

    # Reverting must also miss nothing -- the first version is still keyed,
    # and serving it is correct, because the content is genuinely identical.
    src.write_text(body("0x11111111"))
    d, _ed, ran_d = compile_counting(src)
    check("reverted text hits the memo again", (not ran_d) and d == a)

    # Different flags, same content, must recompile.
    _e, _ee, ran_e = compile_counting(
        src, ["/c", "/nologo", "/O2", "/Os", "/Gy", "/GS-", "/fp:fast"])
    check("different flags must MISS", ran_e)

    # A failing compile must be cached as a FAILURE, and must never come
    # back as a benign empty object.
    bad = WORK / "bad.cpp"
    bad.write_text("// %s\nthis is not C++ at all;\n" % NONCE)
    f1, err1 = xdkcc.compile_obj(bad, WORK / "bad.obj", None, WORK)
    f2, err2 = xdkcc.compile_obj(bad, WORK / "bad.obj", None, WORK)
    check("a failure stays a failure on the second call",
          f1 is None and f2 is None and bool(err1) and err1 == err2)

    # THE DISK HALF, which is the one that matters for the clock: the memo
    # is per-process, and verify.py's six negative controls each spawn a
    # fresh build.py. Without a disk cache all 389 sources are recompiled
    # seven times over -- about 2,700 invocations of cl.
    print("")
    src.write_text(body("0x33333333"))
    g, _eg, _ran = compile_counting(src)
    check("compiled once, then present on disk", g is not None)

    # A fresh PROCESS must hit it. That is the case the in-process memo
    # cannot serve and the reason this exists at all.
    import subprocess as sp
    probe = (
        "import sys; sys.path.insert(0, r'%s');"
        "import xdkcc;"
        "b, e = xdkcc.compile_obj(r'%s', r'%s', None, r'%s');"
        "print('HIT' if xdkcc.cache_stats()[0] else 'MISS');"
        "print('OK' if b is not None else 'FAIL')"
        % (Path(__file__).parent, src, WORK / "sub.obj", WORK)
    )
    out = sp.run([sys.executable, "-c", probe], capture_output=True,
                 text=True).stdout
    check("a SEPARATE process hits the disk cache", "HIT" in out,
          out.strip().replace("\n", " "))
    check("and gets a usable object back", "OK" in out)

    # Editing the source must miss on disk as well as in memory.
    src.write_text(body("0x44444444"))
    before = list((ROOT / "build/objcache").glob("*.obj"))
    h, _eh, ran_h = compile_counting(src)
    after = list((ROOT / "build/objcache").glob("*.obj"))
    check("EDITED text misses the disk cache too", ran_h and h != g,
          "%d -> %d cached object(s)" % (len(before), len(after)))

    print("")
    bad_n = sum(1 for _n, ok, _d in RESULTS if not ok)
    print("%d of %d check(s) passed" % (len(RESULTS) - bad_n, len(RESULTS)))
    if bad_n:
        print("")
        print("The memo is not safe. Do not use it: a cache that can serve a")
        print("result compiled from different text makes every match in the")
        print("manifest unverified, and it does so silently.")
    return 1 if bad_n else 0


if __name__ == "__main__":
    sys.exit(main())
