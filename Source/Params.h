#pragma once

namespace ids
{
    inline constexpr const char* weighting = "weighting";   // A, B, Z (flat), C
    inline constexpr const char* fftSize   = "fftSize";     // 512 ... 16384
    inline constexpr const char* window    = "window";      // Hann, Hamming, Blackman, Rect
    inline constexpr const char* gain      = "gain";        // dB added to the analysis
    inline constexpr const char* floorDb   = "floor";       // bins below this are drawn black
    inline constexpr const char* xlog      = "xlog";        // log / linear frequency axis
    inline constexpr const char* ydir      = "ydir";        // false: newest row on top
    inline constexpr const char* channel   = "channel";     // L, R, LR (mono sum), MIS (side)
    inline constexpr const char* showX     = "showX";
    inline constexpr const char* showY     = "showY";
    inline constexpr const char* showGrid  = "showGrid";
    inline constexpr const char* showRead  = "showRead";
    inline constexpr const char* memTop    = "memTop";      // colour map ceiling (dB)
    inline constexpr const char* memBottom = "memBottom";   // colour map bottom (dB)
    inline constexpr const char* range     = "range";       // history length
    inline constexpr const char* contrast  = "contrast";    // % colour contrast
    inline constexpr const char* active    = "active";      // power button
    inline constexpr const char* hold      = "hold";        // lock / freeze
    inline constexpr const char* fLo       = "fLo";         // zoom strip, low edge (Hz)
    inline constexpr const char* fHi       = "fHi";         // zoom strip, high edge (Hz)
}
