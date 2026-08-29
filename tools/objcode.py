"""Extract and disassemble the PowerPC code a COFF object contains.

This is one half of the matching loop: compile a candidate .cpp with the
XDK's cl.exe, pull the bytes it emitted for a function, and put them beside
the same function's bytes from the retail image.

    python tools/objcode.py foo.obj              every function in the object
    python tools/objcode.py foo.obj --sym ?f@@   one symbol

Capstone does not know Xenon's VMX128 extension, so a word it cannot decode
is printed as a raw word with a marker rather than skipped -- a disassembly
that silently drops instructions is a disassembly of a different function.
"""

import struct
import sys
from pathlib import Path

try:
    from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN
except ImportError:
    print("capstone is required: python -m pip install capstone", file=sys.stderr)
    raise

IMAGE_MACHINE = {0x01F0: "POWERPC", 0x01F1: "POWERPCFP", 0x01F2: "POWERPCBE"}


def read_coff(path):
    d = Path(path).read_bytes()
    mach, nsec, ts, psym, nsym, osz, ch = struct.unpack_from("<HHIIIHH", d, 0)
    if mach not in IMAGE_MACHINE:
        raise ValueError(f"{path}: machine {mach:04X} is not PowerPC")
    sh = 20 + osz
    secs = []
    for i in range(nsec):
        b = sh + i * 40
        name = d[b : b + 8].rstrip(b"\0").decode("latin1")
        vsize, va, rawsz, rawptr = struct.unpack_from("<IIII", d, b + 8)
        chars = struct.unpack_from("<I", d, b + 36)[0]
        secs.append(dict(idx=i + 1, name=name, size=rawsz, ptr=rawptr, chars=chars))

    strtab = psym + nsym * 18
    syms = []
    i = 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o : o + 8]
        if raw[:4] == b"\0\0\0\0":
            off = struct.unpack_from("<I", d, o + 4)[0]
            end = d.index(b"\0", strtab + off)
            name = d[strtab + off : end].decode("latin1")
        else:
            name = raw.rstrip(b"\0").decode("latin1")
        value, secnum, typ, cls, naux = struct.unpack_from("<IhHBB", d, o + 8)
        syms.append(dict(name=name, value=value, sec=secnum, cls=cls))
        i += 1 + naux
    return d, mach, secs, syms


def disasm(code, base=0):
    md = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
    md.skipdata = False
    out = []
    pos = 0
    while pos + 4 <= len(code):
        chunk = code[pos : pos + 4]
        got = list(md.disasm(chunk, base + pos))
        if got:
            ins = got[0]
            out.append((base + pos, chunk, f"{ins.mnemonic} {ins.op_str}".strip()))
        else:
            word = struct.unpack(">I", chunk)[0]
            out.append((base + pos, chunk,
                        f".long 0x{word:08X}   ; UNDECODED (opcode {word >> 26}) "
                        f"-- capstone has no VMX128"))
        pos += 4
    if pos != len(code):
        out.append((base + pos, code[pos:], f".byte  ; {len(code) - pos} trailing byte(s)"))
    return out


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    path = argv[1]
    want = argv[argv.index("--sym") + 1] if "--sym" in argv else None

    d, mach, secs, syms = read_coff(path)
    print(f"{path}: machine {mach:04X} ({IMAGE_MACHINE[mach]}), "
          f"{len(secs)} section(s), {len(syms)} symbol(s)")

    text_secs = {s["idx"]: s for s in secs if s["chars"] & 0x20000000}
    if not text_secs:
        print("  no executable section")
        return 1

    # Function symbols: class 2 (external) or 3 (static), in a code section,
    # excluding section symbols themselves.
    funcs = [s for s in syms
             if s["sec"] in text_secs and s["cls"] in (2, 3)
             and not s["name"].startswith(".")]
    if want:
        funcs = [s for s in funcs if want in s["name"]]
    print(f"  {len(funcs)} function symbol(s)"
          + (f" matching {want!r}" if want else ""))
    print()

    for s in sorted(funcs, key=lambda x: (x["sec"], x["value"])):
        sec = text_secs[s["sec"]]
        # A COFF object has one function per COMDAT section here, so the
        # extent is the section; with several symbols in one section the
        # next symbol bounds it.
        others = sorted(o["value"] for o in funcs
                        if o["sec"] == s["sec"] and o["value"] > s["value"])
        end = others[0] if others else sec["size"]
        code = d[sec["ptr"] + s["value"] : sec["ptr"] + end]
        print(f"  {s['name']}   section {sec['name']}  "
              f"+{s['value']:#x}..{end:#x}  {len(code)} byte(s)")
        for addr, raw, text in disasm(code, s["value"]):
            print(f"    {addr:08X}  {raw.hex():<8}  {text}")
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
