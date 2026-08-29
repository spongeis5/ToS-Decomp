// sub_821AEC78 -- variant probe.
#include "types.h"

static f32 FrameI(f32 dt, f32 x) { return dt * x * 60.0f; }

void ApproachI(f32* p, f32 target, f32 dt, f32 rate, f32 limit)
{
    f32 t = FrameI(dt, rate);
    if (t >= 1.0f)
        t = 1.0f;

    f32 m = FrameI(dt, limit);
    f32 s = (target - *p) * t;

    if (s > m)
    {
        *p = *p + m;
        return;
    }

    m = -m;
    if (s < m)
    {
        *p = *p + m;
        return;
    }

    *p = *p + s;
}
