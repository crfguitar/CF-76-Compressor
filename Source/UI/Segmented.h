#pragma once

#include "Skin.h"

// Row of cream "light" buttons in the bottom bar (AUTO MAKEUP, ANALOG, REVISION).
// Choice parameters select the clicked segment; a bool parameter toggles its single segment.
class SegmentedButtons : public juce::Component,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct Segment
    {
        juce::Rectangle<float> bounds; // reference px, parent coordinates (unpressed position)
        juce::String text;
        int value;                     // choice index (ignored for bool parameters)
    };

    SegmentedButtons (juce::AudioProcessorValueTreeState&, juce::String paramId,
                      std::vector<Segment>, float pressedDrop = 0.0f);
    ~SegmentedButtons() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void parameterChanged (const juce::String&, float) override;
    bool isOn (const Segment&) const;

    juce::AudioProcessorValueTreeState& state;
    juce::String paramId;
    std::vector<Segment> segments;
    float drop;
    bool isBool;
    juce::Point<float> origin;
};
