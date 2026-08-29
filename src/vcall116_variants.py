"""Source shapes for sub_827C5198, for tools/permute.py.

Ours matches 3 of 5 words. The whole difference is which register holds the
vtable slot:

    target:  lwz r11,0(r3) ; lwz r11,64(r11) ; mtctr r11 ; bctr
    ours:    lwz r11,0(r3) ; lwz r10,64(r11) ; mtctr r10 ; bctr

The target REUSES r11 -- the vtable pointer's last use is the slot load, so
the same register is written. Ours allocates a fresh r10, which means our
formulation leaves the compiler thinking something else is live.
"""

OWNER = """
struct Owner { char pad00[116]; Target* member; };
"""

BODIES = [
    ("explicit vtable, local for the object", """
struct Target;
struct VTable { void* (*slot[17])(Target*); };
struct Target { VTable* vt; };
""" + OWNER + """
void* CallSlot16(Owner* o)
{
    Target* t = o->member;
    return t->vt->slot[16](t);
}
"""),

    ("explicit vtable, no local", """
struct Target;
struct VTable { void* (*slot[17])(Target*); };
struct Target { VTable* vt; };
""" + OWNER + """
void* CallSlot16(Owner* o)
{
    return o->member->vt->slot[16](o->member);
}
"""),

    ("vtable as a byte offset, cast at the point of call", """
struct Target { void* vt; };
""" + OWNER + """
typedef void* (*Fn)(Target*);

void* CallSlot16(Owner* o)
{
    Target* t = o->member;
    return ((Fn*)t->vt)[16](t);
}
"""),

    ("real C++ virtual call, slot 16", """
struct Base
{
    virtual void* m00();  virtual void* m01();  virtual void* m02();
    virtual void* m03();  virtual void* m04();  virtual void* m05();
    virtual void* m06();  virtual void* m07();  virtual void* m08();
    virtual void* m09();  virtual void* m10();  virtual void* m11();
    virtual void* m12();  virtual void* m13();  virtual void* m14();
    virtual void* m15();  virtual void* m16();
};

struct Owner { char pad00[116]; Base* member; };

void* CallSlot16(Owner* o)
{
    return o->member->m16();
}
"""),

    ("real C++ virtual call, no local, void* return", """
struct Base
{
    virtual void* m00();  virtual void* m01();  virtual void* m02();
    virtual void* m03();  virtual void* m04();  virtual void* m05();
    virtual void* m06();  virtual void* m07();  virtual void* m08();
    virtual void* m09();  virtual void* m10();  virtual void* m11();
    virtual void* m12();  virtual void* m13();  virtual void* m14();
    virtual void* m15();  virtual void* m16();
};

struct Owner { char pad00[116]; Base* member; };

void* CallSlot16(Owner* o)
{
    Base* b = o->member;
    return b->m16();
}
"""),

    ("virtual call returning void", """
struct Base
{
    virtual void m00();  virtual void m01();  virtual void m02();
    virtual void m03();  virtual void m04();  virtual void m05();
    virtual void m06();  virtual void m07();  virtual void m08();
    virtual void m09();  virtual void m10();  virtual void m11();
    virtual void m12();  virtual void m13();  virtual void m14();
    virtual void m15();  virtual void m16();
};

struct Owner { char pad00[116]; Base* member; };

void CallSlot16(Owner* o)
{
    o->member->m16();
}
"""),

    ("member function of Owner, virtual call on the member", """
struct Base
{
    virtual void* m00();  virtual void* m01();  virtual void* m02();
    virtual void* m03();  virtual void* m04();  virtual void* m05();
    virtual void* m06();  virtual void* m07();  virtual void* m08();
    virtual void* m09();  virtual void* m10();  virtual void* m11();
    virtual void* m12();  virtual void* m13();  virtual void* m14();
    virtual void* m15();  virtual void* m16();
};

struct Owner
{
    char  pad00[116];
    Base* member;
    void* Call();
};

void* Owner::Call()
{
    return member->m16();
}
"""),
]
