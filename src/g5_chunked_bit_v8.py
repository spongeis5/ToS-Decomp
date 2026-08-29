"""Round 8 for sub_8216C240 -- last attempt at the single `or` operand order.

37 of 38 at the exact size and at 44 of the 72 flag combinations.  Round 7
showed the operator's own spelling does not carry it (six read-order shapes,
all 37).  This round varies the TYPES instead, and puts the operation behind
a helper so the two operands arrive as parameters in a chosen order.
"""

H = '#include "types.h"\n\n'


def types(elem):
    return """
struct Block
{
    /* 0x00 */ %s*   words;
    /* 0x04 */ Block* next;
};

struct ChunkedSet
{
    /* 0x00 */ char   unk0000[36];
    /* 0x24 */ s32    perBlock;
    /* 0x28 */ char   unk0028[4];
    /* 0x2C */ Block* head;
};
ASSERT_OFFSET(ChunkedSet, perBlock, 36);
ASSERT_OFFSET(ChunkedSet, head, 44);
""" % elem


WALK = ("        s32 q = w / s->perBlock;\n"
        "        s32 r = w - q * s->perBlock;\n"
        "        Block* p = s->head;\n"
        "        for (s32 i = q; i != 0; i--)\n"
        "            p = p->next;\n")


def mk(elem, bittype, seton, clear, helpers=""):
    return (H + types(elem) + helpers +
            "\nvoid ChunkedSetBit(ChunkedSet* s, s32 index, bool on)\n{\n"
            "    s32 w = index >> 5;\n"
            "    %s bit = 1%s << (index & 31);\n\n"
            "    if (on)\n    {\n" % (bittype, "u" if bittype == "u32" else "") +
            WALK + seton + "    }\n    else\n    {\n" + WALK + clear +
            "    }\n}\n")


SET_U = "        p->words[r] |= bit;\n"
CLR_U = "        p->words[r] &= ~bit;\n"

BODIES = [
    ("u32 words, s32 bit", mk("u32", "s32",
                              "        p->words[r] |= (u32)bit;\n",
                              "        p->words[r] &= ~(u32)bit;\n")),
    ("s32 words, s32 bit", mk("s32", "s32",
                              "        p->words[r] |= bit;\n",
                              "        p->words[r] &= ~bit;\n")),
    ("s32 words, u32 bit", mk("s32", "u32",
                              "        p->words[r] |= (s32)bit;\n",
                              "        p->words[r] &= ~(s32)bit;\n")),
    ("helper Or(mask, word)", mk("u32", "u32",
                                 "        p->words[r] = Or(bit, p->words[r]);\n",
                                 CLR_U,
                                 "\nstatic u32 Or(u32 m, u32 v) { return m | v; }\n")),
    ("helper Or(word, mask)", mk("u32", "u32",
                                 "        p->words[r] = Or(p->words[r], bit);\n",
                                 CLR_U,
                                 "\nstatic u32 Or(u32 v, u32 m) { return v | m; }\n")),
    ("helper taking the element address", mk("u32", "u32",
                                             "        OrInto(&p->words[r], bit);\n",
                                             CLR_U,
                                             "\nstatic void OrInto(u32* e, u32 m) { *e = m | *e; }\n")),
    ("set arm via ~(~v & ~m) is not written; xor-free control", mk(
        "u32", "u32",
        "        p->words[r] = p->words[r] | bit;\n", CLR_U)),
    ("mask negated once into a local", mk(
        "u32", "u32", SET_U,
        "        u32 keep = ~bit;\n        p->words[r] &= keep;\n")),
]
