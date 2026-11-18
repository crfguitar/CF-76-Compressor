#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "Presets.h"
#include "DSP/FetCompressor.h"

class CF76AudioProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener,
                           private juce::AsyncUpdater
{
public:
    CF76AudioProcessor();
    ~CF76AudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undoManager;          // must be declared before apvts
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts, undoManager };

    // Meter feeds for the UI (VU ballistics are applied in the editor).
    std::atomic<float> meterInDb { -100.0f }, meterOutDb { -100.0f }, meterGrDb { 0.0f };
    std::atomic<float> meterInRmsDb { -100.0f }, meterOutRmsDb { -100.0f };

    // Over-level latches for the lamp above the meter (cleared by clicking it).
    std::atomic<bool> overIn { false }, overOut { false };
    void clearOvers() { overIn = false; overOut = false; }

    // Static output level (dBFS peak) of a 0 VU reference tone for the given Input / Ratio
    // settings - the curve Auto Makeup follows. Exposed for tests.
    static float referenceOutputDb (float inputDb, Ratio, Revision = Revision::blacky);

private:
    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override;
    FetCompressor::Settings readSettings() const;

    FetCompressor comp;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 12 };
    juce::AudioBuffer<float> dryBuffer;
    juce::LinearSmoothedValue<float> wetGain, trimGain;

    // Auto Makeup: Input drives Output along the compression curve; manual Output moves re-anchor.
    std::atomic<bool> autoMakeupPending { false };
    bool applyingAutoMakeup = false;
    float anchorInputDb = -30.0f, anchorOutputDb = -18.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CF76AudioProcessor)
};
