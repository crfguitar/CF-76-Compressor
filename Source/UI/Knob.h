#pragma once

#include "Skin.h"

// A rotary control drawn to match the CLA-76: dot scale, numeric labels (nearest one lit blue)
// and the knob itself. Geometry is in reference pixels relative to the knob centre.
struct KnobSpec
{
    enum class Body { chickenHead, small, tiny };

    struct Label
    {
        juce::String text;
        float dx, dy;   // text centre relative to knob centre
        int dotIndex;   // dot that lights up with this label (-1 = none)
    };

    Body body;
    float startDeg, endDeg;          // pointer angle at min / max (0 = up, clockwise)
    float dotRing;                   // radius of the dot scale
    float dotDiameter;
    std::vector<float> dotAngles;    // degrees
    std::vector<Label> labels;
    float labelFontPx;
    bool highlightNearest = true;
    float extent;                    // half-size of the component around the centre

    static KnobSpec bigInputOutput();
    static KnobSpec attackRelease();
    static KnobSpec mixTrim (juce::String minText, juce::String maxText);
};

class Knob : public juce::Slider
{
public:
    explicit Knob (KnobSpec);

    // Place the knob so its centre sits at (cx, cy) in the parent's reference coordinates.
    void setCentre (float cx, float cy);

    // Colour of the scale dots and labels (white on Blacky, near-black on the Bluey panel).
    void setInk (juce::Colour);

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;

private:
    float pointerAngleDeg() const;
    int activeDot() const;

    void drawScale (juce::Graphics&, juce::Point<float> c, int lit) const;
    void drawChickenHead (juce::Graphics&, juce::Point<float> c, float angleRad) const;
    void drawSmall (juce::Graphics&, juce::Point<float> c, float angleRad, float radius) const;

    KnobSpec spec;
    juce::Colour ink { Skin::labelText };
};
