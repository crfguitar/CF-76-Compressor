#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

CF76AudioProcessor::CF76AudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMS", createParameterLayout())
{
    for (auto* id : { ParamIDs::input, ParamIDs::output, ParamIDs::autoMakeup })
        apvts.addParameterListener (id, this);
}

CF76AudioProcessor::~CF76AudioProcessor()
{
    for (auto* id : { ParamIDs::input, ParamIDs::output, ParamIDs::autoMakeup })
        apvts.removeParameterListener (id, this);
    cancelPendingUpdate();
}

bool CF76AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

FetCompressor::Settings CF76AudioProcessor::readSettings() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    FetCompressor::Settings s;
    s.inputDb = get (ParamIDs::input);
    s.outputDb = get (ParamIDs::output);
    s.attackKnob = get (ParamIDs::attack);
    s.releaseKnob = get (ParamIDs::release);
    s.ratio = (Ratio) (int) get (ParamIDs::ratio);
    s.compOff = get (ParamIDs::compOff) > 0.5f;
    s.revision = (Revision) (int) get (ParamIDs::revision);
    s.analog = (Analog) (int) get (ParamIDs::analog);
    return s;
}

void CF76AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = (size_t) jmax (1, getTotalNumInputChannels());

    // 2x oversampling for the nonlinear stages; integer latency so the dry path can be aligned exactly.
    oversampling = std::make_unique<dsp::Oversampling<float>> (numCh, 1, dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                                               true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    const auto latency = (int) std::round (oversampling->getLatencyInSamples());
    setLatencySamples (latency);

    comp.prepare (sampleRate * 2.0, (int) numCh);
    comp.setSettings (readSettings());
    comp.reset();

    dryDelay.prepare ({ sampleRate, (uint32) samplesPerBlock, (uint32) numCh });
    dryDelay.setMaximumDelayInSamples (jmax (16, latency + 1));
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();
    dryBuffer.setSize ((int) numCh, samplesPerBlock);

    wetGain.reset (sampleRate, 0.02);
    trimGain.reset (sampleRate, 0.02);
    wetGain.setCurrentAndTargetValue (apvts.getRawParameterValue (ParamIDs::mix)->load() * 0.01f);
    trimGain.setCurrentAndTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::trim)->load()));
}

void CF76AudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numCh = jmin (getTotalNumInputChannels(), dryBuffer.getNumChannels());
    const int n = buffer.getNumSamples();
    for (int ch = numCh; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (n > dryBuffer.getNumSamples())
        dryBuffer.setSize (dryBuffer.getNumChannels(), n, false, false, true);

    // Input metering and the latency-aligned dry copy.
    float peakIn = 0.0f;
    double sumInSq = 0.0;
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* x = buffer.getReadPointer (ch);
        auto* d = dryBuffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            peakIn = jmax (peakIn, std::abs (x[i]));
            sumInSq += x[i] * x[i];
            dryDelay.pushSample (ch, x[i]);
            d[i] = dryDelay.popSample (ch);
        }
    }

    // Wet path at 2x.
    comp.setSettings (readSettings());
    dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto up = oversampling->processSamplesUp (block);
    comp.process (up);
    oversampling->processSamplesDown (block);

    // Mix (dry/wet) then Trim.
    wetGain.setTargetValue (apvts.getRawParameterValue (ParamIDs::mix)->load() * 0.01f);
    trimGain.setTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::trim)->load()));
    float peakOut = 0.0f;
    double sumOutSq = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float w = wetGain.getNextValue(), t = trimGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto out = (dryBuffer.getSample (ch, i) * (1.0f - w) + buffer.getSample (ch, i) * w) * t;
            buffer.setSample (ch, i, out);
            peakOut = jmax (peakOut, std::abs (out));
            sumOutSq += out * out;
        }
    }

    const auto count = (double) jmax (1, n * numCh);
    meterInDb.store (Decibels::gainToDecibels (peakIn, -100.0f));
    meterOutDb.store (Decibels::gainToDecibels (peakOut, -100.0f));
    // In All-buttons mode the hardware GR meter "goes wild, often resting at maximum" (1176LN manual).
    const bool allButtons = (Ratio) (int) apvts.getRawParameterValue (ParamIDs::ratio)->load() == Ratio::all;
    meterGrDb.store (comp.takeMaxGainReductionDb() * (allButtons ? 1.8f : 1.0f));
    meterInRmsDb.store (Decibels::gainToDecibels ((float) std::sqrt (sumInSq / count), -100.0f));
    meterOutRmsDb.store (Decibels::gainToDecibels ((float) std::sqrt (sumOutSq / count), -100.0f));
    if (peakIn >= 1.0f)  overIn = true;
    if (peakOut >= 1.0f) overOut = true;
}

//==============================================================================
// Auto Makeup

float CF76AudioProcessor::referenceOutputDb (float inputDb, Ratio ratio, Revision revision)
{
    // A 0 VU sine (-18 dBFS RMS, -15 dBFS peak) through the input stage and the static curve.
    if (inputDb <= minusInfDb)
        return -100.0f;
    const float level = -15.0f + inputDb + FetCompressor::inputOffsetDb;
    return level + FetCompressor::staticGainReductionDb (FetCompressor::curveFor (ratio, revision), level);
}

void CF76AudioProcessor::parameterChanged (const String& id, float)
{
    if (applyingAutoMakeup)
        return;

    const bool on = apvts.getRawParameterValue (ParamIDs::autoMakeup)->load() > 0.5f;
    if (id == ParamIDs::input && on)
    {
        autoMakeupPending = true;
        triggerAsyncUpdate(); // move Output on the message thread
    }
    else
    {
        // Enabling Auto Makeup, or a manual Output change, sets the Input/Output offset to keep.
        anchorInputDb = apvts.getRawParameterValue (ParamIDs::input)->load();
        anchorOutputDb = apvts.getRawParameterValue (ParamIDs::output)->load();
    }
}

void CF76AudioProcessor::handleAsyncUpdate()
{
    if (! autoMakeupPending.exchange (false))
        return;

    const auto ratio = (Ratio) (int) apvts.getRawParameterValue (ParamIDs::ratio)->load();
    const float in = apvts.getRawParameterValue (ParamIDs::input)->load();
    const auto rev = (Revision) (int) apvts.getRawParameterValue (ParamIDs::revision)->load();
    const float delta = referenceOutputDb (in, ratio, rev) - referenceOutputDb (anchorInputDb, ratio, rev);
    const float target = jlimit (minusInfDb + 0.1f, 0.0f, anchorOutputDb - delta);

    if (auto* out = apvts.getParameter (ParamIDs::output))
    {
        const ScopedValueSetter<bool> guard (applyingAutoMakeup, true);
        out->beginChangeGesture();
        out->setValueNotifyingHost (out->convertTo0to1 (target));
        out->endChangeGesture();
    }
}

//==============================================================================

AudioProcessorEditor* CF76AudioProcessor::createEditor()
{
    return new CF76AudioProcessorEditor (*this);
}

void CF76AudioProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
    {
        presets.writeTo (*xml);
        copyXmlToBinary (*xml, dest);
    }
}

void CF76AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            presets.readFrom (*xml);
            xml->deleteAllChildElementsWithTagName ("PRESETS");
            apvts.replaceState (ValueTree::fromXml (*xml));
            undoManager.clearUndoHistory();
            anchorInputDb = apvts.getRawParameterValue (ParamIDs::input)->load();
            anchorOutputDb = apvts.getRawParameterValue (ParamIDs::output)->load();
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CF76AudioProcessor();
}
