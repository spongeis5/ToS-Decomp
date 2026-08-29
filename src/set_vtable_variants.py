"""Source shapes for sub_827007E8, for tools/permute.py.

3 of 4 words, and the difference is only which register carries the address:

    target:  lis r11,hi ; addi r11,r11,lo ; stw r11,0(r3)
    ours:    lis r11,hi ; addi r10,r11,lo ; stw r10,0(r3)

Worth noting that sub_82600BD8 -- which MATCHED -- has the opposite shape,
`lis r11 ; addi r10,r11`, so this is not a fixed habit of the compiler and
something about the expression decides it.
"""

BODIES = [
    ("address-of an extern object", """
struct VTable;
extern const VTable kVTable;
struct Object { const VTable* vt; };

void SetVTable(Object* o) { o->vt = &kVTable; }
"""),

    ("extern array, decays to a pointer", """
extern void* const kVTable[];
struct Object { void* const* vt; };

void SetVTable(Object* o) { o->vt = kVTable; }
"""),

    ("extern pointer-sized symbol, void*", """
extern int kVTable;
struct Object { void* vt; };

void SetVTable(Object* o) { o->vt = &kVTable; }
"""),

    ("member function", """
struct VTable;
extern const VTable kVTable;
struct Object
{
    const VTable* vt;
    void Init();
};

void Object::Init() { vt = &kVTable; }
"""),

    ("store through an int*", """
extern int kVTable;

void SetVTable(int** o) { *o = &kVTable; }
"""),

    ("first element of an extern array of functions", """
typedef void (*Fn)();
extern Fn const kVTable[1];
struct Object { const Fn* vt; };

void SetVTable(Object* o) { o->vt = &kVTable[0]; }
"""),

    ("cast from a function symbol", """
extern void SomeFunction();
struct Object { void* vt; };

void SetVTable(Object* o) { o->vt = (void*)&SomeFunction; }
"""),
]
