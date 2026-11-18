#include "DevTests.h"
#include "PluginProcessor.h"

using namespace juce;

namespace
{
    constexpr double fs = 48000.0;
    constexpr int block = 512;

    struct Rig
    {
        Rig()
        {
            proc.setPlayConfigDetails (2, 2, fs, block);
            proc.prepareToPlay (fs, block);
            set (ParamIDs::analog, (float) Analog::off);
        }

        void set (const char* id, float v)
        {
            auto* p = proc.apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        }

        // Runs `in` (mono, duplicated to both channels) through the processor; returns channel 0.
        std::vector<float> run (const std::vector<float>& in)
        {
            std::vector<float> out (in.size());
            AudioBuffer<float> buf (2, block);
            MidiBuffer midi;
            for (size_t pos = 0; pos < in.size(); pos += block)
            {
                auto n = (int) jmin ((size_t) block, in.size() - pos);
                buf.setSize (2, n, false, false, true);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < n; ++i)
                        buf.setSample (ch, i, in[pos + (size_t) i]);
                proc.processBlock (buf, midi);
                for (int i = 0; i < n; ++i)
                    out[pos + (size_t) i] = buf.getSample (0, i);
            }
            return out;
        }

        CF76AudioProcessor proc;
    };

    std::vector<float> sine (float freq, float rmsDb, double seconds)
    {
        std::vector<float> v ((size_t) (seconds * fs));
        auto amp = Decibels::decibelsToGain (rmsDb) * MathConstants<float>::sqrt2;
        for (size_t i = 0; i < v.size(); ++i)
            v[i] = amp * (float) std::sin (MathConstants<double>::twoPi * freq * (double) i / fs);
        return v;
    }

    float rmsDb (const std::vector<float>& v, size_t from, size_t to)
    {
        double s = 0.0;
        for (auto i = from; i < to; ++i) s += (double) v[i] * v[i];
        return Decibels::gainToDecibels ((float) std::sqrt (s / (double) jmax ((size_t) 1, to - from)), -200.0f);
    }

    float tail (const std::vector<float>& v, double seconds = 0.25)
    {
        return rmsDb (v, v.size() - (size_t) (seconds * fs), v.size());
    }

    // THD (%) of a 1 kHz tone from the last 8192 samples (Hann window, harmonics 2..9).
    float thdPercent (const std::vector<float>& v, float f0 = 1000.0f)
    {
        constexpr int order = 13, size = 1 << order;
        dsp::FFT fft (order);
        std::vector<float> data (2 * size, 0.0f);
        auto start = v.size() - size;
        for (int i = 0; i < size; ++i)
            data[(size_t) i] = v[start + (size_t) i] * 0.5f * (1.0f - std::cos (MathConstants<float>::twoPi * (float) i / (float) (size - 1)));
        fft.performFrequencyOnlyForwardTransform (data.data());

        auto power = [&] (float f)
        {
            auto bin = (int) std::round (f * (float) size / (float) fs);
            double p = 0.0;
            for (int b = bin - 3; b <= bin + 3; ++b)
                if (b > 0 && b < size / 2) p += (double) data[(size_t) b] * data[(size_t) b];
            return p;
        };
        double fund = power (f0), harm = 0.0;
        for (int h = 2; h <= 9; ++h) harm += power (f0 * (float) h);
        return (float) (100.0 * std::sqrt (harm / fund));
    }

    struct Report
    {
        void line (const String& s) { text << s << newLine; }
        void check (bool ok, const String& s) { line ((ok ? "PASS  " : "FAIL  ") + s); fails += ok ? 0 : 1; }
        String text;
        int fails = 0;
    };
}

String runDspTests()
{
    Report r;
    r.line ("CF-76 DSP report (fs 48 kHz, analog off unless stated)");
    r.line ("");

    // 1. Unity gain at defaults, below threshold.
    {
        Rig rig;
        auto out = rig.run (sine (1000.0f, -30.0f, 1.0));
        auto g = tail (out) - (-30.0f);
        r.line ("Latency: " + String (rig.proc.getLatencySamples()) + " samples");
        r.check (std::abs (g) < 0.5f, "unity at default knobs, -30 dBFS tone: " + String (g, 2) + " dB");
    }

    // 2. Static curves.
    {
        r.line ("");
        r.line ("Static curve (1 kHz tone, default Input/Output): out - in (dB)");
        const char* names[] = { "4:1", "8:1", "12:1", "20:1", "All" };
        const float levels[] = { -36, -30, -26, -22, -18, -14, -10, -6, -2 };
        String header ("  in RMS dBFS:");
        for (auto l : levels) header << String (l).paddedLeft (' ', 7);
        r.line (header);
        float slopes[5] {};
        for (int ri = 0; ri < 5; ++ri)
        {
            String row = "  " + String (names[ri]).paddedRight (' ', 12);
            float outs[9] {};
            for (int li = 0; li < 9; ++li)
            {
                Rig rig;
                rig.set (ParamIDs::ratio, (float) ri);
                auto out = rig.run (sine (1000.0f, levels[li], 1.2));
                outs[li] = tail (out);
                row << String (outs[li] - levels[li], 1).paddedLeft (' ', 7);
            }
            slopes[ri] = (outs[8] - outs[6]) / (levels[8] - levels[6]);
            r.line (row + "   slope at top " + String (slopes[ri], 3));
        }
        r.check (slopes[0] > 0.18f && slopes[0] < 0.4f, "4:1 slope ~0.25 above knee");
        r.check (slopes[3] < 0.12f, "20:1 slope < 0.12 above knee");
        r.check (slopes[4] < 0.0f && slopes[4] > -0.3f, "All mode over-compresses gently (slope between -0.3 and 0)");
        r.check (slopes[1] > 0.09f && slopes[1] < 0.16f && slopes[2] > 0.05f && slopes[2] < 0.11f, "8:1 and 12:1 slopes match their ratios");
    }

    // 3. Ballistics, measured on the compressor core at 96 kHz with a DC step (sample-accurate).
    {
        r.line ("");
        r.line ("Ballistics (core at 96 kHz, DC step to ~10 dB GR at 4:1 unless stated)");

        struct Step { float attackMs, releaseMs, grDb; };
        auto step = [] (FetCompressor::Settings s, int channels, float amp, double onSec, double offSec)
        {
            FetCompressor c;
            c.prepare (96000.0, channels);
            c.setSettings (s);
            c.reset();
            AudioBuffer<float> b (channels, 1);
            std::vector<float> env;
            auto total = (int) ((onSec + offSec) * 96000.0), on = (int) (onSec * 96000.0);
            for (int i = 0; i < total; ++i)
            {
                for (int ch = 0; ch < channels; ++ch) b.setSample (ch, 0, i < on ? amp : 0.0f);
                dsp::AudioBlock<float> blk (b);
                c.process (blk);
                env.push_back (c.currentGainReductionDb());
            }
            const float grOn = env[(size_t) on - 1];
            Step res { -1.0f, -1.0f, grOn };
            for (int i = 0; i < on; ++i)
                if (env[(size_t) i] <= 0.63f * grOn) { res.attackMs = (float) i / 96.0f; break; }
            for (int i = on; i < total; ++i)
                if (env[(size_t) i] >= 0.37f * grOn) { res.releaseMs = (float) (i - on) / 96.0f; break; }
            return res;
        };

        FetCompressor::Settings base;
        base.analog = Analog::off;
        const float amp = Decibels::decibelsToGain (-4.7f);

        for (float knob : { 1.0f, 4.0f, 7.0f })
        {
            auto s = base;
            s.attackKnob = s.releaseKnob = knob;
            auto mono = step (s, 1, amp, 0.02, 2.0), stereo = step (s, 2, amp, 0.02, 2.0);
            r.line ("  knob " + String (knob, 0) + ": attack mono " + String (mono.attackMs * 1000.0f, 0) + " us, stereo "
                    + String (stereo.attackMs * 1000.0f, 0) + " us; release (after 20 ms burst) "
                    + String (mono.releaseMs, 0) + " ms; GR " + String (mono.grDb, 1) + " dB");
            if (knob == 1.0f)
            {
                r.check (std::abs (mono.attackMs - 0.8f) < 0.2f, "slowest attack ~800 us (hardware spec)");
                r.check (std::abs (mono.releaseMs - 1100.0f) < 330.0f, "slowest release ~1.1 s (hardware spec)");
            }
            if (knob == 7.0f)
            {
                r.check (mono.attackMs > 0.010f && mono.attackMs < 0.032f, "fastest attack ~20 us mono (hardware spec)");
                r.check (stereo.attackMs > 0.030f && stereo.attackMs < 0.055f, "stereo link doubles fastest attack to ~40 us");
                r.check (std::abs (mono.releaseMs - 50.0f) < 15.0f, "fastest release ~50 ms after a transient (hardware spec)");
            }
        }

        auto s4 = base, s20 = base;
        s20.ratio = Ratio::r20;
        auto a4 = step (s4, 1, Decibels::decibelsToGain (-4.7f), 0.02, 0.1);
        auto a20 = step (s20, 1, Decibels::decibelsToGain (2.0f), 0.02, 0.1);
        r.check (a20.attackMs > a4.attackMs, "attack slower at 20:1 (" + String (a20.attackMs * 1000.0f, 0)
                 + " us) than 4:1 (" + String (a4.attackMs * 1000.0f, 0) + " us)");

        for (auto rev : { Revision::blacky, Revision::bluey })
        {
            auto s = base;
            s.revision = rev;
            auto shortBurst = step (s, 1, amp, 0.02, 3.0), longBurst = step (s, 1, amp, 3.0, 3.0);
            r.line (String ("  ") + (rev == Revision::bluey ? "Bluey " : "Blacky") + " program-dependent release: after 20 ms "
                    + String (shortBurst.releaseMs, 0) + " ms, after 3 s " + String (longBurst.releaseMs, 0) + " ms");
            r.check (longBurst.releaseMs > 1.8f * shortBurst.releaseMs, "sustained compression slows the release");
        }
    }

    // 4. Distortion.
    {
        r.line ("");
        float thdGr[2] {}, thdClip[2] {};
        for (int rev : { 1, 0 })
        {
            for (bool off : { true, false })
            {
                Rig rig;
                rig.set (ParamIDs::revision, (float) rev);
                rig.set (ParamIDs::compOff, off ? 1.0f : 0.0f);
                if (! off) rig.set (ParamIDs::input, -20.0f); // ~10 dB of gain reduction
                auto out = rig.run (sine (1000.0f, -18.0f, 1.0));
                auto thd = thdPercent (out);
                if (! off) thdGr[rev] = thd;
                r.line (String (rev == 0 ? "Bluey " : "Blacky") + (off ? "  Comp Off, 0 VU:  THD " : "  ~10 dB GR:      THD ")
                        + String (thd, 3) + " %   level " + String (tail (out), 1) + " dBFS");
            }

            // Hot input (+12 dB into the input transformer), no compression, output trimmed down.
            Rig rig;
            rig.set (ParamIDs::revision, (float) rev);
            rig.set (ParamIDs::compOff, 1.0f);
            rig.set (ParamIDs::input, -12.0f);
            rig.set (ParamIDs::output, -36.0f);
            thdClip[rev] = thdPercent (rig.run (sine (1000.0f, -18.0f, 1.0)));
            r.line (String (rev == 0 ? "Bluey " : "Blacky") + "  hot input, Comp Off: THD " + String (thdClip[rev], 3) + " %");
        }
        r.check (thdGr[0] >= 2.0f * thdGr[1], "pre-LN Bluey has >= 2x the FET distortion of Blacky under GR");
        r.check (thdClip[0] >= 1.5f * thdClip[1], "Bluey input stage clips earlier (Rev A/B input clipping)");

        Rig hot;
        hot.set (ParamIDs::compOff, 1.0f);
        hot.set (ParamIDs::output, 0.0f);
        auto out = hot.run (sine (1000.0f, -18.0f, 1.0));
        auto thd = thdPercent (out);
        r.check (thd <= 0.5f, "Blacky at +22 dBm out (" + String (tail (out), 1) + " dBFS RMS): THD " + String (thd, 3)
                 + " % <= 0.5 % (hardware spec)");

        // Frequency response (Comp Off, low level): spec 20 Hz - 20 kHz +/- 1 dB.
        auto response = [] (float f)
        {
            Rig rg;
            rg.set (ParamIDs::compOff, 1.0f);
            return tail (rg.run (sine (f, -40.0f, 1.5))) + 40.0f;
        };
        auto f20 = response (20.0f), f1k = response (1000.0f), f20k = response (20000.0f);
        r.check (std::abs (f20 - f1k) <= 1.0f && std::abs (f20k - f1k) <= 1.0f,
                 "frequency response 20 Hz " + String (f20 - f1k, 2) + " dB, 20 kHz " + String (f20k - f1k, 2) + " dB (spec +/- 1 dB)");
    }

    // 4b. All-buttons attack lag: initial transients poke through before the slam.
    {
        auto overshoot = [] (Ratio ratio)
        {
            Rig rig;
            rig.set (ParamIDs::ratio, (float) ratio);
            rig.set (ParamIDs::attack, 7.0f);
            std::vector<float> in ((size_t) (0.2 * fs), 0.0f);
            auto burst = sine (1000.0f, -6.0f, 0.5);
            in.insert (in.end(), burst.begin(), burst.end());
            auto out = rig.run (in);
            const auto on = (size_t) (0.2 * fs) + (size_t) rig.proc.getLatencySamples();
            float early = 0.0f, settled = 0.0f;
            for (size_t i = on; i < on + 96; ++i) early = jmax (early, std::abs (out[i]));
            for (size_t i = out.size() - 4800; i < out.size(); ++i) settled = jmax (settled, std::abs (out[i]));
            return Decibels::gainToDecibels (early / settled);
        };
        auto o20 = overshoot (Ratio::r20), oAll = overshoot (Ratio::all);
        r.line ("");
        r.check (oAll > o20 + 3.0f, "All-buttons lets the initial transient through: overshoot " + String (oAll, 1)
                 + " dB vs " + String (o20, 1) + " dB at 20:1");
    }

    // 5. Noise and hum floor (silence in).
    {
        r.line ("");
        std::vector<float> silence ((size_t) fs, 0.0f);
        for (int rev : { 1, 0 })
            for (int a : { 0, 1, 2 })
            {
                Rig rig;
                rig.set (ParamIDs::revision, (float) rev);
                rig.set (ParamIDs::analog, (float) a);
                auto out = rig.run (silence);
                r.line (String (rev == 0 ? "Bluey " : "Blacky") + "  Analog " + String (a == 0 ? "Off " : a == 1 ? "50Hz" : "60Hz")
                        + ": floor " + String (tail (out, 0.5), 1) + " dBFS");
                if (a == 0) r.check (tail (out, 0.5) < -150.0f, "Analog Off is silent");
            }
    }

    // 6. Mix = 0 must be the latency-aligned dry signal (null test), Mix 50 must stay flat.
    {
        r.line ("");
        Rig rig;
        rig.set (ParamIDs::mix, 0.0f);
        Random rnd (7);
        std::vector<float> noise ((size_t) fs);
        for (auto& x : noise) x = (rnd.nextFloat() * 2.0f - 1.0f) * 0.25f;
        auto out = rig.run (noise);
        const int lat = rig.proc.getLatencySamples();
        float maxDiff = 0.0f;
        for (size_t i = 24000; i < noise.size(); ++i)
            maxDiff = jmax (maxDiff, std::abs (out[i] - noise[i - (size_t) lat]));
        r.check (maxDiff < 1.0e-6f, "Mix 0% nulls against latency-aligned input (max diff " + String (maxDiff, 8) + ")");

        float lo, hi;
        {
            Rig a; a.set (ParamIDs::mix, 50.0f); a.set (ParamIDs::compOff, 1.0f);
            lo = tail (a.run (sine (100.0f, -40.0f, 1.0))) + 40.0f;
        }
        {
            Rig b; b.set (ParamIDs::mix, 50.0f); b.set (ParamIDs::compOff, 1.0f);
            hi = tail (b.run (sine (10000.0f, -40.0f, 1.0))) + 40.0f;
        }
        r.check (std::abs (lo - hi) < 0.5f, "Mix 50% is flat (100 Hz " + String (lo, 2) + " dB, 10 kHz " + String (hi, 2) + " dB)");
    }

    // 7. Auto Makeup curve.
    {
        r.line ("");
        String row ("Auto Makeup: Output move vs Input (4:1):");
        for (float in : { -48.0f, -36.0f, -30.0f, -24.0f, -18.0f, -12.0f, -6.0f, 0.0f })
            row << "  " << String (in, 0) << "->" << String (-(CF76AudioProcessor::referenceOutputDb (in, Ratio::r4)
                                                        - CF76AudioProcessor::referenceOutputDb (-30.0f, Ratio::r4)), 1);
        r.line (row);
        auto low = CF76AudioProcessor::referenceOutputDb (-46.0f, Ratio::r4) - CF76AudioProcessor::referenceOutputDb (-48.0f, Ratio::r4);
        auto high = CF76AudioProcessor::referenceOutputDb (0.0f, Ratio::r4) - CF76AudioProcessor::referenceOutputDb (-2.0f, Ratio::r4);
        r.check (low > 1.9f && high < 0.6f, "makeup follows 1:1 below threshold, flattens once compressing");
    }

    // 8. CPU: 10 s of stereo audio at 48 kHz, heavy compression.
    {
        Rig rig;
        rig.set (ParamIDs::analog, (float) Analog::hz60);
        rig.set (ParamIDs::ratio, (float) Ratio::all);
        auto in = sine (440.0f, -10.0f, 10.0);
        auto t0 = Time::getMillisecondCounterHiRes();
        rig.run (in);
        auto ms = Time::getMillisecondCounterHiRes() - t0;
        r.line ("");
        r.line ("CPU: 10 s of stereo audio processed in " + String (ms, 0) + " ms (" + String (10000.0 / ms, 0)
                + "x realtime, ~" + String (100.0 * ms / 10000.0, 2) + "% of one core)");
    }

    r.line ("");
    r.line (r.fails == 0 ? "ALL CHECKS PASSED" : String (r.fails) + " CHECK(S) FAILED");
    return r.text;
}
