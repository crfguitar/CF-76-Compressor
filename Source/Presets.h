#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory + user presets, the A/B setup slots, and the "modified" (*) tracking shown in the toolbar.
// User presets are APVTS state XML files in Documents/CF76/Presets/*.cf76preset.
class PresetManager : private juce::ValueTree::Listener
{
public:
    struct Entry
    {
        juce::String name;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    PresetManager (juce::AudioProcessorValueTreeState&, juce::UndoManager&);
    ~PresetManager() override;

    static juce::File userDirectory();
    static juce::String fileExtension() { return ".cf76preset"; }

    juce::Array<Entry> entries() const; // factory presets first, then user presets (sorted)
    int currentIndex() const;           // index into entries(), or -1

    void load (const Entry&);
    void step (int delta);              // previous / next preset
    void resetToDefault();
    bool loadFile (const juce::File&);
    bool saveTo (const juce::File&);
    bool canOverwrite() const;          // current preset is a user file
    bool overwrite();

    juce::String name() const   { return slots[(size_t) active].name; }
    bool isModified() const     { return slots[(size_t) active].modified; }

    // A/B setups.
    int activeSlot() const { return active; }
    void toggleSlot();          // Setup A <-> Setup B
    void copyToOtherSlot();     // A -> B (or B -> A)

    // Persist names, slots and the modified flag alongside the plugin state.
    void writeTo (juce::XmlElement&) const;
    void readFrom (const juce::XmlElement&);

private:
    struct Slot
    {
        juce::ValueTree state;
        juce::String name { "Default" };
        juce::File file;
        bool modified = false;
    };

    void applyState (const juce::ValueTree&, bool undoable);
    void markClean (const juce::String& name, const juce::File& file);
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    juce::AudioProcessorValueTreeState& apvts;
    juce::UndoManager& undo;
    std::array<Slot, 2> slots;
    int active = 0;
    juce::uint32 ignoreChangesUntil = 0;
};
