#include "Parameters.h"

using namespace juce;

static String formatAttenuation (float v, int)
{
    return v <= minusInfDb ? String ("-inf") : String (v, 1) + " dB";
}

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    // Knob scale is evenly spaced in angle but not in dB (labels: inf 48 36 30 24 18 12 6 0),
    // so the normalised value follows the knob angle piecewise-linearly.
    auto attenuation = [] (const char* id, const char* name, float def)
    {
        static constexpr std::array<float, 9> marks { minusInfDb, -48.0f, -36.0f, -30.0f, -24.0f, -18.0f, -12.0f, -6.0f, 0.0f };
        constexpr float seg = 1.0f / (float) (marks.size() - 1);

        NormalisableRange<float> range (
            minusInfDb, 0.0f,
            [] (float, float, float t)
            {
                auto i = jlimit (0, (int) marks.size() - 2, (int) (t / seg));
                return jmap ((t - (float) i * seg) / seg, marks[(size_t) i], marks[(size_t) i + 1]);
            },
            [] (float, float, float v)
            {
                for (size_t i = 0; i + 1 < marks.size(); ++i)
                    if (v <= marks[i + 1])
                        return ((float) i + jmax (0.0f, (v - marks[i]) / (marks[i + 1] - marks[i]))) * seg;
                return 1.0f;
            },
            [] (float, float, float v) { return jlimit (minusInfDb, 0.0f, std::round (v * 10.0f) / 10.0f); });

        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, range, def,
            AudioParameterFloatAttributes().withStringFromValueFunction (formatAttenuation));
    };

    auto timeKnob = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (1.0f, 7.0f, 0.01f), def);
    };

    layout.add (attenuation (ParamIDs::input,  "Input",  -30.0f),
                attenuation (ParamIDs::output, "Output", -18.0f),
                timeKnob (ParamIDs::attack,  "Attack",  3.0f),
                timeKnob (ParamIDs::release, "Release", 4.0f),
                std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::ratio, 1 }, "Ratio",
                                                        StringArray { "4:1", "8:1", "12:1", "20:1", "All" }, 0),
                std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::compOff, 1 }, "Comp Off", false),
                std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::revision, 1 }, "Revision",
                                                        StringArray { "Bluey", "Blacky" }, 1),
                std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::analog, 1 }, "Analog",
                                                        StringArray { "Off", "50Hz", "60Hz" }, 2),
                std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::meter, 1 }, "Meter",
                                                        StringArray { "GR", "IN", "OUT" }, 0),
                std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f),
                std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::trim, 1 }, "Trim",
                                                       NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f),
                std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::autoMakeup, 1 }, "Auto Makeup", false));

    return layout;
}
