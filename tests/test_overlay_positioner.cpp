#include "overlay/overlay_positioner.h"

#include <cassert>
#include <cmath>
#include <iostream>

static bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

int main()
{
    vrperf::OverlayPositioner positioner;

    positioner.SetHudPosition(0.0f, 0.0f, 1.25f);
    auto hud = positioner.MakeHudTransform();
    assert(Near(hud.m[0][0], 1.0f));
    assert(Near(hud.m[1][1], 1.0f));
    assert(Near(hud.m[2][2], 1.0f));
    assert(Near(hud.m[2][3], -1.25f));

    positioner.SetWristHand(true);
    auto left = positioner.MakeWristTransform();
    assert(left.m[0][3] > 0.0f);

    positioner.SetWristHand(false);
    auto right = positioner.MakeWristTransform();
    assert(right.m[0][3] <= 0.0f);

    std::cout << "[PASS] OverlayPositioner transform tests passed\n";
    return 0;
}
