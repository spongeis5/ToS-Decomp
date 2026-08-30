"""What kind of work a matched function represents. One definition.

Three categories, and the difference between them is the difference between
numbers that mean very different things:

  handwritten  read off the disassembly. The only one that says any of this
               game has been UNDERSTOOD.
  generated    a single expression each -- a constant return, a field
               accessor, a vtable forwarder -- written by script from the
               instructions themselves. Real matches; a link needs every one.
  upstream     third-party source, not ours. Mostly OBTAINED: libogg 1.1.3
               and libvorbis 1.2.0, the release pair decided by measurement
               in FINDINGS §8a. A few are RECONSTRUCTED -- the image contains
               a modified build of an upstream file and the modification had
               to be read out of the disassembly (see
               thirdparty/ogg_vorbis_fmod/README.md). Those sit here rather
               than under `handwritten` deliberately: their bodies are
               upstream text, so counting them as "read off the disassembly"
               would inflate the one figure that claims this game has been
               understood. Erring toward the smaller claim is the point.
               Either way these reproduce the image exactly -- the compiler
               does not care where the source came from.

`report.py`, `readme_stats.py`, `matched_table.py` and `objdiff_export.py`
each had their own copy of the generated-prefix tuple. Four copies of one
rule is how the split silently stops agreeing, which is the drift this
project has paid for more than any other; adding a third category to four
places separately would have been the fifth time.

Both tests are exact rather than a judgement call: a path under `thirdparty/`
is not ours, and a filename with one of three generated prefixes was written
by `gen_typeids.py`, `gen_accessors.py` or their siblings.
"""

from pathlib import PurePosixPath

GENERATED_PREFIX = ("vt_typeid_", "vt_const_", "vt_acc_")
UPSTREAM_ROOT = "thirdparty"

HANDWRITTEN = "handwritten"
GENERATED = "generated"
UPSTREAM = "upstream"

# id -> the name a reader sees. Order is the order they are reported in.
CATEGORIES = [
    (HANDWRITTEN, "Hand-written from disassembly"),
    (GENERATED, "Generated from encodings"),
    (UPSTREAM, "Upstream third-party source"),
]


def category(src):
    """-> 'handwritten' | 'generated' | 'upstream' for a manifest source."""
    p = PurePosixPath(str(src).replace("\\", "/"))
    if p.parts and p.parts[0] == UPSTREAM_ROOT:
        return UPSTREAM
    if p.name.startswith(GENERATED_PREFIX):
        return GENERATED
    return HANDWRITTEN


def is_generated(src):
    return category(src) == GENERATED


def is_upstream(src):
    return category(src) == UPSTREAM
