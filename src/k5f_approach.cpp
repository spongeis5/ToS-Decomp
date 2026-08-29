// sub_821AEC78 -- variant probe.
#include "types.h"

void ApproachF(f32* p, f32 target, f32 dt, f32 rate, f32 limit)
{
    f32 t = dt * rate * 60.0f;
    if (t >= 1.0f)
        t = 1.0f;

    f32 m = dt * limit * 60.0f;
    f32 v = *p;
    f32 s = (target - v) * t;

    if (s > m)
    {
        *p = v + m;
        return;
    }

    m = -m;
    if (s < m)
    {
        *p = v + m;
        return;
    }

    *p = v + s;
}
