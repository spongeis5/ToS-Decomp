"""The two new permuter mutations must produce C++ that actually COMPILES.

    python tools/test_mutations.py

`mut_temp` is the warning. It was added because liveness is what register
allocation follows from, could not name a type without a parser, and settled
for `(void)(expr);` -- which is valid C++ and changes nothing. It has never
moved a function. A mutation that always compiles and never helps and a
mutation that would help but never compiles look identical from the outside:
both just fail to find anything.

So these checks do two things per mutation. They confirm it FIRES on a
source shaped like the ones in src/, and they compile the result with the
real XDK cl. A mutation whose output does not compile is not a search space,
it is a wasted iteration every time it is drawn.
"""

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import permuter
import xdkcc

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "build/test_mut"
RESULTS = []

SAMPLE = '''#include "types.h"

struct Node
{
    Node* next;
    Node* prev;
    s32   count;
};

struct Owner
{
    Node* head;
    s32   total;
    s32   flags;
};

void Insert(Owner* o, Node* n)
{
    n->next = o->head;
    n->prev = 0;
    o->head = n;
    o->total = o->total + n->count;
}
'''


def check(name, ok, detail=""):
    RESULTS.append(ok)
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  -- " + detail) if detail else ""))


def compiles(text, tag):
    WORK.mkdir(parents=True, exist_ok=True)
    p = WORK / (tag + ".cpp")
    p.write_text(text, encoding="utf-8")
    blob, err = xdkcc.compile_obj(p, WORK / (tag + ".obj"), None, WORK)
    return blob is not None, (err or "")


def main():
    print("permuter mutations -- each must FIRE and then COMPILE")
    print("")

    ok, err = compiles(SAMPLE, "base")
    check("the sample source itself compiles", ok, err[:120])

    for name, fn in (("constview", permuter.mut_constview),
                     ("addrof", permuter.mut_addrof)):
        fired = 0
        failed = []
        for seed in range(40):
            rng = random.Random(seed)
            out = fn(SAMPLE, rng)
            if out is None or out == SAMPLE:
                continue
            fired += 1
            good, e = compiles(out, "%s_%d" % (name, seed))
            if not good:
                failed.append((seed, e.splitlines()[0] if e else "?", out))
        check("%s fires on the sample" % name, fired > 0,
              "%d of 40 seed(s)" % fired)
        check("%s output compiles on the sample" % name,
              fired > 0 and not failed,
              "%d of %d failed" % (len(failed), fired) if failed else "")
        if failed:
            seed, msg, out = failed[0]
            print("       first failure, seed %d: %s" % (seed, msg))
            for ln in out.splitlines():
                if "__cv" in ln or "__store_through" in ln:
                    print("         %s" % ln.strip())

    # THE REAL SOURCES, which is the harder and more honest bar. The
    # synthetic sample above has no comments, one function and no arrays;
    # every bug these mutations had showed up only on real files -- a use
    # matched inside the disassembly comment every source opens with, a
    # declaration inserted into a static helper above the real function,
    # uses rewritten in a second function that never saw the declaration,
    # and `&p->field` yielding a const pointer.
    #
    # The bar is a RATE, not perfection. A textual mutation without a C++
    # parser cannot be right every time, and permuter discards what does not
    # compile, so a high rate is a good search space and only a low one is a
    # broken mutation. Measured at 89% for constview over 60 sources.
    RATE = 0.70
    srcs = sorted(ROOT.glob("src/*.cpp"))[:40]
    if not srcs:
        check("real sources available to test against", False)
    else:
        for name, fn in (("constview", permuter.mut_constview),
                         ("addrof", permuter.mut_addrof)):
            fired = ok_n = 0
            reasons = {}
            for p in srcs:
                t = p.read_text(encoding="utf-8", errors="replace")
                for seed in range(4):
                    out = fn(t, random.Random(seed))
                    if out is None or out == t:
                        continue
                    fired += 1
                    good, e = compiles(out, "real")
                    if good:
                        ok_n += 1
                    else:
                        k = (e or "?").splitlines()[0].split(") : ")[-1][:44]
                        reasons[k] = reasons.get(k, 0) + 1
            rate = ok_n / float(fired) if fired else 0.0
            check("%s fires on real sources" % name, fired > 0,
                  "%d time(s) over %d file(s)" % (fired, len(srcs)))
            check("%s compiles at >= %d%% on real sources" % (name,
                                                             RATE * 100),
                  fired > 0 and rate >= RATE,
                  "%d of %d = %.0f%%" % (ok_n, fired, 100 * rate))
            for k, v in sorted(reasons.items(), key=lambda kv: -kv[1])[:3]:
                print("         %3d  %s" % (v, k))

    # constview must rewrite SOME uses, never all: renaming every use just
    # renames the pointer and restores the CSE tie it exists to break.
    partial = 0
    total = 0
    for seed in range(40):
        out = permuter.mut_constview(SAMPLE, random.Random(seed))
        if not out:
            continue
        total += 1
        if "o->" in out and "__cv->" in out:
            partial += 1
    check("constview leaves some uses on the original name",
          total > 0 and partial == total,
          "%d of %d" % (partial, total))

    print("")
    bad = RESULTS.count(False)
    print("%d of %d check(s) passed" % (len(RESULTS) - bad, len(RESULTS)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
