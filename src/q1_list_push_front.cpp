#include "types.h"

// sub_82155968 -- push a node on the front of an intrusive list whose link
// offset is a RUNTIME u16 field, optionally unlinking it from a second list
// first. 112 B, 9 callers.
//
//      cmplwi  cr6,r4,0
//      beqlr   cr6                v == 0: nothing to do
//      lhz     r11,6(r3)          h->flags
//      lwz     r8,0(r3)           old = h->head       (BEFORE the unlink)
//      clrlwi  r9,r11,31
//      lhz     r10,4(r3)          off = h->linkOff
//      cmplwi  cr6,r9,0
//      beq-    cr6,tail
//      lwz     r11,8(r3)          p  = h->watch
//      addi    r9,r3,8            pp = &h->watch
//      cmplwi  cr6,r11,0
//      beq-    cr6,tail           peeled `p != 0`
//  L:  cmplw   cr6,r11,r4
//      beq-    cr6,hit            the break
//      add     r9,r11,r10         pp = link of p
//      lwzx    r11,r11,r10        p  = *pp
//      cmplwi  cr6,r11,0
//      bne+    cr6,L
//      stwx    r8,r10,r4          (falls straight into the tail)
//      stw     r4,0(r3)
//      blr
// hit: cmplwi  cr6,r11,0
//      beq-    cr6,tail
//      lwzx    r11,r11,r10
//      stw     r11,0(r9)          *pp = *link(p)
// tail:stwx    r8,r10,r4          *link(v) = old
//      stw     r4,0(r3)           h->head = v
//      blr
//
// The head is loaded at the TOP, before the unlink store, so it is a named
// local read early -- the store through `pp` could alias h+0 and would
// otherwise force a reload.  `off` is likewise read once, so it is a local
// too; spelled at each use the same store would force it to be re-loaded.
//
// The loop is a rotated `while (p != 0)` whose body starts with the break:
// the peeled null test guards it, the back edge repeats it, and the two exits
// that KNOW p is null fall through to the tail while the `break` exit -- the
// only one where the compiler cannot prove it -- re-tests.  That is the
// sub_822CEE08 pattern exactly.
//
// `lhz` with no `extsh` on either field, and both used unwidened in `add` and
// `lwzx`, so the offset is an unsigned 16-bit field.
//
// NOT MATCHED: 18 of 28 words. EVERY INSTRUCTION AND EVERY OFFSET IS RIGHT
// and the size is exact; r9 and r10 are TRANSPOSED throughout. The target
// keeps `off` in r10 and uses r9 for the short-lived values -- the
// `flags & 1` temp first, then `pp` -- and we do the opposite.
//
// MATCHED.md's rule for a transposed pair is "change the flag", and that is
// not available here: /O2 /Os rewrites the guard as `clrlwi.` and the body
// as 96 bytes, four words shorter, which is the OTHER documented /Os
// signature and rules it out on its own. Its companion rule, "try the member
// form when the first argument is an object pointer", also does nothing.
//
// Ten shapes were compiled, all 112 bytes with the same transposition:
// `old` and `off` declared in either order; the flag word named in a local
// declared first and declared last; the flag test named as a `bool`; the
// test written `(h->flags & 1) != 0`; `off` declared `u16` instead of `u32`
// (which is 128 bytes -- it re-masks at all four uses, so the field is read
// into a 32-bit local); the watch pointer read as `h->watch` rather than
// `*pp`; a const view of the link offset; and the member-function form.
//
// So this is a register-NAMING difference with the whole instruction stream
// already correct, and neither of the two levers that normally move one
// applies.
struct LNode;

struct LList
{
    /* 0x00 */ LNode* head;
    /* 0x04 */ u16    linkOff;
    /* 0x06 */ u16    flags;
    /* 0x08 */ LNode* watch;
};
ASSERT_OFFSET(LList, linkOff, 0x04);
ASSERT_OFFSET(LList, flags,   0x06);
ASSERT_OFFSET(LList, watch,   0x08);

static LNode** LinkAt(void* n, u32 off)
{
    return (LNode**)((u8*)n + off);
}

void ListPushFront(LList* h, LNode* v)
{
    if (v == 0)
        return;

    LNode* old = h->head;
    u32    off = h->linkOff;

    if (h->flags & 1)
    {
        LNode** pp = &h->watch;
        LNode*  p  = *pp;

        while (p != 0)
        {
            if (p == v)
                break;
            pp = LinkAt(p, off);
            p  = *pp;
        }

        if (p != 0)
            *pp = *LinkAt(p, off);
    }

    *LinkAt(v, off) = old;
    h->head = v;
}
