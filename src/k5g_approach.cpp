// sub_821AEC78 -- variant probe.
#include "types.h"

struct TrackedG
{
    f32 value;
    void Approach(f32 target, f32 dt, f32 rate, f32 limit);
};

void TrackedG::Approach(f32 target, f32 dt, f32 rate, f32 limit)
{
    f32 t = dt * rate * 60.0f;
    if (t >= 1.0f)
        t = 1.0f;

    f32 m = dt * limit * 60.0f;
    f32 s = (target - value) * t;

    if (s > m)
    {
        value = value + m;
        return;
    }

    m = -m;
    if (s < m)
    {
        value = value + m;
        return;
    }

    value = value + s;
}
