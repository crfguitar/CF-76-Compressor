#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Parameters.h"

// The CF-76 gain path, run at the oversampled rate (block diagram from the 1176LN manual):
//   input attenuator -> input transformer -> FET gain cell -> preamp -> [sidechain tap]
//   -> noise/hum -> output attenuator -> class A output amp/transformer -> LF/HF response
//
// The detector is a FEEDBACK design like the hardware: it measures the preamp output (after gain
// reduction, before the Output control). The loop is solved implicitly each sample, so ratios are
// exact and the loop stays stable even at 20 us attack.
//
// Calibration: -18 dBFS = +4 dBu = 0 VU. Default Input (-30) and Output (-18) give unity gain.
class FetCompressor
{
public:
    struct Settings
    {
        float inputDb = -30.0f, outputDb = -18.0f; // knob values (attenuation, <= minusInfDb is -inf)
        float attackKnob = 3.0f, releaseKnob = 4.0f;
        Ratio ratio = Ratio::r4;
        bool compOff = false;
        Revision revision = Revision::blacky;
        Analog analog = Analog::hz60;
    };

    // Static compression law for one ratio/revision (exposed for Auto Makeup and tests).
    struct Curve
    {
        float thresholdDb;  // input-referred threshold (manual: 4:1 -18 dBFS ... 20:1 -12 dBFS)
        float ratio;
        float kneeDb;       // knee width, input-referred
        float overSlope;    // All-buttons: extra reduction past the knee (output falls as input rises)
        float stiffen;      // pre-LN (Bluey) curve non-linearity: ratio creeps up with depth
    };
    static Curve curveFor (Ratio, Revision);

    // Steady-state gain reduction (dB, <= 0) for an FET-input level, solving the feedback loop.
    static float staticGainReductionDb (const Curve&, float levelDb);

    static constexpr float inputOffsetDb = 30.0f;  // Input knob at -30 => unity into the FET
    static constexpr float outputOffsetDb = 18.0f; // Output knob at -18 => unity out

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setSettings (const Settings&);

    // In-place processing of an oversampled block.
    void process (juce::dsp::AudioBlock<float>&);

    // Deepest gain reduction (dB, <= 0) since the last call.
    float takeMaxGainReductionDb();

    // Current gain reduction (dB, <= 0); for tests.
    float currentGainReductionDb() const { return envDb; }

private:
    struct Shaper { float headroom, even, hardness; };
    static float shape (float x, Shaper s);

    // Feedback law pieces (output-referred over-threshold -> reduction).
    static float knee (float o, float w);
    static float kneeSlope (float o, float w);
    static float solve (const Curve&, float levelDb, float prevEnv, float coef, float thresholdShift, float ratioScale);

    double fs = 88200.0;
    int channels = 2;
    Settings settings;

    juce::LinearSmoothedValue<float> inGain, outGain;
    float attackCoef = 0.0f, releaseFast = 0.1f, releaseSlowFactor = 3.0f, transitionCoef = 0.0f, sustainDecayCoef = 0.0f;
    Curve curve {};
    float fetDistortion = 0.0f;
    Shaper inputXfmr {}, preamp {}, outputStage {};
    bool allButtons = false;
    int lagSamples = 0;

    float envDb = 0.0f;      // gain reduction (dB, <= 0)
    float sustained = 0.0f;  // 0..1, how long we've been compressing (program-dependent release)
    int lagCount = 0;        // All-buttons attack lag ("reverse look-ahead")
    int idleSamples = 0, idleRearm = 0;
    float maxGrDb = 0.0f;

    // Analog: noise + mains hum.
    float noiseGain = 0.0f, humGain = 0.0f, humPhase = 0.0f, humInc = 0.0f;
    std::array<float, 2> noiseLp {};
    float noiseLpCoef = 0.0f;
    juce::Random rng { 1176 };

    // Transformer frequency response: LF high-pass (also blocks DC) and gentle HF low-pass.
    std::array<float, 2> hpX {}, hpY {};
    float hpCoef = 0.0f;
    std::array<juce::IIRFilter, 2> lowpass;
};
