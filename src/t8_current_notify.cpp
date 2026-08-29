#include "types.h"

// sub_826043B0 -- notify the current scene, but only if the argument belongs
// to it. 60 B, 6 callers.
//
//      lis     r11,-32093 ; mr r4,r3 ; cmplwi cr6,r3,0
//      lwz     r3,20976(r11)             -> 82A351F0, a global POINTER
//      beq-    cr6,-> li 0
//      lwz     r11,8(r4) ; cmplw cr6,r11,r3 ; li r11,1 ; beq- -> test
//      li      r11,0
//      clrlwi  r11,r11,24 ; cmplwi cr6,r11,0 ; beqlr cr6
//      b       826776C8
//      blr                               <-- unreachable, MSVC's tail-call tail
//
// A single `lis` feeding a `lwz` with a signed displacement is a global
// pointer VARIABLE read, not the address of a global object.
//
// `li 1 / li 0 / clrlwi / cmplwi / beqlr` is a materialised-then-masked bool:
// a bare `if (a && b)` branches out of each term and never builds a value, so
// the predicate is an inlined bool-returning helper. The TRUE constant is
// materialised before the second branch and the FALSE after it, which is the
// `&&` short-circuit spelling.
//
// The global is loaded ONCE and serves both the comparison and the call's
// first argument, so the helper and the call read the same expression.

struct Scene;

struct Node
{
    /* 0x00 */ char   unk0000[0x08];
    /* 0x08 */ Scene* scene;
};
ASSERT_OFFSET(Node, scene, 0x08);

extern Scene* g_currentScene;

void SceneNotify(Scene* s, Node* n);

static bool BelongsToCurrent(Node* n)
{
    return n != 0 && n->scene == g_currentScene;
}

void NotifyIfCurrent(Node* n)
{
    if (BelongsToCurrent(n))
        SceneNotify(g_currentScene, n);
}
