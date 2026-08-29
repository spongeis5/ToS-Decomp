"""Search source shapes automatically for a function that will not match.

    python tools/permuter.py src/init12.cpp 826C1480
    python tools/permuter.py src/init12.cpp 826C1480 --iters 2000 --seed 7

Eleven functions in MATCHED.md have the right instructions in the right
multiset and differ only in a decision the compiler made on its own --
register assignment, instruction order, branch polarity. No flag reaches
those (72 combinations tried on one of them, all byte-identical), and hand-
written variants ran out long before the space did.

This is the decomp-permuter idea applied to this project: mutate the source
in ways that cannot change what it computes, compile each mutation with the
real XDK compiler, and score it against the retail bytes. It is not a clever
search -- it is a dumb one that can afford to be, because compiling one of
these functions takes about a tenth of a second.

WHAT IT MUTATES, and each of these is here because a real stall has that
shape:

  reorder      swap adjacent independent statements in a block. The store
               order of sub_826C1480 and sub_82649240 is source order, so
               the ordering IS the search space.
  invert       turn `if (c) A; return X;` into `if (!c) return X; A;` and
               back. Two functions in the last batch matched only after this,
               and it cannot be reached by any flag.
  member       move a free function taking T* as its first parameter into
               T as a member. This is what took sub_826C0FC8 from 2/6 to 6/6
               when six free-function shapes could not.
  temp         touch a subexpression before its first use, changing what
               the compiler keeps live. WEAK: without a C++ parser there
               is no way to name the type of a subexpression, so this
               cannot introduce a real typed local. It is the least
               useful mutation here and is kept only because liveness is
               what register assignment follows from.
  compare      rewrite `x != 0` as `x > 0` and back. For an unsigned
               value those are the same thing and compile to DIFFERENT
               branch conditions -- `beq-` versus `ble-`. sub_822D0BE8
               came down to exactly that one word.
  inline       remove a local that only names a subexpression, or the
               reverse. Changes the register allocator's problem.
  sign         swap int/unsigned on a parameter or field.

WHAT IT CANNOT DO: it has no C++ parser. Mutations are textual and structural
on the small, regular sources this project writes, and a mutation that fails
to compile is simply discarded -- which is safe but means the search is
weaker on anything ornate. It reports how many mutations were discarded so
that weakness is visible rather than assumed.

SAFETY: the input file is never modified. Everything happens in
build/permuter/, and the best source found is written there for inspection.
"""

import random
import re
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from peimage import Image, load_inventory
from libmatch import trim_padding
from coffreloc import functions_with_relocs
import build as buildmod
import ppcdis
import xdkcc

WORK = Path("build/permuter")
FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]


# ---------------------------------------------------------------- scoring

def compile_and_score(text, tbytes, tsize, target, want_sym=None):
    """-> (words_matching, size, code) or None if it did not compile.

    Relocations are RESOLVED against the retail bytes before scoring, exactly
    as tools/build.py does. Without that, every `bl` and every `lis`/`addi`
    pair counts as a mismatch no matter how correct the function is -- which
    made a 4-of-6 function score 1 of 6 here and gave the hill-climb a signal
    that was mostly noise about relocations rather than about the code.
    """
    WORK.mkdir(parents=True, exist_ok=True)
    src = WORK / "p.cpp"
    src.write_text(text)
    blob, _err = xdkcc.compile_obj(src, WORK / "p.obj", FLAGS, WORK)
    if blob is None:
        return None
    fns = functions_with_relocs(blob)
    if not fns:
        return None
    if want_sym:
        pick = [f for f in fns if ("?" + want_sym + "@@") in f[0]]
        if len(pick) != 1:
            return None
        sym, code, relocs = pick[0]
    else:
        sym, code, relocs = max(fns, key=lambda f: len(f[1]))
    code, _m = trim_padding(code, bytes([1]) * len(code))
    relocs = [r for r in relocs if r.off < len(code)]

    ref = tbytes[:len(code)] if tsize >= len(code) else \
        tbytes + bytes(len(code) - tsize)
    try:
        code, _notes, _problems = buildmod.relocate(code, relocs, target,
                                                    ref, False)
    except Exception:
        pass

    n = min(len(code), tsize) // 4
    same = 0
    for i in range(n):
        if (struct.unpack_from(">I", tbytes, i * 4)[0]
                == struct.unpack_from(">I", code, i * 4)[0]):
            same += 1
    return same, len(code), code


def rank(result, tsize):
    """Order candidates. Exact size first, then matching words."""
    if result is None:
        return (-1, -1)
    same, size, _code = result
    return (1 if size == tsize else 0, same)


# ------------------------------------------------------------- mutations

BODY_RE = re.compile(r"\{([^{}]*)\}", re.S)

# Comments and string literals are NOT code and must never be rewritten.
# Substituting the parameter name `s` for `this` across raw text turned the
# comment "the target's own store order" into "the target'this own store
# order" -- which still compiled as a comment, so nothing complained, and the
# mutation then failed for an unrelated reason that took a debug session to
# find. Identifier rewriting goes through code_sub().
SPAN_RE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"'
                     r"|'(?:\\.|[^'\\])*'", re.S)


def code_sub(pattern, repl, text):
    """re.sub, but only outside comments and string literals."""
    out, last = [], 0
    for m in SPAN_RE.finditer(text):
        out.append(re.sub(pattern, repl, text[last:m.start()]))
        out.append(m.group(0))
        last = m.end()
    out.append(re.sub(pattern, repl, text[last:]))
    return "".join(out)


def members_of(text, tname):
    """Field names declared in `struct tname`, best effort."""
    m = re.search(r"struct\s+%s\s*\{(.*?)\}\s*;" % re.escape(tname), text, re.S)
    if not m:
        return set()
    names = set()
    for line in m.group(1).splitlines():
        line = SPAN_RE.sub("", line).strip()
        for mm in re.finditer(r"\b(\w+)\s*(?:\[[^\]]*\])?\s*;", line):
            names.add(mm.group(1))
    return names


def _statements(block):
    """Split a brace-free block into statements, keeping their text."""
    parts, cur, depth = [], "", 0
    for ch in block:
        cur += ch
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == ";" and depth == 0:
            parts.append(cur)
            cur = ""
    if cur.strip():
        parts.append(cur)
    return parts


def mut_reorder(text, rng):
    """Swap two adjacent statements inside some innermost block."""
    blocks = list(BODY_RE.finditer(text))
    rng.shuffle(blocks)
    for m in blocks:
        stmts = _statements(m.group(1))
        real = [i for i, s in enumerate(stmts) if s.strip()]
        if len(real) < 2:
            continue
        # Only swap simple assignments/expressions: anything with a keyword
        # that affects control flow is left alone.
        def simple(s):
            t = s.strip()
            return t and not re.match(
                r"\b(if|else|for|while|do|return|switch|goto)\b", t)
        pairs = [(real[k], real[k + 1]) for k in range(len(real) - 1)
                 if simple(stmts[real[k]]) and simple(stmts[real[k + 1]])]
        if not pairs:
            continue
        i, j = rng.choice(pairs)
        stmts[i], stmts[j] = stmts[j], stmts[i]
        return text[:m.start(1)] + "".join(stmts) + text[m.end(1):]
    return None


def mut_invert(text, rng):
    """Turn `if (c) { A } return X;` into `if (!c) return X; A` and back."""
    m = re.search(r"if\s*\(([^()]*(?:\([^()]*\))?[^()]*)\)\s*\n?\s*"
                  r"return ([^;]+);\s*\n\s*return ([^;]+);", text)
    if m:
        cond, a, b = m.group(1).strip(), m.group(2).strip(), m.group(3).strip()
        neg = cond[1:] if cond.startswith("!") else "!(%s)" % cond
        return (text[:m.start()]
                + "if (%s)\n        return %s;\n    return %s;" % (neg, b, a)
                + text[m.end():])
    return None


def mut_member(text, rng):
    """Move a free function taking T* first into T as a member.

    This is the lever that fixed sub_826C0FC8 when six free-function shapes
    could not, so it is worth trying on anything with a transposition.
    """
    m = re.search(r"\n([A-Za-z_][\w:<>* ]*?)\s+(\w+)\(\s*(\w+)\*\s*(\w+)\s*"
                  r"(,\s*[^)]*)?\)\s*\{", text)
    if not m:
        return None
    ret, fname, tname, pname, rest = m.groups()
    rest = (rest or "").strip()
    struct_re = re.compile(r"struct\s+%s\s*\{" % re.escape(tname))
    sm = struct_re.search(text)
    if not sm:
        return None
    body_start = m.end()
    depth, i = 1, body_start
    while i < len(text) and depth:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    body = text[body_start:i - 1]
    args = rest[1:].strip() if rest.startswith(",") else rest

    # A parameter that shares a name with a member would SHADOW it once the
    # `pname->` prefix is dropped, silently changing what the function does.
    # sub_826C1480 has a parameter `int f` and a member `int f[12]`, so
    # `s->f[3] = d` became `f[3] = d` referring to the int. Refuse instead.
    members = members_of(text, tname)
    params = set(re.findall(r"\b(\w+)\s*(?:,|$)", args))
    if members & params:
        return None

    # `pname->x` becomes `x`; a bare `pname` becomes `this`. Code only.
    body = code_sub(r"\b%s\s*->\s*" % re.escape(pname), "", body)
    body = code_sub(r"\b%s\b" % re.escape(pname), "this", body)
    decl = "    %s %s(%s);\n" % (ret.strip(), fname, args)
    out = text[:sm.end()] + "\n" + decl + text[sm.end():]
    shift = len(decl) + 1
    ns, ne = m.start() + shift, i + shift
    out = (out[:ns] + "\n%s %s::%s(%s)\n{%s}"
           % (ret.strip(), tname, fname, args, body) + out[ne:])
    return out


def mut_temp(text, rng):
    """Hoist a `a->b` subexpression into a local, or split a chained access.

    This changes what the compiler has to keep live across the statement,
    which is what register assignment follows from -- the failure mode of six
    of the eleven stalls. It only rewrites CODE, never comments.
    """
    if "__tmp0" in text:
        return None
    code_only = SPAN_RE.sub("", text)
    hits = re.findall(r"\b(\w+)\s*->\s*(\w+)\s*->", code_only)
    if not hits:
        return None
    obj, fld = rng.choice(hits)
    prefix = "%s->%s" % (obj, fld)

    line = re.search(r"\n([ \t]+)[^\n]*" + re.escape(prefix) + r"\s*->", text)
    if not line:
        return None
    indent = line.group(1)

    # The declared type is unknown without a parser, so reuse the member
    # access itself in a way that is always well-typed: take the address and
    # dereference it. That keeps the value in a named local without naming
    # its type.
    decl = "\n%s__typeof0* __tmp0 = &*(%s);" % (indent, prefix)
    body = text[:line.start()] + decl + text[line.start():]
    body = code_sub(re.escape(prefix) + r"\s*->", "__tmp0->", body)
    # Give __typeof0 a definition by deriving it from the existing pointer
    # declaration is not possible textually, so declare via the expression.
    body = body.replace("__typeof0* __tmp0 = &*(%s);" % prefix,
                        "typedef __typeof0 unused_t;", 1)
    # The above cannot be made well-typed without a parser. Fall back to the
    # one liveness change that IS always valid: repeating the access into a
    # statement of its own before first use.
    out = text[:line.start()] + "\n%s(void)(%s);" % (indent, prefix) \
        + text[line.start():]
    return out if out != text else None


def _pointer_decls(text):
    """[(type, name)] for every `T* name` this source declares.

    Textual, and it does not need to be better than that: the sources this
    project writes declare their struct types by name a few lines above the
    function, so the type is recoverable without a parser. That is the whole
    reason the mutations below can do what `mut_temp` could not.
    """
    code_only = SPAN_RE.sub("", text)
    out = []
    for m in re.finditer(r"\b(?:const\s+)?(\w+)\s*\*\s*(\w+)\s*[,)=;]",
                         code_only):
        ty, nm = m.group(1), m.group(2)
        if ty in ("return", "sizeof", "void") or nm in ("const",):
            continue
        if (ty, nm) not in out:
            out.append((ty, nm))
    return out


def mut_constview(text, rng):
    """Read SOME uses of `p->` through a named `const T* view = p;`.

    THE LEVER THIS EXISTS FOR. `sub_82667EE0` (VectorGrow) was one word short
    on a `mullw`'s operand order, which is decided by which read of a field
    becomes the CSE representative. A named const-qualified view of the same
    pointer breaks the value-number tie and it comes out 32 of 32.

    Two details are load-bearing and both were measured:

      The local must be NAMED. An inline cast and an inlined `const`
      accessor are both folded back to the same value number and change
      nothing, which is why `mut_temp`'s `(void)(expr);` never moved
      anything.

      Only SOME uses are rewritten. Rewriting all of them just renames the
      pointer and restores the tie; the point is to split the reads into two
      value numbers. So a random non-empty proper subset is chosen.

    Its known limit, from the arena twins `sub_82606EC8`/`sub_82606FD8`: the
    view has to name a field reached through a POINTER, not a global's
    lis/addi address expression. This mutation therefore only fires where a
    pointer declaration exists, which is the same condition.
    """
    if "__cv" in text:
        return None
    decls = _pointer_decls(text)
    if not decls:
        return None
    rng.shuffle(decls)
    for ty, nm in decls:
        # READS only, and only OUTSIDE comments.
        #
        # Two separate mistakes lived here. Writes: `__cv->prev = 0;` assigns
        # through a const pointer and is error C2166, which 31 of 40 seeds
        # produced. And comments: every source in this project opens with the
        # target's disassembly, and those listings are annotated with things
        # like `h->flags`. Searching the raw text made the FIRST use a
        # position in the header comment, before any function brace, so the
        # insertion point could not be found and the mutation silently
        # returned None on every real source while passing on a synthetic one
        # that had no comments.
        spans = [(m.start(), m.end()) for m in SPAN_RE.finditer(text)]

        def in_comment(pos):
            return any(a <= pos < b for a, b in spans)

        uses = []
        for m in re.finditer(r"\b" + re.escape(nm) + r"\s*->\s*\w+", text):
            if in_comment(m.start()):
                continue
            # ADDRESS-OF is not a read either. `&__cv->watch` has type
            # `LNode* const*` and will not initialise an `LNode**`
            # (error C2440).
            before = text[:m.start()].rstrip()
            if before.endswith("&"):
                continue
            # A generous lookahead, and one that steps over a subscript:
            # `p->arr[i] = 0` is a write, and matching only `->name` then
            # looking for `=` sees the `[` and calls it a read.
            after = text[m.end():m.end() + 24]
            after = re.sub(r"^\s*\[[^\]]*\]", "", after)
            if re.match(r"\s*(?:[-+*/%&|^]=|<<=|>>=|=(?!=))", after):
                continue
            uses.append(m.start())
        if len(uses) < 2:
            continue

        # WHERE the declaration goes. This used to take the first `) {` in
        # the file, which is the first FUNCTION in the file -- so on a source
        # with a small static helper above the real one, `const LList* __cv
        # = h;` landed in the helper where `h` does not exist, and every
        # single mutation failed to compile with "undeclared identifier".
        # The permuter reported it as "discarded" and the mutation looked
        # like it was simply never firing.
        #
        # If `nm` is a LOCAL, the view must come after its declaration; if it
        # is a PARAMETER, after the opening brace of the function that takes
        # it. Both are found by looking backwards from the first use.
        decl_here = None
        for m in re.finditer(r"\b(?:const\s+)?" + re.escape(ty)
                             + r"\s*\*\s*" + re.escape(nm) + r"\s*=[^;]*;",
                             text):
            if m.end() <= uses[0]:
                decl_here = m.end()
        if decl_here is None:
            braces = [m.end() for m in re.finditer(r"\)\s*\n?\s*\{", text)
                      if m.end() <= uses[0]]
            if not braces:
                continue
            decl_here = braces[-1]
        at = decl_here

        # Only uses in the SAME function body. A source with two functions
        # that both take a `T* p` had its declaration put in the first and
        # its uses rewritten in both, giving "'__cv' : undeclared identifier"
        # in the second. Walk the braces forward from the insertion point to
        # find where this body ends.
        depth = 0
        end = len(text)
        for i in range(at, len(text)):
            c = text[i]
            if c == "{":
                depth += 1
            elif c == "}":
                if depth == 0:
                    end = i
                    break
                depth -= 1
        uses = [u for u in uses if at <= u < end]
        if len(uses) < 2:
            continue
        # A proper, non-empty subset: renaming every use restores the tie.
        k = rng.randrange(1, len(uses))
        chosen = set(rng.sample(uses, k))
        out = []
        last = 0
        for pos in sorted(uses):
            if pos < at or pos not in chosen:
                continue
            out.append(text[last:pos])
            out.append("__cv")
            last = pos + len(nm)
        if not out:
            continue
        out.append(text[last:])
        rewritten = "".join(out)
        decl = "\n    const %s* __cv = %s;" % (ty, nm)
        return rewritten[:at] + decl + rewritten[at:]
    return None


def mut_addrof(text, rng):
    """Store through `T** pp = &o->field;` instead of assigning `o->field`.

    Two constant offsets off one base provably cannot alias, so MSVC freely
    hoists a later load above a store. Taking the member's address stops it:
    `sub_827FEE48` went from 9 of 11 to 11 of 11 on exactly this, and
    `sub_8216E778` needed it at all three update sites at once.

    Textual and deliberately narrow -- it only fires on a whole-statement
    assignment `a->b = c;`, which is the shape the lever was measured on.
    """
    if "__pp" in text:
        return None
    code_only = SPAN_RE.sub("", text)
    hits = list(re.finditer(r"\n([ \t]+)(\w+)\s*->\s*(\w+)\s*=\s*([^;=][^;]*);",
                            code_only))
    if not hits:
        return None
    h = rng.choice(hits)
    obj, fld, val = h.group(2), h.group(3), h.group(4)
    stmt = "%s->%s = %s;" % (obj, fld, val)
    if text.count(stmt) != 1:
        return None

    # The field's type is not recoverable textually, and cl 15.00 has no
    # `auto`. A one-line function template supplies the type instead: it is
    # well-typed for any field, it takes the member's ADDRESS, and it adds a
    # call boundary -- which agent measurement on sub_825FAC00 showed is
    # sometimes required on top of the address itself (a bare `int*` local
    # sat at 16 of 26 there; an inlined `static void Pack(int*, ...)` reached
    # 28 of 28).
    # T is deduced from the POINTER only. Deducing it from both parameters
    # makes `__store_through(&n->prev, 0)` ambiguous -- the field is `Node*`
    # and the literal is `int` -- which is error C2782 and killed 12 of 40
    # seeds. `__id<T>::type` is a non-deduced context, so the value simply
    # converts to whatever the field is.
    helper = ("template <class T> struct __id { typedef T type; };\n"
              "template <class T>\n"
              "static __forceinline void __store_through("
              "T* __pp, typename __id<T>::type __v)\n"
              "{ *__pp = __v; }\n\n")
    new = "__store_through(&%s->%s, %s);" % (obj, fld, val)
    out = text.replace(stmt, new, 1)
    if "__store_through" not in text:
        # Insert the helper above the first function definition, after the
        # includes and type declarations it may depend on.
        anchor = re.search(r"\n(?=[A-Za-z_][\w:<>, \*&]*\s+[\w:~]+\s*\([^;]*\)"
                           r"\s*\n?\s*\{)", out)
        at = anchor.start() + 1 if anchor else 0
        out = out[:at] + helper + out[at:]
    return out if out != text else None


def mut_sign(text, rng):
    """Swap int/unsigned on one declaration."""
    subs = [(r"\bint\b", "unsigned"), (r"\bunsigned\b", "int"),
            (r"\bs32\b", "u32"), (r"\bu32\b", "s32")]
    rng.shuffle(subs)
    for pat, rep in subs:
        hits = list(re.finditer(pat, text))
        if not hits:
            continue
        h = rng.choice(hits)
        return text[:h.start()] + rep + text[h.end():]
    return None


def mut_compare(text, rng):
    """Rewrite a comparison as an equivalent one that compiles differently.

    For an UNSIGNED value `x != 0` and `x > 0` mean exactly the same thing,
    and the compiler emits a different branch condition for each: `beq-` for
    the first, `ble-` for the second. sub_822D0BE8 came down to that single
    word and matched the moment the comparison was written the other way.

    Nothing here changes what the code computes; only which condition bit the
    branch tests.
    """
    forms = [
        (r"\(\s*(\w+(?:->\w+)*)\s*\)", r"(\1 > 0)"),
        (r"\(\s*(\w+(?:->\w+)*)\s*>\s*0\s*\)", r"(\1 != 0)"),
        (r"\(\s*(\w+(?:->\w+)*)\s*!=\s*0\s*\)", r"(\1)"),
        (r"\(\s*!\s*(\w+(?:->\w+)*)\s*\)", r"(\1 == 0)"),
        (r"\(\s*(\w+(?:->\w+)*)\s*==\s*0\s*\)", r"(\1 <= 0)"),
        (r"\(\s*(\w+(?:->\w+)*)\s*<=\s*0\s*\)", r"(!\1)"),
    ]
    rng.shuffle(forms)
    for pat, rep in forms:
        hits = [m for m in re.finditer(r"if\s*" + pat, SPAN_RE.sub("", text))]
        if not hits:
            continue
        out = code_sub(r"if\s*" + pat, "if " + rep, text)
        if out != text:
            return out
    return None


def mut_inline(text, rng):
    """Remove a local that just names a subexpression, or the reverse.

    `T* t = p->member; use(t);` and `use(p->member);` compute the same thing
    and give the register allocator different problems -- which is the
    failure mode of six of the eleven stalls.
    """
    code_only = SPAN_RE.sub("", text)
    decls = list(re.finditer(
        r"\n[ \t]*([A-Za-z_]\w*\s*\**)\s+(\w+)\s*=\s*([^;=][^;]*);",
        code_only))
    if not decls:
        return None
    m = rng.choice(decls)
    name, expr = m.group(2), m.group(3).strip()
    if name in ("this",) or len(expr) > 60:
        return None
    real = re.search(
        r"\n[ \t]*" + re.escape(m.group(1).strip()) +
        r"\s+" + re.escape(name) + r"\s*=\s*" + re.escape(expr) + r"\s*;",
        text)
    if not real:
        return None
    out = text[:real.start()] + text[real.end():]
    uses = len(re.findall(r"\b%s\b" % re.escape(name), SPAN_RE.sub("", out)))
    if uses == 0:
        return None
    out = code_sub(r"\b%s\b" % re.escape(name), "(" + expr + ")", out)
    return out


MUTATIONS = [
    ("reorder", mut_reorder),
    ("invert", mut_invert),
    ("member", mut_member),
    ("compare", mut_compare),
    ("inline", mut_inline),
    ("constview", mut_constview),
    ("addrof", mut_addrof),
    ("temp", mut_temp),
    ("sign", mut_sign),
]


# ------------------------------------------------------------------ main

def main(argv):
    args = [a for a in argv[1:] if not a.startswith("--")]
    if len(args) < 2:
        print(__doc__)
        return 1
    iters = 400
    seed = 1
    for i, a in enumerate(argv):
        if a == "--iters":
            iters = int(argv[i + 1])
        if a == "--seed":
            seed = int(argv[i + 1])
    want_sym = None
    if "--sym" in argv:
        want_sym = argv[argv.index("--sym") + 1]

    src = Path(args[0])
    target = int(args[1], 16)
    base_text = src.read_text()

    img = Image()
    sizes = dict(load_inventory())
    if target not in sizes:
        print("%08X is not a known function start" % target)
        return 1
    tsize = sizes[target]
    tbytes = img.read(target, tsize)
    words = tsize // 4

    start = compile_and_score(base_text, tbytes, tsize, target, want_sym)
    if start is None:
        print("the unmodified source does not compile; nothing to permute")
        return 2

    print("target %08X, %d byte(s), %d word(s)" % (target, tsize, words))
    print("source %s" % src)
    print("start  %d/%d words at %d bytes\n" % (start[0], words, start[1]))
    if start[0] == words and start[1] == tsize:
        print("already an exact match; nothing to do")
        return 0

    rng = random.Random(seed)
    best_text, best = base_text, start
    pool = [(base_text, start)]
    tried = compiled = discarded = 0
    used = {}
    t0 = time.time()

    for _ in range(iters):
        text, _sc = pool[rng.randrange(len(pool))]
        name, fn = MUTATIONS[rng.randrange(len(MUTATIONS))]
        try:
            cand = fn(text, rng)
        except Exception:
            cand = None
        if cand is None or cand == text:
            continue
        tried += 1
        res = compile_and_score(cand, tbytes, tsize, target, want_sym)
        if res is None:
            discarded += 1
            continue
        compiled += 1
        used[name] = used.get(name, 0) + 1
        if rank(res, tsize) > rank(best, tsize):
            best, best_text = res, cand
            print("  %-8s -> %d/%d words at %d bytes"
                  % (name, res[0], words, res[1]))
            pool.append((cand, res))
            if res[0] == words and res[1] == tsize:
                break
        elif rank(res, tsize) == rank(best, tsize) and len(pool) < 24:
            pool.append((cand, res))

    dt = time.time() - t0
    out = WORK / ("best_%08X.cpp" % target)
    out.write_text(best_text)

    print("")
    print("%d mutation(s) produced, %d compiled, %d discarded, in %.1fs"
          % (tried, compiled, discarded, dt))
    if used:
        print("  mutations that compiled: %s"
              % ", ".join("%s x%d" % kv for kv in sorted(used.items())))
    print("best: %d/%d words at %d bytes (started at %d/%d)"
          % (best[0], words, best[1], start[0], words))
    print("  -> %s" % out)

    if best[0] == words and best[1] == tsize:
        print("")
        print("EXACT MATCH. Copy it over %s and add the address to"
              " src/manifest.txt." % src)
        return 0
    if best[0] > start[0]:
        print("")
        print("Improved but not matched. The remaining diff:")
        for i in range(min(len(best[2]), tsize) // 4):
            va = target + i * 4
            a = struct.unpack_from(">I", tbytes, i * 4)[0]
            b = struct.unpack_from(">I", best[2], i * 4)[0]
            if a != b:
                print("  %08X  want %08x  %-28s" % (va, a, ppcdis.words([a], va)[0][2]))
                print("            got  %08x  %-28s" % (b, ppcdis.words([b], va)[0][2]))
    return 1




def selftest():
    """Rediscover an answer already known by hand.

    sub_826C0FC8 scores 2/6 as a free function and 6/6 as a member. If the
    permuter cannot find that, its mutations are not reaching the thing they
    exist to reach, and any negative result it reports is meaningless.
    """
    known = WORK / "selftest_stride24.cpp"
    WORK.mkdir(parents=True, exist_ok=True)
    known.write_text(
        '#include "types.h"\n\n'
        "struct E24 { char unk0000[24]; };\n"
        "ASSERT_SIZE(E24, 24);\n"
        "struct Holder24 { char unk0000[0x18]; E24* items; };\n"
        "ASSERT_OFFSET(Holder24, items, 0x18);\n\n"
        "E24* At24(Holder24* h, int i)\n{\n    return &h->items[i];\n}\n")
    rc = main(["permuter", str(known), "826C0FC8", "--iters", "120"])
    print("")
    if rc == 0:
        print("SELF TEST PASSED: rediscovered the member-function match.")
        return 0
    print("SELF TEST FAILED: the known 2/6 -> 6/6 answer was not found.")
    print("The mutations are not reaching register allocation, so a negative")
    print("result from this tool means nothing.")
    return 1


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(selftest())
    sys.exit(main(sys.argv))
