// sub_827618E8 -- compare two length-counted 16-bit strings.
//
// Read from the disassembly:
//   r3 = a, r4 = na, r5 = b, r6 = nb, r7 = a byte flag (clrlwi ...,0x18).
//   Both pointers are walked with `lhzu`, i.e. pre-increment through a
//   pointer biased by -2 before the loop.
//   r30/r31 hold the ORIGINAL na/nb; the equal case returns na0 - nb0
//   (subf r3,r31,r30) and every mismatch path returns ca - cb.
//   na is decremented with addic. immediately after the first load, so the
//   flag test that follows is on the DECREMENTED value.

typedef unsigned short u16;

int CompareCounted(const u16* a, unsigned int na,
                   const u16* b, unsigned int nb,
                   unsigned char exact)
{
    if (na == 0)
        return -(int)nb;

    unsigned int na0 = na;
    unsigned int nb0 = nb;

    const u16* pa = a - 1;
    const u16* pb = b - 1;

    // signed: the target uses cmpwi/cmpw, not cmplwi/cmplw
    int ca, cb;
    for (;;)
    {
        ca = (int)*++pa;
        --na;
        cb = (int)*++pb;
        if (na == 0)
            break;
        if (ca == 0)
            break;
        if (ca != cb)
            return ca - cb;
        --nb;
        if (nb == 0)
            break;
    }

    if (ca != cb)
        return ca - cb;
    if (nb == 0)
        return ca - cb;
    if (exact && na == 0)
        return ca - cb;

    return (int)na0 - (int)nb0;
}
