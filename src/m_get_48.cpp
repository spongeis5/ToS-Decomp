#include "types.h"

// sub_82639C68 -- read a field. 8 bytes, 12 callers.
//
//      lwz     r3,48(r3)
//      blr
//
// Another two-body row: `lwz r3,48(r3) ; blr` appears twice in a row, at
// 82639C68 and 82639C70, reading the SAME offset. Two distinct accessors on
// two distinct types that happen to compile identically, which is why the
// linker's COMDAT folding did not merge them -- /Gy folds identical COMDATs
// only when told to, and this build was not.
struct Holder48
{
    char  unk0000[48];
    void* value;
};
ASSERT_OFFSET(Holder48, value, 48);

void* GetValue48(const Holder48* h)
{
    return h->value;
}
