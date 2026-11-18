#include "FetCompressor.h"

using namespace juce;

namespace
{
    // Knob position 1..7 (7 = fastest), log-interpolated between the hardware extremes.
    float knobTime (float knob, float slowSec, float fastSec)
    {
        auto t = jlimit (0.0f, 1.0f, (knob - 1.0f) / 6.0f);
        return slowSec * std::pow (fastSec / slowSec, t);
    }

    float onePole (double fs, float seconds)
    {
        return std::exp (-1.0f / (seconds * (float) fs));
    }

    float attenuationGain (float db)
    {
        return db <= minusInfDb ? 0.0f : Decibels::decibelsToGain (db);
    }

    // Per-revision constants. Values follow the manuals where they give numbers; the rest are
    // voiced by ear (see the DSP report for what each produces).
    struct Voicing
    {
        float attackScale, releaseScale;
        float slowReleaseFactor, transitionSec;  // program-dependent release (UA manual: fast/slow/transition)
        float fetDistortion;                     // pre-LN FET distortion is much higher (1176LN manual)
        float inputHeadroom;                     // Rev A/B input clipping "most evident" (UA manual)
        float preampHeadroom, preampEven;
        float noiseDb;
        float kneeScale, stiffen;                // LN: "more linear compression response"
    };

    Voicing voicing (Revision r)
    {
        if (r == Revision::bluey)  // Rev B "Bluestripe", pre-LN
            return { 1.15f, 1.1f, 4.0f, 0.6f, 0.25f, 2.5f, 1.6f, 0.016f, -84.0f, 1.7f, 0.025f };
        return     { 1.0f,  1.0f, 3.0f, 0.3f, 0.08f, 4.0f, 2.0f, 0.008f, -90.0f, 1.0f, 0.0f };  // Rev D/E "Blackface" LN
    }
}

//==============================================================================
// Compression law

FetCompressor::Curve FetCompressor::curveFor (Ratio r, Revision rev)
{
    auto v = voicing (rev);
    Curve c { -18.0f, 4.0f, 10.0f, 0.0f, 0.0f };
    switch (r)
    {
        case Ratio::r4:  c = { -18.0f,  4.0f, 10.0f, 0.0f,  0.0f }; break;
        case Ratio::r8:  c = { -16.0f,  8.0f,  8.0f, 0.0f,  0.0f }; break;
        case Ratio::r12: c = { -14.0f, 12.0f,  6.0f, 0.0f,  0.0f }; break;
        case Ratio::r20: c = { -12.0f, 20.0f,  4.0f, 0.0f,  0.0f }; break;
        // All-buttons: "ratio goes to somewhere between 12:1 and 20:1" plus the bias-shift hump.
        case Ratio::all: c = { -15.0f, 16.0f,  3.0f, 0.1f,  0.0f }; break;
    }
    c.kneeDb *= v.kneeScale;
    c.stiffen = v.stiffen;
    return c;
}

float FetCompressor::knee (float o, float w)
{
    if (o <= -0.5f * w) return 0.0f;
    if (o >= 0.5f * w) return o;
    return square (o + 0.5f * w) / (2.0f * w);
}

float FetCompressor::kneeSlope (float o, float w)
{
    if (o <= -0.5f * w) return 0.0f;
    if (o >= 0.5f * w) return 1.0f;
    return (o + 0.5f * w) / w;
}

float FetCompressor::solve (const Curve& c, float levelDb, float prevEnv, float coef,
                            float thresholdShift, float ratioScale)
{
    // Feedback law: the sidechain sees the OUTPUT over threshold, o = level + env - T, and demands
    // env = -k * knee(o) with k = R - 1, which settles at (out - T) = (in - T) / R.
    // Knee width is converted from input-referred to output-referred.
    // With smoothing: env = coef * prevEnv + (1 - coef) * target(env), solved by Newton's method.
    const float k = (c.ratio - 1.0f) * ratioScale;
    const float w = jmax (0.05f, 2.0f * c.kneeDb / (c.ratio + 1.0f));
    const float t = c.thresholdDb + thresholdShift;
    // All-buttons hump acts on the input level (bias shift), so it must survive the loop: the loop
    // divides any reduction it sees by R, hence the scaling.
    const float extra = c.overSlope > 0.0f ? -c.overSlope * c.ratio * jmax (0.0f, levelDb - t - 6.0f) : 0.0f;

    float env = prevEnv;
    for (int it = 0; it < 6; ++it)
    {
        const float o = levelDb + env - t;
        const float h = knee (o, w), dh = kneeSlope (o, w);
        const float target = -k * h * (1.0f + c.stiffen * h) + extra;   // pre-LN: ratio creeps up with depth
        const float dTarget = -k * dh * (1.0f + 2.0f * c.stiffen * h);
        const float f = env - coef * prevEnv - (1.0f - coef) * target;
        const float df = 1.0f - (1.0f - coef) * dTarget;
        const float step = f / df;
        env = jlimit (-60.0f, 0.0f, env - step);
        if (std::abs (step) < 1.0e-4f) break;
    }
    return env;
}

float FetCompressor::staticGainReductionDb (const Curve& c, float levelDb)
{
    return solve (c, levelDb, 0.0f, 0.0f, 0.0f, 1.0f);
}

//==============================================================================

float FetCompressor::shape (float x, Shaper s)
{
    // hardness 1: tanh-style soft saturation. Higher hardness keeps the stage clean until near the
    // clip level (class A output amp: <= 0.5% THD up to +22 dBm, then a firm ceiling).
    float y;
    if (s.hardness <= 1.0f)
        y = s.headroom * std::tanh (x / s.headroom);
    else
    {
        const float r = std::abs (x) / s.headroom;
        y = x / std::pow (1.0f + std::pow (r, 2.0f * s.hardness), 0.5f / s.hardness);
    }
    return y + s.even * y * y;
}

void FetCompressor::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = jlimit (1, 2, numChannels);
    inGain.reset (sampleRate, 0.02);
    outGain.reset (sampleRate, 0.02);
    noiseLpCoef = std::exp (-MathConstants<float>::twoPi * 9000.0f / (float) fs);
    hpCoef = std::exp (-MathConstants<float>::twoPi * 6.0f / (float) fs);
    for (auto& f : lowpass)
        f.setCoefficients (IIRCoefficients::makeLowPass (fs, jmin (40000.0, 0.45 * fs), 0.7071));
    setSettings (settings);
    reset();
}

void FetCompressor::reset()
{
    envDb = sustained = maxGrDb = 0.0f;
    lagCount = idleSamples = 0;
    noiseLp = {};
    hpX = {};
    hpY = {};
    for (auto& f : lowpass) f.reset();
    humPhase = 0.0f;
    inGain.setCurrentAndTargetValue (inGain.getTargetValue());
    outGain.setCurrentAndTargetValue (outGain.getTargetValue());
}

void FetCompressor::setSettings (const Settings& s)
{
    settings = s;
    inGain.setTargetValue (attenuationGain (s.inputDb) * Decibels::decibelsToGain (inputOffsetDb));
    outGain.setTargetValue (attenuationGain (s.outputDb) * Decibels::decibelsToGain (outputOffsetDb));

    const auto v = voicing (s.revision);
    allButtons = s.ratio == Ratio::all;

    // Hardware ranges (1176LN manual): attack 800 us .. 20 us, release 1.1 s .. 50 ms.
    // Lower ratios keep the fastest attack (UA manual). Linking two units for stereo doubles the
    // fastest attack to 40 us (1176LN manual, stereo operation).
    const float ratioAttack[] = { 1.0f, 1.08f, 1.16f, 1.25f, 1.3f };
    auto attack = knobTime (s.attackKnob, 800.0e-6f, 20.0e-6f) * v.attackScale * ratioAttack[(int) s.ratio];
    if (channels == 2) attack += 20.0e-6f;
    auto release = knobTime (s.releaseKnob, 1.1f, 50.0e-3f) * v.releaseScale * (allButtons ? 0.8f : 1.0f);
    // Above threshold the feedback loop speeds the response up by ~R, so the smoothing constant is
    // stretched by R to make the *measured* attack match the spec.
    curve = curveFor (s.ratio, s.revision);
    attackCoef = onePole (fs, attack * curve.ratio);
    releaseFast = release;
    releaseSlowFactor = v.slowReleaseFactor;
    transitionCoef = onePole (fs, v.transitionSec);
    sustainDecayCoef = onePole (fs, 4.0f * v.transitionSec);  // slow mode lingers after the signal stops

    fetDistortion = v.fetDistortion * (allButtons ? 2.5f : 1.0f);
    lagSamples = allButtons ? (int) (0.8e-3 * fs) : 0;  // "lag time on the attack of initial transients"
    idleRearm = (int) (0.03 * fs);

    inputXfmr   = { v.inputHeadroom, 0.0f, 1.0f };
    preamp      = { v.preampHeadroom, v.preampEven, 1.0f };
    outputStage = { 2.6f, 0.004f, 3.0f };                // clean to ~0 dBFS (+22 dBm), firm ceiling above

    noiseGain = s.analog == Analog::off ? 0.0f : Decibels::decibelsToGain (v.noiseDb);
    humGain = s.analog == Analog::hz50 ? Decibels::decibelsToGain (-82.0f)
            : s.analog == Analog::hz60 ? Decibels::decibelsToGain (-86.0f) : 0.0f;
    humInc = MathConstants<float>::twoPi * (s.analog == Analog::hz50 ? 50.0f : 60.0f) / (float) fs;
}

void FetCompressor::process (dsp::AudioBlock<float>& block)
{
    const auto n = (int) block.getNumSamples();
    const auto nch = jmin (channels, (int) block.getNumChannels());
    const bool compOff = settings.compOff;
    const float noiseScale = noiseGain * 1.7f; // white -> ~equal RMS after the low-pass

    for (int i = 0; i < n; ++i)
    {
        const float gIn = inGain.getNextValue(), gOut = outGain.getNextValue();

        // Input attenuator + transformer, then the FET input level (stereo-linked sidechain).
        float s[2] {}, level = 0.0f;
        for (int ch = 0; ch < nch; ++ch)
        {
            s[ch] = shape (block.getSample (ch, i) * gIn, inputXfmr);
            level = jmax (level, std::abs (s[ch]));
        }
        const float levelDb = Decibels::gainToDecibels (level, -120.0f);

        if (compOff)
        {
            envDb = 0.0f;
            sustained = 0.0f;
        }
        else
        {
            // All-buttons bias shift: the curve keeps moving with how hard we've been compressing.
            const float tShift = allButtons ? -1.5f * sustained : 0.0f;
            const float rScale = allButtons ? 1.0f + 0.15f * sustained : 1.0f;

            // Branch on what the loop wants from here: attack (more reduction) or release.
            const float demand = solve (curve, levelDb, envDb, 0.0f, tShift, rScale);
            if (demand < envDb)
            {
                // All-buttons attack lag on initial transients (only after the unit has been idle).
                if (lagCount < lagSamples)
                    ++lagCount;
                else
                    envDb = solve (curve, levelDb, envDb, attackCoef, tShift, rScale);
            }
            else
            {
                // Program-dependent release: fast after transients, up to `slowFactor` x slower after
                // sustained compression; `sustained` builds over the transition time.
                const float t = releaseFast * (1.0f + (releaseSlowFactor - 1.0f) * sustained);
                envDb = solve (curve, levelDb, envDb, onePole (fs, t), tShift, rScale);
            }

            // The attack lag re-arms only once the unit has been idle for a while (not at every
            // zero crossing of a waveform).
            idleSamples = (envDb > -0.1f && demand > -0.1f) ? idleSamples + 1 : 0;
            if (idleSamples > idleRearm)
                lagCount = 0;

            // `sustained` builds while the signal is driving compression and fades otherwise; it is
            // what turns the fast release into the slow one.
            const float driving = demand < -1.0f ? 1.0f : 0.0f;
            sustained = driving + (driving > sustained ? transitionCoef : sustainDecayCoef) * (sustained - driving);
        }
        maxGrDb = jmin (maxGrDb, envDb);

        const float g = Decibels::decibelsToGain (envDb);
        const float fetAmount = fetDistortion * jlimit (0.0f, 1.0f, -envDb / 20.0f);

        // Mains hum: fundamental + 2nd/3rd harmonics, identical on both channels.
        float hum = 0.0f;
        if (humGain > 0.0f)
        {
            humPhase += humInc;
            if (humPhase > MathConstants<float>::twoPi) humPhase -= MathConstants<float>::twoPi;
            hum = humGain * (std::sin (humPhase) + 0.5f * std::sin (2.0f * humPhase) + 0.35f * std::sin (3.0f * humPhase));
        }

        for (int ch = 0; ch < nch; ++ch)
        {
            // FET cell: gain reduction plus even-order distortion that grows with the reduction.
            auto y = s[ch] * g;
            y += fetAmount * y * y;
            y = shape (y, preamp);

            if (noiseGain > 0.0f)
            {
                auto white = (rng.nextFloat() * 2.0f - 1.0f) * noiseScale;
                noiseLp[(size_t) ch] = white + noiseLpCoef * (noiseLp[(size_t) ch] - white);
                y += noiseLp[(size_t) ch] + hum;
            }

            y = shape (y * gOut, outputStage);

            // Transformer response: 6 Hz high-pass (also removes DC from the even-order terms)
            // and a gentle low-pass well above the audio band (spec: 20 Hz - 20 kHz +/- 1 dB).
            auto hp = y - hpX[(size_t) ch] + hpCoef * hpY[(size_t) ch];
            hpX[(size_t) ch] = y;
            hpY[(size_t) ch] = hp;
            block.setSample (ch, i, lowpass[(size_t) ch].processSingleSampleRaw (hp));
        }
    }
}

float FetCompressor::takeMaxGainReductionDb()
{
    auto v = maxGrDb;
    maxGrDb = envDb;
    return v;
}
