// sub_826C1480 -- stores 12 values into a struct. 76 bytes, 180 callers,
// no conditional branch at all.
//
// Read from the disassembly: r4, r5, r6 go to +0x00, +0x04, +0x08 and
// r8, r9, r10 go to +0x0C, +0x10, +0x14 -- r7 is loaded over and never
// stored, so the FOURTH register parameter is unused. The remaining six
// values come from the caller's stack at r1+0x54, +0x5C, +0x64, +0x6C,
// +0x74, +0x7C (the low word of six 8-byte parameter slots).
//
// Chosen as a branchless target: if branch layout is what blocks
// sub_82806FD0 (see MATCHED.md), a function with no conditional branch
// should match cleanly.

struct S { int f[12]; };

void Init12(S* s, int a, int b, int c, int unused,
            int d, int e, int f,
            int g, int h, int i, int j, int k, int l)
{
    // Written in the target's own store order. The compiler schedules the
    // r6 store where the source puts it: hoisting f[2] to the top produced
    // the same 19 instructions with `stw r6,8(r3)` one position too early.
    //
    // NOT MATCHED: 13 of 19 words. All six wrong words are the SAME ONE
    // STORE, `stw r6,8(r3)`, five slots too early, and the five stack loads
    // it displaces. The target issues it as LATE as it can -- one
    // instruction before `lwz r6,124(r1)` clobbers r6 -- and we issue it as
    // early as we can, immediately after the three register-parameter
    // stores.
    //
    // Seven more shapes give the identical schedule: the five stack
    // parameters copied into locals ahead of the f[2] store (and all six,
    // and each on its own line); f[2] moved after f[0] and f[1]; the whole
    // function in index order 0..11 (which moves the store to slot ONE, the
    // wrong way); and the tail six arriving as a by-value struct, which
    // changes the stack offsets but not the order.
    //
    // The local-copy idea was the interesting one and it failed cleanly: a
    // load of an incoming stack parameter is NOT pinned relative to a store
    // through `s` the way src/list_insert.cpp's load is pinned relative to
    // its store. MSVC knows the caller's parameter area cannot be the
    // object, so the loads float freely and no source-level read order
    // reaches them.
    s->f[3] = d;  s->f[4] = e;  s->f[5]  = f;
    s->f[2] = c;
    s->f[0] = a;  s->f[1] = b;
    s->f[6] = g;  s->f[7] = h;  s->f[8]  = i;
    s->f[9] = j;  s->f[10] = k; s->f[11] = l;
}
