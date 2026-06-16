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

    positioner.SetOverlayOffset(0.12f, -0.08f, 0.2f);
    auto offsetHud = positioner.MakeHudTransform();
    assert(Near(offsetHud.m[0][3], 0.12f));
    assert(Near(offsetHud.m[1][3], -0.38f));
    assert(Near(offsetHud.m[2][3], -1.05f));

    positioner.SetOverlayOffset(0.0f, 0.0f, 0.0f);
    positioner.SetWristHand(true);
    auto left = positioner.MakeWristTransform();
    assert(Near(left.m[0][0], 0.0f));
    assert(Near(left.m[0][1], 0.0f));
    assert(Near(left.m[0][2], -0.25f));
    assert(Near(left.m[0][3], -0.07f));
    assert(Near(left.m[1][0], 0.0f));
    assert(Near(left.m[1][1], -0.25f));
    assert(Near(left.m[1][2], 0.0f));
    assert(Near(left.m[1][3], -0.05f));
    assert(Near(left.m[2][0], -0.25f));
    assert(Near(left.m[2][1], 0.0f));
    assert(Near(left.m[2][2], 0.0f));
    assert(Near(left.m[2][3], 0.06f));

    positioner.SetOverlayOffset(0.12f, -0.08f, 0.2f);
    auto offsetLeft = positioner.MakeWristTransform();
    assert(Near(offsetLeft.m[0][3], 0.05f));
    assert(Near(offsetLeft.m[1][3], -0.13f));
    assert(Near(offsetLeft.m[2][3], 0.26f));

    positioner.SetOverlayOffset(0.0f, 0.0f, 0.0f);
    positioner.SetWristHand(false);
    auto right = positioner.MakeWristTransform();
    assert(Near(right.m[0][0], 0.0f));
    assert(Near(right.m[0][1], 0.0f));
    assert(Near(right.m[0][2], 0.25f));
    assert(Near(right.m[0][3], 0.07f));
    assert(Near(right.m[1][0], 0.0f));
    assert(Near(right.m[1][1], -0.25f));
    assert(Near(right.m[1][2], 0.0f));
    assert(Near(right.m[1][3], -0.05f));
    assert(Near(right.m[2][0], 0.25f));
    assert(Near(right.m[2][1], 0.0f));
    assert(Near(right.m[2][2], 0.0f));
    assert(Near(right.m[2][3], 0.06f));

    positioner.SetOverlayOffset(-0.12f, 0.08f, -0.2f);
    auto offsetRight = positioner.MakeWristTransform();
    assert(Near(offsetRight.m[0][3], -0.05f));
    assert(Near(offsetRight.m[1][3], 0.03f));
    assert(Near(offsetRight.m[2][3], -0.14f));

    std::cout << "[PASS] OverlayPositioner transform tests passed\n";
    return 0;
}
