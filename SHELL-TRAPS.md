# Shell traps in this environment

Written 2026-08-28, after the first trap below corrupted three separate files
in a single session — twice silently.

---

## 1. Backslashes through a heredoc are not reliable

**The rule: never put a backslash inside a heredoc. Use the Write tool.**

That is stated as a prohibition rather than a mechanism on purpose, because
the mechanism could not be pinned down and the behaviour is **demonstrably
inconsistent**. What is certain is that it corrupts files.

### It bit three times in one session

**(a) A regex that silently matched nothing.**

```python
pats = [rb'[A-Za-z]:\\[A-Za-z0-9_\\. \-]{6,120}',   # windows paths
```

`\\` arrived as `\`, so `\[` became an escaped bracket and the pattern
searched for a literal `[`. It returned **zero** Windows paths from an image
that contains 64 of them. No error. No warning. The scan simply reported a
benign empty result, and the conclusion "this image has no source paths"
was one sentence away.

This is the worst kind, because the failure is invisible: **a corrupted
pattern is still a valid pattern.**

**(b) A SyntaxError that at least announced itself.**

```python
p = m.group(2).replace('\\','/')
```

arrived as

```
p=m.group(2).replace('\','/')
              SyntaxError: unterminated string literal
```

Direct proof of the stripping, and the only one of the three that failed
loudly.

**(c) Literal control bytes written into a source file.**

```python
'    if len(b) % 4 == 0 and all(b[i:i + 4] == b"\\x60\\x00\\x00\\x00"\n'
```

The heredoc reduced `\\x60` to `\x60`, Python then interpreted that as a real
byte, and the *generated* file received actual `0x60 0x00 0x00 0x00` bytes in
the middle of a string literal. The next run died with

```
SyntaxError: source code cannot contain null bytes
```

and the file could not even be repaired by an ordinary text edit, because the
`old_string` to match contained unprintable bytes. It had to be fixed by a
separate byte-level repair script.

### What testing actually showed

Two tests, same shell, same `<<'EOF'` quoting, opposite results:

```
cat <<'EOF' > f.txt
two backslashes: \\
four backslashes: \\\\
EOF
->  two backslashes: \          (HALVED)
    four backslashes: \\        (HALVED)
```

```
cat <<'EOF' > t_a.py
s = "\\"
EOF
->  s = "\\"                     (INTACT)
```

`<<'EOF'` is supposed to be fully literal — POSIX says no expansion of any
kind occurs. It is not behaving that way, and it is not behaving *consistently*
either. Somewhere between authoring the command and bash receiving it, one
level of escaping is sometimes removed.

**Because it is inconsistent, "count the backslashes carefully" is not a
workable defence.** A rule you can only follow by getting an unpredictable
transformation right every time is not a rule.

### What to do instead

| instead of | do |
|---|---|
| `python <<'EOF'` with backslashes | **Write the script to a file with the Write tool, then run it** |
| `sed`/`awk` with escapes in a heredoc | Write tool, then run |
| patching a file with an inline Python heredoc | Write the patch script as a file |
| needing a backslash in a Python string | `chr(92)`, or `os.sep`, or a raw path with `/` |
| needing specific bytes | `bytes([0x60, 0x00, 0x00, 0x00])`, never `b"\x60..."` in a heredoc |

Concrete substitutions that worked in this project:

```python
BS = chr(92)                                    # instead of '\\'
p = m.group(2).replace(BS, '/')

NOP = bytes([0x60, 0x00, 0x00, 0x00])           # instead of b"\x60\x00\x00\x00"
ZERO4 = bytes(4)                                # instead of b"\0\0\0\0"
ARCH_MAGIC = bytes([0x21, 0x3C, 0x61, 0x72,     # instead of b"!<arch>\n"
                    0x63, 0x68, 0x3E, 0x0A])
```

Windows paths in Python: use forward slashes. `C:/Users/...` works everywhere
in Python on Windows and contains no backslash at all.

### The general lesson

Trap (a) is the one worth remembering. It did not fail — it **succeeded with a
wrong answer**, and the wrong answer was "no matches", which reads exactly like
a fact about the data. A tool whose pattern has been silently corrupted reports
absence of evidence as evidence of absence.

So: after any scan that returns zero, ask whether the scan could have run
correctly and found nothing, or whether it could not have found anything at
all. Validate a search against something you already know it must find,
*before* believing a negative result. That is how (a) was caught — searching
for `sb09.ini`, which had to be there.

---

## 2. MSVC-style `/flags` are rewritten into Windows paths

Git Bash's MSYS layer treats a leading `/` as a path and helpfully converts it.

```
cl.exe /c /nologo t.cpp
```

reached the compiler as

```
cl : Command line warning D9024 : unrecognized source file type 'C:/', object file assumed
cl : Command line warning D9024 : unrecognized source file type 'C:/Program Files/Git/nologo', object file assumed
```

`/c` became `C:/` and `/nologo` became `C:/Program Files/Git/nologo`. The
compile happened to succeed anyway, which is exactly what makes it dangerous —
the flags were silently dropped rather than refused.

**Run the XDK tools from PowerShell**, or set `MSYS_NO_PATHCONV=1`.

---

## 3. Other environment facts worth knowing

- **A pipeline's exit status is the last command's.** `cmd | tail` reports
  tail's success and hides the failure. Start any Bash command whose exit
  status matters with `set -o pipefail`.
- **The Bash tool resets the working directory between calls**, and `cd` does
  not survive into `cmd //c`. Use absolute paths.
- **Windows Python does not understand `/c/...` or `/tmp`.** Use `C:/...`.
  A file written to `/tmp` in one call was gone by the next; the scratchpad
  directory is the reliable place.
- **7-Zip preserves the read-only attribute** from an archive. Extracted XDK
  files came out read-only, and `mt.exe` failed with "Access is denied" until
  they were cleared — an error that reads like a permissions problem with the
  tool rather than an attribute on the file.
