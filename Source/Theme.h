#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>

namespace theme
{
    inline const juce::Colour bg0     = juce::Colour (0xff0a0d12);
    inline const juce::Colour bg1     = juce::Colour (0xff141922);
    inline const juce::Colour glass   = juce::Colour (0x14ffffff);
    inline const juce::Colour glassHi = juce::Colour (0x26ffffff);
    inline const juce::Colour stroke  = juce::Colour (0x26ffffff);
    inline const juce::Colour text    = juce::Colour (0xffe8edf5);
    inline const juce::Colour textDim = juce::Colour (0xff8b95a5);
    inline const juce::Colour accent  = juce::Colour (0xff52c7ff);
    inline const juce::Colour accent2 = juce::Colour (0xff8b7bff);
    inline const juce::Colour field   = juce::Colour (0xff0b0e13);

    inline juce::Font font (float height, bool bold = false)
    {
       #if JUCE_MAC
        const char* face = "Helvetica Neue";
       #elif JUCE_WINDOWS
        const char* face = "Segoe UI";
       #else
        const char* face = "DejaVu Sans";
       #endif
        return juce::Font (juce::FontOptions (face, height, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Spectral colour map: black -> navy -> blue -> cyan -> green -> yellow -> orange -> red.
    // t = 0 is the quiet end (-100 dB), t = 1 is the loud end (0 dB).
    inline juce::Colour heat (float t)
    {
        struct Stop { float p; float r, g, b; };
        static constexpr Stop s[] = {
            { 0.00f,   0,   0,   0 }, { 0.08f,   8,  12, 110 }, { 0.26f,  22,  64, 232 },
            { 0.40f,  18, 200, 214 }, { 0.54f,  32, 212,  72 }, { 0.68f, 214, 255,  30 },
            { 0.80f, 255, 212,  30 }, { 0.91f, 255, 140,  30 }, { 1.00f, 240,  60,  20 } };

        t = juce::jlimit (0.0f, 1.0f, t);
        for (int i = 1; i < 9; ++i)
        {
            if (t <= s[i].p)
            {
                const float k = (t - s[i - 1].p) / (s[i].p - s[i - 1].p);
                auto mix = [k] (float a, float b) { return (juce::uint8) juce::roundToInt (a + (b - a) * k); };
                return juce::Colour (mix (s[i - 1].r, s[i].r), mix (s[i - 1].g, s[i].g), mix (s[i - 1].b, s[i].b));
            }
        }
        return juce::Colour ((juce::uint8) 240, (juce::uint8) 60, (juce::uint8) 20);
    }
}
