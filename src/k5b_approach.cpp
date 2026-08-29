// sub_821AEC78 -- variant B: named 60.0f local with the first product
// parenthesised so /fp:fast cannot reassociate it.
#include "types.h"

void ApproachB(f32* p, f32 target, f32 dt, f32 rate, f32 limit)
{
    f32 k = 60.0f;
    f32 t = (dt * rate) * k;
    if (t >= 1.0f)
        t = 1.0f;

    f32 v = *p;
    f32 m = (dt * limit) * k;
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
