"""Source shapes for sub_82807B38, for tools/permute.py.

Ours is byte-identical for all four of the target's instructions and then
emits a FIFTH -- a `blr` after an unconditional branch, which is unreachable:

    target:  lbz / cmplwi / bltlr / b Process
    ours:    lbz / cmplwi / bltlr / b Process / blr

So the question is not what the function computes -- that is already right --
but what makes MSVC treat the call as a true tail call and stop, rather than
branching and then appending an epilogue it can never reach.
"""

DECL_VOID = """
struct Node { unsigned char state; };
void Process(Node*);
"""

DECL_INT = """
struct Node { unsigned char state; };
int Process(Node*);
"""

BODIES = [
    ("if (>=5) call;  void", DECL_VOID + """
void ProcessIfReady(Node* n)
{
    if (n->state >= 5)
        Process(n);
}
"""),

    ("early return, then call", DECL_VOID + """
void ProcessIfReady(Node* n)
{
    if (n->state < 5)
        return;
    Process(n);
}
"""),

    ("`return Process(n)` from a void function", DECL_VOID + """
void ProcessIfReady(Node* n)
{
    if (n->state < 5)
        return;
    return Process(n);
}
"""),

    ("void wrapper returning an int callee", DECL_INT + """
void ProcessIfReady(Node* n)
{
    if (n->state < 5)
        return;
    Process(n);
}
"""),

    ("int function forwarding the result", DECL_INT + """
int ProcessIfReady(Node* n)
{
    if (n->state < 5)
        return 0;
    return Process(n);
}
"""),

    ("int function, uninitialised early exit", DECL_INT + """
int ProcessIfReady(Node* n)
{
    if (n->state >= 5)
        return Process(n);
    int u;
    return u;
}
"""),

    ("ternary", DECL_VOID + """
void ProcessIfReady(Node* n)
{
    n->state < 5 ? (void)0 : Process(n);
}
"""),

    ("signed char field", """
struct Node { char state; };
void Process(Node*);

void ProcessIfReady(Node* n)
{
    if ((unsigned char)n->state >= 5)
        Process(n);
}
"""),

    ("field is a bitfield-free unsigned char, compare > 4", DECL_VOID + """
void ProcessIfReady(Node* n)
{
    if (n->state > 4)
        Process(n);
}
"""),
]
