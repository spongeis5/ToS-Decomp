// sub_821AEC78 -- variant probe.
#include "types.h"

void ApproachD(f32* p, f32 target, f32 dt, f32 rate, f32 limit)
{
    f32 t = 60.0f * (dt * rate);
    if (t >= 1.0f)
        t = 1.0f;

    f32 v = *p;
    f32 m = 60.0f * (dt * limit);
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
