#pragma once

#include "Skin.h"

class CF76AudioProcessor;

// Top bar: CF-76 badge, undo/redo, preset name + menu, prev/next, A->B, Setup A/B, Save, menu.
// Geometry measured from the reference toolbar (y 0..60 in reference px).
class TopBar : public juce::Component, private juce::Timer
{
public:
    explicit TopBar (CF76AudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    enum class Item { undo, redo, preset, prev, next, copyAB, setup, save, menu, none };

    void timerCallback() override;
    Item itemAt (juce::Point<float>) const;
    void showPresetMenu();
    void showMainMenu();
    void saveAs();
    void loadFromFile();

    CF76AudioProcessor& proc;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastSignature; // repaint only when the visible state changes
};
