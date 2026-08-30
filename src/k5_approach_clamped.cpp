// sub_821AEC78 -- move a float towards a target by a fraction of the gap,
// with the per-call step clamped symmetrically. 100 B, 5 callers.
//
// NEAR MISS: 14 of 25 words at /O2. Every instruction, every immediate,
// every branch displacement and the whole schedule are correct, and the
// LENGTH is correct. All eleven wrong words are one thing: f0 and f13 are
// exactly TRANSPOSED throughout.
//
//      want fmuls f13,f2,f3        got fmuls f0,f2,f3
//      want lfs   f0,12140(r11)    got lfs   f13,0(r11)
//      want fmuls f13,f13,f0       got fmuls f0,f0,f13
//      want fcmpu cr6,f13,f12      got fcmpu cr6,f0,f12
//      want fmr   f13,f12          got fmr   f0,f12
//      want fmuls f0,f11,f0        got fmuls f13,f11,f13
//      want fmuls f13,f10,f13      got fmuls f0,f10,f0
//      want fcmpu cr6,f13,f0       got fcmpu cr6,f0,f13
//      want fadds f0,f12,f0        got fadds f0,f12,f13
//      want fneg  f0,f0            got fneg  f13,f13
//      want fcmpu cr6,f13,f0       got fcmpu cr6,f0,f13
//      want fadds f0,f12,f13       got fadds f0,f12,f0
//
// MSVC allocates the volatile FP registers f0, f13, f12, f11, f10 in that
// order to the temporaries as they are CREATED. Ours creates the product
// dt*rate first and the 60.0f constant second; the image creates them the
// other way round. Nothing below reaches that ordering.
//
// What IS established:
//
//   * 82002F6C = 60.0f, 82002D40 = 1.0f. 60.0f is loaded once and shared by
//     both products, and is written LAST in each -- naming it in a local
//     (`f32 k = 60.0f; dt * rate * k`) makes /fp:fast reassociate to
//     `(dt * k) * rate` and loses the first multiply entirely. Parenthesising
//     restores the schedule and changes no register.
//   * The `fneg` is why the low clamp ADDS `-m` instead of subtracting `m`;
//     a subtraction emits `fsubs` and there is nothing to negate. Negating in
//     place -- `m = -m;` -- makes the two clamped arms textually identical so
//     MSVC tail-merges them, which is the backward `blt+` at 821AECCC, and
//     the unclamped arm keeps its own copy of the store. This is what fixed
//     the length: an if/else-if/else with three distinct stores is 112 bytes.
//   * `blt-` skipping the `fmr` inverts `>=`, so the clamp is `>=`, not `>`.
//   * SPELLING `*p` AT EVERY USE rather than naming it in a local is worth
//     two words on its own: with `f32 v = *p;` declared before `m`, both
//     `fadds` put the OTHER operand in rA -- the read-order rule for `add`,
//     with the local's read position ahead of m's. Spelled out, the CSE
//     representative sits in the `(target - *p)` line, after m, and both
//     adds come out with `*p` in rA as the image has them.
//
// THIRTEEN SOURCE SHAPES were measured and all of them are still 14 of 25
// with the identical transposition unless noted: a member function on a
// struct whose float is at +0 (MATCHED.md's member lever, and this is
// exactly its stated signature -- transposed registers with the first
// argument an object pointer); the member form with `*p` spelled out;
// `60.0f * (dt * rate)`, which MSVC canonicalises straight back; a named
// 60.0f local with and without parentheses; `v` declared after `m` (12 of
// 25); `v` declared first (12 of 25); `m` declared before `t` (10 of 25); an
// inlined helper computing the product; an inlined helper returning the
// clamped factor; a TWO-LEVEL inlined helper, since inlining depth is what
// moved sub_82164040; the product passed to a helper with the constant as
// its second parameter; and the clamp writing into a second variable so the
// product and the clamped value are separate vregs that coalesce.
//
// TWO MORE, both byte-identical to the baseline with the same eleven-word
// transposition:
//
//   * the 60.0f as an `extern const f32` rather than a literal, written
//     `(dt * rate) * g_k60`. The point was that a genuine global load cannot
//     be folded or sunk to its use the way a materialised literal can, so
//     PRE might hoist it to the entry and create its value first. It does
//     not: the emitted `lis`/`lfs` pair is in the same place and f13 still
//     holds it. (Both `lis` words are relocated either way, so this costs
//     nothing in the score.)
//   * `s` declared before `m`, leaving `m` as the last declaration. The
//     earlier measurements moved `v` and `m` relative to `t`, but not `s`
//     relative to `m`. No change.
//
// SAID AS TWO WEBS, which is the shortest true statement of what is left.
// The volatile FP registers go f0, f13, f12, f11, f10 to the values as they
// are created, and there are two long webs:
//
//      W1 = { 60.0f, m, the stored sum }      W2 = { dt*rate, t, s }
//
// The image gives f0 to W1 and f13 to W2; we give f0 to W2 and f13 to W1,
// and everything else -- f12 for 1.0f then *p, f11 for dt*limit, f10 for
// target-*p -- agrees. `(dt * rate) * 60.0f` creates W2's first member
// before W1's because the product is the LEFT operand of the outer multiply,
// and every spelling that puts the constant first either reassociates under
// /fp:fast or is canonicalised back to this one.
//
// THE FLAG AXIS IS EXHAUSTED, not merely untried: `tools/flagsweep.py`
// compiles all 72 combinations, 0 failed, and the best is this one.
//
//      10/25   100 B    22 combinations, e.g. /O2 /Gy /GS- /fp:fast
//       4/25    96 B    14 combinations, e.g. /O2 /Os /Gy /GS- /fp:fast
//       2/25   100 B    14 combinations, /Os with /fp:precise
//       0/25   104 B    22 combinations, /fp:precise
//
// /O2 /Os is worse STRUCTURALLY -- it tail-merges the store and the blr into
// one copy, 24 words against the image's 25 -- and the transposition
// survives it as well, so it is not the /Os transposition signature.
//
// This is a register-ALLOCATION stall, the class MATCHED.md names as the one
// the permuter's mutations do not reach. That is a mechanism, and per this
// project's own record a mechanism does not bound the search: the next thing
// to try is something that changes register PRESSURE rather than statement
// order or inlining depth.

#include "types.h"

void ApproachClamped(f32* p, f32 target, f32 dt, f32 rate, f32 limit)
{
    f32 t = dt * rate * 60.0f;
    if (t >= 1.0f)
        t = 1.0f;

    f32 m = dt * limit * 60.0f;
    f32 s = (target - *p) * t;

    if (s > m)
    {
        *p = *p + m;
        return;
    }

    m = -m;
    if (s < m)
    {
        *p = *p + m;
        return;
    }

    *p = *p + s;
}
