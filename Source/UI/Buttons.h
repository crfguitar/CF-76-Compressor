#pragma once

#include "Skin.h"

// Vertical stack of square push buttons in a black frame (RATIO and METER columns).
// Each cell maps to a parameter value; the lit cell shows the current selection.
class ButtonStack : public juce::Component,
                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct Cell
    {
        juce::Rectangle<float> bounds;  // reference px, parent coordinates
        juce::String label;
        bool red = false;               // COMP OFF style
    };

    struct Spec
    {
        std::vector<Cell> cells;
        juce::Rectangle<float> frame;   // outer black frame, reference px
        bool labelsLeft = true;         // RATIO labels sit left (right-aligned), METER labels right
        float labelEdgeX = 0.0f;        // right edge (left labels) or left edge (right labels)
        std::vector<float> labelY;      // measured ink centres, one per label line
        bool highlightSelectedLabel = false;
    };

    // `cellValue[i]` is the choice index cell i selects. A red cell toggles `toggleParam` instead.
    ButtonStack (juce::AudioProcessorValueTreeState&, Spec, juce::String choiceParam,
                 std::vector<int> cellValue, juce::String toggleParam = {});
    ~ButtonStack() override;

    void setInk (juce::Colour);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void parameterChanged (const juce::String&, float) override;
    bool isLit (int cell) const;

    juce::AudioProcessorValueTreeState& state;
    Spec spec;
    juce::String choiceId, toggleId;
    std::vector<int> values;
    juce::Colour ink { Skin::labelText };
    juce::Point<float> origin;
};

// Draw one rounded push button (pillow shading, mottled when lit). Shared with the bottom bar.
void drawPushButton (juce::Graphics&, juce::Rectangle<float>, bool lit, bool red);
