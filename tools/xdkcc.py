"""One place that knows how to invoke the XDK compiler.

There were four copies of this: match.py, permute.py, flagsweep.py and
build.py each built their own environment dict and their own argv. They drifted
the moment `include/` was added to the search path -- build.py got it, the
other three did not, and seven functions that still compiled and still
reproduced under build.py started reporting

    fatal error C1083: Cannot open include file: 'types.h'

under match.py. Nothing was wrong with the sources or the matches; three
harnesses were simply compiling differently from the one that verifies.

So the invocation lives here, once.

Notes that are load-bearing:

  * cl.exe is invoked through subprocess DIRECTLY, never through a shell. Git
    Bash rewrites MSVC-style `/c` and `/nologo` into Windows paths and the
    flags are then silently dropped rather than refused (SHELL-TRAPS.md 2).
  * `include/` comes BEFORE the XDK's own include directory, so the project's
    headers win.
  * The compiler's diagnostics are returned in full. cl prints the source
    filename as line 1, so a caller that shows only the first line of output
    reports the filename and hides the error -- which is how a working
    ASSERT_OFFSET once looked like a byte mismatch.
"""

import hashlib
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
XDK = ROOT / "SDKFiles/xdk/XDK"
BIN = XDK / "bin/win32"
CL = BIN / "cl.exe"
XDK_INCLUDE = XDK / "include/xbox"
PROJECT_INCLUDE = ROOT / "include"

DEFAULT_FLAGS = ["/c", "/nologo", "/O2", "/Gy", "/GS-", "/fp:fast"]


def env(workdir):
    import os
    return {
        "PATH": str(BIN.resolve()),
        "INCLUDE": os.pathsep.join([str(PROJECT_INCLUDE.resolve()),
                                    str(XDK_INCLUDE.resolve())]),
        "SystemRoot": "C:/Windows",
        "TEMP": str(Path(workdir).resolve()),
    }


def diagnostics(text):
    """The lines of cl output that are actually diagnostics.

    cl prints the source filename first, so `output.splitlines()[0]` is never
    the error.
    """
    lines = [l.strip() for l in (text or "").splitlines() if l.strip()]
    diag = [l for l in lines if "error" in l.lower() or "warning" in l.lower()]
    return diag or lines


# One entry per (source CONTENT, flags) actually compiled in this process.
#
# The manifest has one row per FUNCTION, not per file, and a single .cpp can
# define many -- 194 of the image's functions are a one-line constant return,
# and putting each in its own file to keep one-compile-per-row would mean 194
# files and 194 invocations of cl per verify.
#
# Keyed on the source's CONTENT, never its path or mtime: a cache keyed on a
# path serves the previous text after an edit, which is precisely the
# stale-object failure the `obj.unlink()` below exists to prevent, moved one
# level up where it is harder to see. Keyed on content it cannot: editing the
# file changes the key. In-process only, with no on-disk half, so it cannot
# outlive the run that built it.
_MEMO = {}
_MEMO_STATS = [0, 0]          # [hits, misses]


def cache_stats():
    """(hits, misses) for this process. Print it rather than assuming it."""
    return tuple(_MEMO_STATS)


LOCK = ROOT / "build/.negative_controls.lock"


def _blocked_by_negative_controls():
    """Is verify.py currently holding a source file corrupted on purpose?

    verify.py's negative controls edit a REAL file in src/ -- a struct offset,
    an ASSERT_SIZE, a manifest address -- run the build, require it to fail,
    and restore. That window is short but it is not atomic, and while it is
    open any other process compiling from src/ reads text that is wrong by
    design and gets an answer that looks like a mismatch.

    That is not hypothetical: five agents were compiling through this
    function while a verify ran, and the build came back with an unlink race
    and a stale-table failure that had nothing to do with either.

    So verify.py writes its PID here for the duration and every compile in
    every other process refuses while it exists. The refusal is loud and
    names the cause, which is the whole point -- a wrong answer here would
    be recorded in a manifest and believed later.
    """
    if not LOCK.exists():
        return None
    try:
        holder = int(LOCK.read_text().strip() or "0")
    except (OSError, ValueError):
        return None
    if holder == os.getpid():
        return None                     # verify.py itself, doing the test
    # ...and its CHILDREN. Every negative control runs build.py as a
    # subprocess, which has a different pid, so a pid-only check refused the
    # very builds the controls exist to run. They then failed for the wrong
    # reason -- a refusal message instead of the C2118 or hash mismatch each
    # one looks for -- and four controls reported NOT CAUGHT. The controls
    # caught the regression, which is the entire argument for having them.
    #
    # verify.py exports its pid, and the environment is inherited by
    # everything it spawns, so descendants pass and unrelated processes do
    # not.
    if os.environ.get("TOS_VERIFY_LOCK", "") == str(holder):
        return None
    # A DEAD holder holds nothing. verify.py releases the lock in the normal
    # path, but it was killed by a command timeout while holding it and every
    # tool in the project then refused to compile -- permanently, and with a
    # message confidently explaining that a verify was running when none was.
    # A guard that cannot be cleared is worse than the race it prevents.
    if not _pid_alive(holder):
        # Clearing the lock is only half of it. The holder died WHILE a
        # source file was corrupted, and that file is still corrupted on
        # disk; releasing the lock without putting it back just lets everyone
        # compile the wrong text, which is the exact failure the lock exists
        # to prevent. verify.py restores from this sentinel at startup, but
        # nothing else did -- so table_index.cpp sat with a deliberately
        # wrong ASSERT_SIZE and objdiff_export reported it as COMPILE FAILED.
        _restore_sentinel()
        try:
            LOCK.unlink()
        except OSError:
            pass
        return None
    return holder


SENTINEL = ROOT / "build/.verify_restore.json"


def _restore_sentinel():
    """Put back whatever a killed verify.py left corrupted. -> [paths]."""
    if not SENTINEL.exists():
        return []
    import json
    try:
        saved = json.loads(SENTINEL.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return []
    done = []
    for rel, text in saved.items():
        try:
            (ROOT / rel).write_text(text, encoding="utf-8")
            done.append(rel)
        except OSError:
            pass
    if done:
        sys.stderr.write(
            "xdkcc: a killed tools/verify.py left %d file(s) corrupted by a\n"
            "negative control; restored %s before compiling.\n"
            % (len(done), ", ".join(done)))
    try:
        SENTINEL.unlink()
    except OSError:
        pass
    return done


def _pid_alive(pid):
    """Is `pid` a live process? On Windows, ask the OS rather than signal 0."""
    if pid <= 0:
        return False
    if os.name != "nt":
        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False
    try:
        out = subprocess.run(
            ["tasklist", "/FI", "PID eq %d" % pid, "/NH"],
            capture_output=True, text=True, timeout=10).stdout
    except Exception:
        return True          # cannot tell: assume alive, i.e. keep refusing
    return str(pid) in out


def compile_obj(src, obj, flags=None, workdir=None):
    """Compile `src` to `obj`. -> (object bytes, None) or (None, diagnostics).

    `obj` is removed first, so a stale object from a previous run can never be
    mistaken for a successful compile.
    """
    holder = _blocked_by_negative_controls()
    if holder is not None:
        return None, (
            "REFUSING TO COMPILE: tools/verify.py (pid %d) is running its\n"
            "negative controls, which deliberately corrupt a source file in\n"
            "src/ and restore it a moment later. Anything compiled now may\n"
            "read that corrupted text, and the result would look like an\n"
            "ordinary mismatch rather than a race.\n"
            "Wait for verify.py to finish and try again." % holder)
    src = Path(src)
    obj = Path(obj)
    workdir = Path(workdir) if workdir else obj.parent
    workdir.mkdir(parents=True, exist_ok=True)
    obj.parent.mkdir(parents=True, exist_ok=True)
    if obj.exists():
        obj.unlink()

    use = list(flags if flags is not None else DEFAULT_FLAGS)
    try:
        key = (hashlib.sha256(src.read_bytes()).hexdigest(), tuple(use))
    except OSError:
        key = None              # unreadable: fall through and let cl say so
    if key is not None and key in _MEMO:
        _MEMO_STATS[0] += 1
        blob, err = _MEMO[key]
        if blob is not None:
            # Write it out anyway: callers are entitled to the .obj on disk.
            obj.write_bytes(blob)
        return blob, err

    cmd = ([str(CL.resolve())] + use
           + ["/Fo" + str(obj.resolve()), str(src.resolve())])
    r = subprocess.run(cmd, capture_output=True, text=True,
                       cwd=str(workdir.resolve()), env=env(workdir))
    out = r.stdout + r.stderr
    if r.returncode != 0 or not obj.exists():
        result = (None, "\n".join(diagnostics(out)))
    else:
        result = (obj.read_bytes(), None)
    _MEMO_STATS[1] += 1
    if key is not None:
        _MEMO[key] = result
    return result


def self_test():
    """Compile a file that includes a project header. Both arms must behave.

    This exists because the failure it guards against was invisible: the
    sources were fine, the matches were fine, and only the harness differed.
    """
    work = ROOT / "build/xdkcc_selftest"
    work.mkdir(parents=True, exist_ok=True)
    good = work / "good.cpp"
    good.write_text('#include "types.h"\n'
                    'struct S { char a[8]; s32 v; };\n'
                    'ASSERT_OFFSET(S, v, 8);\n'
                    'int f(S* s) { return s->v; }\n')
    bad = work / "bad.cpp"
    bad.write_text('#include "types.h"\n'
                   'struct S { char a[4]; s32 v; };\n'
                   'ASSERT_OFFSET(S, v, 8);\n'
                   'int f(S* s) { return s->v; }\n')

    ok = True
    blob, err = compile_obj(good, work / "good.obj")
    print("  project header resolves      %s"
          % ("yes" if blob else "NO -- " + (err or "").splitlines()[0]))
    ok &= blob is not None
    blob, err = compile_obj(bad, work / "bad.obj")
    caught = blob is None and "C2118" in (err or "")
    print("  wrong offset is a hard error %s" % ("yes" if caught else "NO"))
    ok &= caught
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(self_test())
