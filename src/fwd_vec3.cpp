#include "types.h"

// sub_82166FD0 -- forward three floats. 16 B, 11 callers.
//   lfs f3,8(r4) ; lfs f2,4(r4) ; lfs f1,0(r4) ; b 0x821528F8
// The loads run backwards because f1 is wanted last; r3 passes through.
struct Vec3 { f32 x; f32 y; f32 z; };
ASSERT_OFFSET(Vec3, x, 0x00);
ASSERT_OFFSET(Vec3, y, 0x04);
ASSERT_OFFSET(Vec3, z, 0x08);
void Apply3(void*, float, float, float);
void ApplyVec3(void* o, const Vec3* v) { Apply3(o, v->x, v->y, v->z); }
