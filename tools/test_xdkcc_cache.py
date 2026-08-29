"""The compile memo must never serve a result from different source text.

    python tools/test_xdkcc_cache.py

A cache is the one optimisation in this project that can turn a correct
pipeline into a confidently wrong one, because a stale object does not look
like a failure -- it looks like a match. `compile_obj` already deletes the
.obj before every compile for exactly that reason; adding a memo above it
re-opens the same hole one level up unless the key is right.

So the memo is keyed on the source's CONTENT, and these checks prove the
three things that makes true. Of the seven, THREE must miss the cache and
actually invoke cl (first compile, edited text, changed flags), TWO must hit
it, and TWO check that what comes back is the right bytes and that a failure
stays a failure. A suite where every case expected a hit would pass just as
happily with no invalidation at all, so the misses are the point.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import xdkcc

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "build/test_cache"

RESULTS = []


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

    print("compile memo -- 7 checks: 3 must MISS, 2 must HIT, 2 check content")
    print("")

    src.write_text("unsigned int f(void) { return 0x11111111u; }\n")
    a, ea, ran_a = compile_counting(src)
    check("first compile invokes cl", ran_a and a is not None,
          ea or "")

    b, _eb, ran_b = compile_counting(src)
    check("identical text hits the memo", (not ran_b) and b == a)

    # The one that matters: same path, different content.
    src.write_text("unsigned int f(void) { return 0x22222222u; }\n")
    c, _ec, ran_c = compile_counting(src)
    check("EDITED text must MISS", ran_c, "same path, new content")
    check("EDITED text yields different bytes", c is not None and c != a)

    # Reverting must also miss nothing -- the first version is still keyed,
    # and serving it is correct, because the content is genuinely identical.
    src.write_text("unsigned int f(void) { return 0x11111111u; }\n")
    d, _ed, ran_d = compile_counting(src)
    check("reverted text hits the memo again", (not ran_d) and d == a)

    # Different flags, same content, must recompile.
    _e, _ee, ran_e = compile_counting(
        src, ["/c", "/nologo", "/O2", "/Os", "/Gy", "/GS-", "/fp:fast"])
    check("different flags must MISS", ran_e)

    # A failing compile must be cached as a FAILURE, and must never come
    # back as a benign empty object.
    bad = WORK / "bad.cpp"
    bad.write_text("this is not C++ at all;\n")
    f1, err1 = xdkcc.compile_obj(bad, WORK / "bad.obj", None, WORK)
    f2, err2 = xdkcc.compile_obj(bad, WORK / "bad.obj", None, WORK)
    check("a failure stays a failure on the second call",
          f1 is None and f2 is None and bool(err1) and err1 == err2)

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
