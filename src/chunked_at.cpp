// sub_82806FD0 -- element accessor for a chunked (deque-like) container.
// 84 bytes, 220 callers, calls nothing.  /O2 /Os.
//
// Read from the disassembly:
//   r3 = this, r4 = the index argument.
//   [this+0x18] is a "map" object; [this+0x20] a base index.
//   map+0x08 / map+0x0C are pointers whose difference, >>4, is the element
//     count in the partial chunk -- so elements are 16 bytes.
//   map+0x18 is the chunk-pointer array, map+0x1C the chunk count.
//   rlwinm r9,r10,29,3,29  =  (i >> 5) * 4     chunk pointer index
//   rlwinm r10,r10,4,23,27 =  (i & 31) * 16    offset within the chunk
//   so a chunk holds 32 elements of 16 bytes.
//   The bounds test is UNSIGNED (cmplw) and returns 0 via bgtlr, with r3
//   zeroed early.
//
// MATCHED, 21 of 21 words.
//
// THE ANSWER: THE ZERO IS A PHI, NOT AN EARLY RETURN. This function was one
// of the six in MATCHED.md's "What still resists", listed as "branch
// polarity -- bgtlr vs ble-, a probability decision". It is not a
// probability decision and it is not polarity.
//
//      if (i > total) return none;          li r3,0 in its own block after
//      return chunk + off;                  a forward `ble-`; 10 of 21
//
//      void* r = 0;                         li r3,0 THIRD, above the guard,
//      if (i <= total) r = chunk + off;     and the guard becomes `bgtlr`;
//      return r;                            21 of 21
//
// Written as an early return, the zero belongs to one path and MSVC gives
// it a block of its own at the bottom. Written as an accumulator that one
// path overwrites, the zero is the merge value: it has to exist before the
// branch, so it is materialised in the third instruction, and the guard
// then needs no block at all because r3 is already correct -- a conditional
// return.
//
// Everything else in this function follows from that one `li r3,0`. It
// clobbers r3 while `this` is still needed, which is where `mr r10,r3`
// comes from, and the copy in turn defers `lwz r10,32(r10)` -- the
// self->base load -- past three map loads it would otherwise precede. Eight
// of the eleven wrong words were downstream of the zero's position, not
// independent problems.
//
// So: A DEFAULT RETURN VALUE MATERIALISED ABOVE A GUARD MEANS A SINGLE
// RETURN THROUGH AN ACCUMULATOR. Declaring the zero in a local and
// returning it early is NOT the same thing and does not do it -- the
// distinction is whether both paths reach one `return`.
//
// The ternary form is a third shape and also wrong: it keeps the fast path
// as the fall-through with the zero out of line (`bgt-` forward), 84 bytes
// and the branch inverted.

struct Map
{
    char  pad00[8];
    char* p08;          // +0x08
    char* p0C;          // +0x0C
    char  pad10[8];
    char** chunks;      // +0x18
    int   nchunks;      // +0x1C
};

struct Chunked
{
    char pad00[0x18];
    Map* map;           // +0x18
    char pad1C[4];      // +0x1C -- the target loads base from 0x20, not 0x1C
    unsigned base;      // +0x20
};

void* ChunkedAt(Chunked* self, unsigned k)
{
    void* r = 0;
    Map* m = self->map;
    unsigned total = ((unsigned)(m->nchunks - 1) << 5)
                   + (unsigned)((m->p08 - m->p0C) >> 4);
    unsigned i = self->base - k;
    if (i <= total)
        r = m->chunks[i >> 5] + (i & 31) * 16;
    return r;
}
