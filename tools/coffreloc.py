"""COFF functions WITH their relocation records, for PowerPC objects.

`libmatch.coff_functions` returns a byte mask that blanks every relocated
word. That is right for scanning a whole image cheaply, but it is far too
coarse for verifying a function: masking a 4-byte word throws away the
opcode and the register fields as well as the address, so

    bl   <symbol>          and          bl   <other symbol>
    stw  r11,0(r3)         and          stw  r10,0(r3)     (if relocated)

compare equal. This returns the relocations themselves, so only the IMMEDIATE
FIELD a relocation actually patches is excused, and the rest of the word is
compared like any other.

    functions_with_relocs(blob) -> [(name, code, [Reloc, ...])]

Reloc.off is relative to the START OF THE FUNCTION, not the section.
"""

import struct
from collections import namedtuple

IMAGE_FILE_MACHINE_POWERPCBE = 0x01F2
ZERO4 = bytes(4)

Reloc = namedtuple("Reloc", "off type sym")

# IMAGE_REL_PPC_*
ABSOLUTE = 0x00
ADDR64   = 0x01
ADDR32   = 0x02
ADDR24   = 0x03
ADDR16   = 0x04
ADDR14   = 0x05
REL24    = 0x06
REL14    = 0x07
TOCREL16 = 0x0A
TOCREL14 = 0x0F
ADDR32NB = 0x10
SECREL   = 0x11
SECTION  = 0x12
SECREL16 = 0x15
REFHI    = 0x16
REFLO    = 0x17
PAIR     = 0x18
SECRELLO = 0x19
GPREL    = 0x1A

TYPE_NAME = {
    ABSOLUTE: "ABSOLUTE", ADDR64: "ADDR64", ADDR32: "ADDR32",
    ADDR24: "ADDR24", ADDR16: "ADDR16", ADDR14: "ADDR14",
    REL24: "REL24", REL14: "REL14", TOCREL16: "TOCREL16",
    TOCREL14: "TOCREL14", ADDR32NB: "ADDR32NB", SECREL: "SECREL",
    SECTION: "SECTION", SECREL16: "SECREL16", REFHI: "REFHI",
    REFLO: "REFLO", PAIR: "PAIR", SECRELLO: "SECRELLO", GPREL: "GPREL",
}

# Which bits of the instruction word the relocation patches. Everything
# outside these bits -- the opcode and the register fields -- is compared
# like any other word, which is the whole point of resolving relocations
# instead of masking them.
#
# MEASURED against this XDK rather than assumed. A `lis`/`addi` pair
# addressing an extern compiles to:
#
#     +0x00  ADDR32NB + SECTION   lis  rX,<high half>
#     +0x04  SECREL   + SECTION   addi rY,rX,<low half>
#
# so on PowerPC, ADDR32NB and SECREL land on INSTRUCTIONS and patch only the
# 16-bit immediate. Treating ADDR32NB as a whole-word data patch -- the first
# version of this table did -- copies the retail register fields in as well,
# and a function with the wrong destination register then verifies clean.
PATCH_BITS = {
    REL24:    0x03FFFFFC,
    ADDR24:   0x03FFFFFC,
    REL14:    0x0000FFFC,
    ADDR14:   0x0000FFFC,
    ADDR16:   0x0000FFFF,
    TOCREL16: 0x0000FFFF,
    SECREL16: 0x0000FFFF,
    REFHI:    0x0000FFFF,
    REFLO:    0x0000FFFF,
    SECRELLO: 0x0000FFFF,
    GPREL:    0x0000FFFF,
    ADDR32NB: 0x0000FFFF,
    SECREL:   0x0000FFFF,
    TOCREL14: 0x0000FFFF,
}

# TOCREL14 is the `__declspec(thread)` case, and its width was MEASURED, not
# read off the name. A probe with members at offsets 0,1,2,4,8 compiled to
#
#     li   r11,0            <- TOCREL14 on the symbol, immediate ALWAYS zero
#     lwz  r10,0(r13)
#     addi r11,r11,1        <- the member offset, in a SEPARATE instruction
#
# five times out of five. The compiler never folds a member offset into the
# relocated immediate, so every bit of that D-form immediate is chosen by the
# linker (it is the TLS slot offset) and taking all sixteen from the retail
# word cannot mask anything the source decided.
#
# That reasoning depends entirely on the placeholder being zero, so it is
# ENFORCED rather than assumed: a non-zero field here means the compiler did
# fold something in, the justification above is void, and build.py must
# refuse instead of copying retail bits over a source-determined value.
PLACEHOLDER_MUST_BE_ZERO = (TOCREL14,)

# Records that carry an operand for a NEIGHBOURING relocation rather than
# patching a site of their own. SECTION appears on the same offset as the
# ADDR32NB/SECREL above and names @comp.id, not the referenced symbol.
COMPANION = (PAIR, SECTION)

# True whole-word patches. None occur inside code sections, so this is empty
# for our purposes and kept only so the distinction stays explicit.
WHOLE_WORD = (ADDR32, ADDR64)


def type_name(t):
    return TYPE_NAME.get(t, "type_%#x" % t)


def functions_with_relocs(blob):
    if len(blob) < 20:
        return []
    mach, nsec, _ts, psym, nsym, osz, _ch = struct.unpack_from("<HHIIIHH", blob, 0)
    if mach != IMAGE_FILE_MACHINE_POWERPCBE or nsym == 0:
        return []

    sh = 20 + osz
    secs = []
    for i in range(nsec):
        b = sh + i * 40
        if b + 40 > len(blob):
            return []
        _vsize, _va, rawsz, rawptr = struct.unpack_from("<IIII", blob, b + 8)
        relptr = struct.unpack_from("<I", blob, b + 24)[0]
        nrel = struct.unpack_from("<H", blob, b + 32)[0]
        chars = struct.unpack_from("<I", blob, b + 36)[0]
        secs.append(dict(idx=i + 1, size=rawsz, ptr=rawptr, relptr=relptr,
                         nrel=nrel, is_code=bool(chars & 0x20000000)))

    strtab = psym + nsym * 18
    syms, i = [], 0
    while i < nsym:
        o = psym + i * 18
        if o + 18 > len(blob):
            break
        raw = blob[o:o + 8]
        if raw[:4] == ZERO4:
            off = struct.unpack_from("<I", blob, o + 4)[0]
            e = blob.find(bytes(1), strtab + off)
            name = blob[strtab + off:e].decode("latin1") if e > 0 else "?"
        else:
            name = raw.rstrip(bytes(1)).decode("latin1")
        value, secnum, _typ, cls, naux = struct.unpack_from("<IhHBB", blob, o + 8)
        syms.append(dict(name=name, value=value, sec=secnum, cls=cls))
        for _k in range(naux):
            syms.append(None)          # keep indices aligned with the table
        i += 1 + naux

    out = []
    for s in secs:
        if not s["is_code"] or not s["size"] or not s["ptr"]:
            continue
        code = blob[s["ptr"]:s["ptr"] + s["size"]]
        if len(code) != s["size"]:
            continue

        rels = []
        for r in range(s["nrel"]):
            ro = s["relptr"] + r * 10
            if ro + 10 > len(blob):
                break
            va, symidx, rtype = struct.unpack_from("<IIH", blob, ro)
            sname = "?"
            if 0 <= symidx < len(syms) and syms[symidx]:
                sname = syms[symidx]["name"]
            rels.append((va, rtype, sname))

        fs = sorted([y for y in syms
                     if y and y["sec"] == s["idx"] and y["cls"] in (2, 3)
                     and not y["name"].startswith(".")],
                    key=lambda y: y["value"])
        for n, f in enumerate(fs):
            a = f["value"]
            b_ = fs[n + 1]["value"] if n + 1 < len(fs) else len(code)
            if b_ <= a:
                continue
            mine = [Reloc(va - a, rtype, sname)
                    for va, rtype, sname in rels if a <= va < b_]
            out.append((f["name"], code[a:b_], mine))
    return out


def sign_extend(value, bits):
    m = 1 << (bits - 1)
    return (value ^ m) - m


def solve_address(rtype, target_word, va):
    """The address a retail instruction's patched field refers to.

    Returns (kind, value) or None when the type is not one we solve.
      'abs'  a whole address
      'hi'   the high half of an address, needing its REFLO partner
      'lo'   the low half, signed
    """
    if rtype in (REL24, ADDR24):
        disp = sign_extend(target_word & 0x03FFFFFC, 26)
        if rtype == REL24:
            return ("abs", (va + disp) & 0xFFFFFFFF)
        return ("abs", disp & 0xFFFFFFFF)
    if rtype in (REFHI, ADDR32NB):
        # The high half, carrying the @ha carry adjustment the low half needs.
        return ("hi", (target_word & 0xFFFF) << 16)
    if rtype in (REFLO, ADDR16, SECRELLO, SECREL):
        return ("lo", sign_extend(target_word & 0xFFFF, 16))
    if rtype == TOCREL14:
        # Not an address at all: the linker's TLS slot offset, read through
        # r13. Reported as its own kind so it is not printed as though some
        # location had been recovered.
        return ("tls", sign_extend(target_word & 0xFFFF, 16))
    return None
