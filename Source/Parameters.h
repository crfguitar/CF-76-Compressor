#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter IDs and ranges, taken from the CLA-76 user guide (appendix, p.11).
namespace ParamIDs
{
    inline constexpr auto input      = "input";      // dB attenuation, -inf..0, default -30
    inline constexpr auto output     = "output";     // dB attenuation, -inf..0, default -18
    inline constexpr auto attack     = "attack";     // 1 (slow, 1 ms) .. 7 (fast, <50 us), default 3
    inline constexpr auto release    = "release";    // 1 (slow, 1 s) .. 7 (fast, 50 ms), default 4
    inline constexpr auto ratio      = "ratio";      // 4, 8, 12, 20, All; default 4
    inline constexpr auto compOff    = "compOff";    // default Comp On
    inline constexpr auto revision   = "revision";   // Bluey, Blacky; default Blacky
    inline constexpr auto analog     = "analog";     // Off, 50Hz, 60Hz; default 60Hz
    inline constexpr auto meter      = "meter";      // GR, IN, OUT; default GR
    inline constexpr auto mix        = "mix";        // 0..100 %, default 100
    inline constexpr auto trim       = "trim";       // -18..+18 dB, default 0
    inline constexpr auto autoMakeup = "autoMakeup"; // default off
}

enum class Ratio    { r4, r8, r12, r20, all };
enum class Revision { bluey, blacky };
enum class Analog   { off, hz50, hz60 };
enum class MeterMode { gr, in, out };

// Attenuation knobs bottom out at this value, which is treated as -inf.
inline constexpr float minusInfDb = -60.0f;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
