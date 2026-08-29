// sub_822607F0 -- 16-bit indices for a grid drawn as one triangle strip,
// with degenerate indices stitching each row to the next.
//
// Read from the disassembly:
//   r5 = width, r6 = height, r7 = output pointer.
//   r3 and r4 are never read before being written, so the first two
//   parameters are unused by the body.
//   The inner loop walks a pointer:  sth v,0(p) / sthu v+1,2(p) / p += 2,
//   which is `*p = a; *++p = b; ++p;` rather than indexed stores.
//   The outer tail uses INDEXED stores (sthx) off the running index, so
//   those stay written as out[o].

typedef unsigned short u16;

void BuildGridStripIndices(int, int, int width, int height, u16* out)
{
    int limit = width - 1;
    int o = 0;
    int base = 0;

    if (limit <= 0)
        return;

    do
    {
        if (height > 0)
        {
            u16* p = out + o;
            int v = base;
            int c = height;
            do
            {
                *p = (u16)v;
                o += 2;
                *++p = (u16)(v + 1);
                v += width;
                ++p;
            } while (--c);
        }

        // The target computes base+1 into its own register BEFORE either
        // store, uses it for the second store, then adopts it as the new
        // base and compares THAT against the limit.
        int next = base + 1;
        out[o] = (u16)base;
        o++;
        out[o] = (u16)next;
        o++;
        base = next;
    } while (base < limit);
}
