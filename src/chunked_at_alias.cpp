// sub_82806FD0 -- aliasing hypothesis.
//
// With Map* and Chunked* as distinct types, MSVC may reorder the load of
// self->base above the loads from *m, because type-based aliasing says they
// cannot overlap. The target does NOT reorder: it saves `this` in r10 and
// loads base LATE, after every m-> load. Reading everything through one
// pointer type removes the compiler's licence to reorder.
typedef unsigned u32;

void* ChunkedAt(char* self, u32 k)
{
    char* m = *(char**)(self + 0x18);
    u32 n = *(u32*)(m + 0x1C);
    char* p08 = *(char**)(m + 0x08);
    char* p0C = *(char**)(m + 0x0C);
    u32 total = ((n - 1) << 5) + (u32)((p08 - p0C) >> 4);
    u32 i = *(u32*)(self + 0x20) - k;
    if (i > total) return 0;
    return *(char**)(*(char**)(m + 0x18) + (i >> 5) * 4) + (i & 31) * 16;
}
