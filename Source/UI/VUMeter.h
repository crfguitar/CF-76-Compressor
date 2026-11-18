#pragma once

#include "Skin.h"

class CF76AudioProcessor;

// The VU meter: bezel, cream face with the -20..+3 scale, needle and screw plate.
// Reads the processor's meter feeds and applies VU ballistics (~300 ms).
class VUMeter : public juce::Component, private juce::Timer
{
public:
    explicit VUMeter (CF76AudioProcessor&);

    void paint (juce::Graphics&) override;

    // Force the needle to a VU reading (used by the snapshot hook).
    void setReading (float vu) { reading = vu; repaint(); }

private:
    void timerCallback() override;

    CF76AudioProcessor& proc;
    float reading = 0.0f;     // VU units (dB relative to 0 VU)
    float smoothedLin = 0.0f; // level ballistics for IN/OUT
    juce::Image faceCache;
};
