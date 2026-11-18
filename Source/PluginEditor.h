#pragma once

#include "PluginProcessor.h"
#include "UI/Knob.h"
#include "UI/Buttons.h"
#include "UI/VUMeter.h"
#include "UI/Segmented.h"
#include "UI/TopBar.h"

// Everything is laid out on a canvas in reference pixels (2396 x 865) and scaled to the window.
class Canvas : public juce::Component, private juce::Timer
{
public:
    explicit Canvas (CF76AudioProcessor&);
    ~Canvas() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    // Re-applies revision-dependent colours (called when the Revision parameter changes).
    void updateSkin();

private:
    Revision revision() const;
    void timerCallback() override;
    bool lampLit = false;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    CF76AudioProcessor& proc;
    juce::SharedResourcePointer<Skin::Cache> skinCache;

    Knob input   { KnobSpec::bigInputOutput() };
    Knob output  { KnobSpec::bigInputOutput() };
    Knob attack  { KnobSpec::attackRelease() };
    Knob release { KnobSpec::attackRelease() };
    Knob mix     { KnobSpec::mixTrim ("0", "100") };
    Knob trim    { KnobSpec::mixTrim ("-18", "18") };

    std::unique_ptr<ButtonStack> ratioButtons, meterButtons;
    std::unique_ptr<SegmentedButtons> autoMakeup, analog, revisionButtons;
    VUMeter vu { proc };
    TopBar toolbar { proc };

    std::vector<std::unique_ptr<SliderAttachment>> attachments;

public:
    VUMeter& getVU() { return vu; }

private:

    struct RevisionListener;
    std::unique_ptr<RevisionListener> revisionListener;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Canvas)
};

class CF76AudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit CF76AudioProcessorEditor (CF76AudioProcessor&);

    void resized() override;

private:
    void timerCallback() override;
    void writeSnapshots();
    void runSelfTest();

    CF76AudioProcessor& proc;
    Canvas canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CF76AudioProcessorEditor)
};
