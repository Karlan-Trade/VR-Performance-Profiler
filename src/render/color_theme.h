#pragma once

#include <d2d1.h>

namespace vrperf {

struct ColorTheme {
    D2D1_COLOR_F background;
    D2D1_COLOR_F textPrimary;
    D2D1_COLOR_F textSecondary;
    D2D1_COLOR_F accentGood;      // Green for normal values
    D2D1_COLOR_F accentWarning;   // Yellow for warning
    D2D1_COLOR_F accentCritical;  // Red for critical
    D2D1_COLOR_F barBackground;
    D2D1_COLOR_F barFill;
    D2D1_COLOR_F panelBackground;
    D2D1_COLOR_F divider;
};

// Dark theme (default, works well in VR)
inline ColorTheme DarkTheme()
{
    return {
        {0.05f, 0.05f, 0.08f, 0.90f},  // background
        {1.0f,  1.0f,  1.0f,  1.0f},    // textPrimary
        {0.7f,  0.7f,  0.7f,  1.0f},    // textSecondary
        {0.2f,  0.8f,  0.3f,  1.0f},    // accentGood
        {1.0f,  0.8f,  0.0f,  1.0f},    // accentWarning
        {1.0f,  0.2f,  0.2f,  1.0f},    // accentCritical
        {0.15f, 0.15f, 0.18f, 1.0f},    // barBackground
        {0.3f,  0.6f,  1.0f,  1.0f},    // barFill
        {0.08f, 0.08f, 0.12f, 0.85f},   // panelBackground
        {0.3f,  0.3f,  0.35f, 0.5f},    // divider
    };
}

// Light theme
inline ColorTheme LightTheme()
{
    return {
        {0.95f, 0.95f, 0.95f, 0.90f},  // background
        {0.1f,  0.1f,  0.1f,  1.0f},    // textPrimary
        {0.4f,  0.4f,  0.4f,  1.0f},    // textSecondary
        {0.1f,  0.6f,  0.2f,  1.0f},    // accentGood
        {0.8f,  0.6f,  0.0f,  1.0f},    // accentWarning
        {0.9f,  0.1f,  0.1f,  1.0f},    // accentCritical
        {0.85f, 0.85f, 0.85f, 1.0f},    // barBackground
        {0.2f,  0.5f,  0.9f,  1.0f},    // barFill
        {0.92f, 0.92f, 0.92f, 0.85f},   // panelBackground
        {0.7f,  0.7f,  0.7f,  0.5f},    // divider
    };
}

} // namespace vrperf
