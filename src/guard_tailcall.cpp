// sub_82807B38 -- guarded tail call, 16 bytes, 314 CALLERS (the most-called
// unattributed leaf in the image).
//
//      lbz     r11,0(r3)
//      cmplwi  cr6,r11,5
//      bltlr   cr6                 return when the byte is < 5
//      b       0x828071C8          otherwise tail-call
//
// cmplwi is an UNSIGNED compare against a byte load, so the field is an
// unsigned char. The guard is a conditional return and the call is the
// fall-through, which means the compiler expected the call to be taken.
//
// The branch target is a relocation, so that word is masked and 3 of 4 are
// actually compared.

struct Node
{
    unsigned char state;
};

void Process(Node*);

void ProcessIfReady(Node* n)
{
    if (n->state >= 5)
        Process(n);
}
