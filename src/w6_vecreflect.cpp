#include "types.h"
#include <math.h>

// sub_822542D8 -- project b onto the plane whose normal is n, with a
// degenerate-normal guard. 196 B, 4 callers.
//
//   dot1 = a.x*n.x + a.y*n.y + a.z*n.z        (two fmadds)
//   if (fabs(dot1) < 0.01f) return            -- fabs then fcmpu/bltlr
//   dot2 = b.x*n.x + b.y*n.y + b.z*n.z
//   b.y -= n.y*dot2 ; b.x -= n.x*dot2 ; b.z -= n.z*dot2
//
// The store order is y, x, z -- source order, not address order.  The
// normal's components are spilled to the red zone and re-read per use.
// 0.009999999776482582f lives at 82067C40.

struct Vec3f
{
    float x, y, z;
};

void Project(Vec3f* a, Vec3f* b, const Vec3f* n)
{
    float dot1 = a->x * n->x + a->y * n->y + a->z * n->z;
    if (fabs(dot1) < 0.009999999776482582f)
        return;
    float dot2 = b->x * n->x + b->y * n->y + b->z * n->z;
    b->y = b->y - n->y * dot2;
    b->x = b->x - n->x * dot2;
    b->z = b->z - n->z * dot2;
}

// NEAR-MISS. SIZE DIFFERS 124 vs 196 -- argument passing shape wrong (floats spilled to red zone).
