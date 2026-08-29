// sub_82806FD0 -- element accessor for a chunked (deque-like) container.
// 84 bytes, 220 callers, calls nothing.
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
//   zeroed early -- the compiler hoists the default return value above the
//   branch.
//
// First attempt.

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
    // The null is written as an explicit value so the compiler materialises
    // it BEFORE the guard: the target does `li r3,0` as its second
    // instruction, which is what forces `this` into r10 and defers the
    // base load, and which lets the guard become a conditional return
    // (bgtlr) instead of a forward branch to an epilogue.
    void* none = 0;
    Map* m = self->map;
    unsigned total = ((unsigned)(m->nchunks - 1) << 5)
                   + (unsigned)((m->p08 - m->p0C) >> 4);
    unsigned i = self->base - k;
    if (i > total)
        return none;
    return m->chunks[i >> 5] + (i & 31) * 16;
}
