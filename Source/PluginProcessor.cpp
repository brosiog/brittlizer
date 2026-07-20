/*
  ==============================================================================

   BrittLizer — Plugin Processor implementation
   Full 5-module DSP chain: Bit Crusher → Decimator → Distortion → Noise → Output.

   Realtime-safe: no heap allocations in processBlock, parameter reads via
   atomic pointers, smoothed parameter transitions.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BrittLizerAudioProcessor::BrittLizerAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

BrittLizerAudioProcessor::~BrittLizerAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BrittLizerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addBool = [&](const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { id, 1 }, name, def));
    };

    auto addFloat = [&](const char* id, const char* name,
                        juce::NormalisableRange<float> range, float def)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def));
    };

    auto addIntChoice = [&](const char* id, const char* name,
                            juce::NormalisableRange<float> range, float def,
                            const juce::StringArray& choices)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, 1 }, name, choices, static_cast<int>(def)));
    };

    // ── Bit Crusher ──────────────────────────────────────────────────────────
    addFloat   (ParamID::bitDepth,   ParamName::bitDepth,   ParamRange::bitDepth,   ParamDefault::bitDepth);
    addBool    (ParamID::bitEnabled, ParamName::bitEnabled, ParamDefault::bitEnabled);

    // ── Decimator ─────────────────────────────────────────────────────────────
    addFloat   (ParamID::sampRate,   ParamName::sampRate,   ParamRange::sampRate,   ParamDefault::sampRate);
    addBool    (ParamID::aaPre,      ParamName::aaPre,      ParamDefault::aaPre);
    addBool    (ParamID::aaPost,     ParamName::aaPost,     ParamDefault::aaPost);
    addBool    (ParamID::decEnabled, ParamName::decEnabled, ParamDefault::decEnabled);

    // ── Distortion ────────────────────────────────────────────────────────────
    addIntChoice (ParamID::distType,       ParamName::distType,
                  {}, ParamDefault::distType,       { "Hard Clip", "Foldback", "Asymmetric" });
    addFloat     (ParamID::distDrive,      ParamName::distDrive,      ParamRange::distDrive,      ParamDefault::distDrive);
    addFloat     (ParamID::distThreshold,  ParamName::distThreshold,  ParamRange::distThreshold,  ParamDefault::distThreshold);
    addIntChoice (ParamID::distOversample, ParamName::distOversample,
                  {}, ParamDefault::distOversample, { "Off", "2x", "4x" });
    addBool      (ParamID::distEnabled,    ParamName::distEnabled,    ParamDefault::distEnabled);

    // ── Noise ─────────────────────────────────────────────────────────────────
    addIntChoice (ParamID::noiseMode,      ParamName::noiseMode,
                  {}, ParamDefault::noiseMode,    { "TPDF", "Pink", "Brown", "White", "Crackle" });
    addFloat     (ParamID::noiseLevel,     ParamName::noiseLevel,     ParamRange::noiseLevel,     ParamDefault::noiseLevel);
    addFloat     (ParamID::popRate,        ParamName::popRate,        ParamRange::popRate,        ParamDefault::popRate);
    addFloat     (ParamID::popIntensity,   ParamName::popIntensity,   ParamRange::popIntensity,   ParamDefault::popIntensity);
    addBool      (ParamID::noiseEnabled,   ParamName::noiseEnabled,   ParamDefault::noiseEnabled);

    // ── Output ────────────────────────────────────────────────────────────────
    addIntChoice (ParamID::filterType, ParamName::filterType,
                  {}, ParamDefault::filterType, { "Off", "Low Pass", "Band Pass", "High Pass" });
    addFloat     (ParamID::filterFreq, ParamName::filterFreq, ParamRange::filterFreq, ParamDefault::filterFreq);
    addFloat     (ParamID::filterQ,    ParamName::filterQ,    ParamRange::filterQ,    ParamDefault::filterQ);
    addFloat     (ParamID::mix,        ParamName::mix,        ParamRange::mix,        ParamDefault::mix);
    addFloat     (ParamID::outputGain, ParamName::outputGain, ParamRange::outputGain, ParamDefault::outputGain);

    return layout;
}

//==============================================================================
void BrittLizerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec
    {
        sampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        static_cast<juce::uint32> (getTotalNumInputChannels())
    };

    // ── Cache raw parameter pointers ──────────────────────────────────────────
    auto cache = [&](const char* id) -> std::atomic<float>*
    {
        return apvts.getRawParameterValue (id);
    };

    bitDepthParam       = cache (ParamID::bitDepth);
    bitEnabledParam     = cache (ParamID::bitEnabled);
    sampRateParam       = cache (ParamID::sampRate);
    aaPreParam          = cache (ParamID::aaPre);
    aaPostParam         = cache (ParamID::aaPost);
    decEnabledParam     = cache (ParamID::decEnabled);
    distTypeParam       = cache (ParamID::distType);
    distDriveParam      = cache (ParamID::distDrive);
    distThresholdParam  = cache (ParamID::distThreshold);
    distOversampleParam = cache (ParamID::distOversample);
    distEnabledParam    = cache (ParamID::distEnabled);
    noiseModeParam      = cache (ParamID::noiseMode);
    noiseLevelParam     = cache (ParamID::noiseLevel);
    popRateParam        = cache (ParamID::popRate);
    popIntensityParam   = cache (ParamID::popIntensity);
    noiseEnabledParam   = cache (ParamID::noiseEnabled);
    filterTypeParam     = cache (ParamID::filterType);
    filterFreqParam     = cache (ParamID::filterFreq);
    filterQParam        = cache (ParamID::filterQ);
    mixParam            = cache (ParamID::mix);
    outputGainParam     = cache (ParamID::outputGain);

    // ── Prepare DSP modules ───────────────────────────────────────────────────
    bitCrusher.prepare (spec);
    decimator.prepare  (spec);
    distortion.prepare (spec);
    noiseGen.prepare   (spec);
    outputStage.prepare (spec);

    // ── Pre-allocate dry buffer ───────────────────────────────────────────────
    dryBuffer.setSize (static_cast<int> (spec.numChannels),
                       static_cast<int> (spec.maximumBlockSize));
}

void BrittLizerAudioProcessor::releaseResources()
{
    bitCrusher.reset();
    decimator.reset();
    distortion.reset();
    noiseGen.reset();
    outputStage.reset();
}

//==============================================================================
void BrittLizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    // Clear any output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // ── Capture dry signal before any processing ─────────────────────────────
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ══════════════════════════════════════════════════════════════════════════
    //  Pipeline: Bit Crusher → Decimator → Distortion → Noise → Output
    // ══════════════════════════════════════════════════════════════════════════

    // ── 1. Bit Crusher ───────────────────────────────────────────────────────
    {
        const float bitDepth = bitDepthParam->load (std::memory_order_relaxed);
        const bool  enabled  = bitEnabledParam->load (std::memory_order_relaxed) >= 0.5f;

        bitCrusher.setBitDepth (bitDepth);
        bitCrusher.setBypassed (! enabled);
        bitCrusher.processBlock (buffer);
    }

    // ── 2. Decimator ─────────────────────────────────────────────────────────
    {
        const float targetRate = sampRateParam->load (std::memory_order_relaxed);
        const bool  preAA      = aaPreParam->load (std::memory_order_relaxed) >= 0.5f;
        const bool  postAA     = aaPostParam->load (std::memory_order_relaxed) >= 0.5f;
        const bool  enabled    = decEnabledParam->load (std::memory_order_relaxed) >= 0.5f;

        decimator.setSampleRate (targetRate);
        decimator.setPreAntiAlias (preAA);
        decimator.setPostAntiAlias (postAA);
        decimator.setBypassed (! enabled);
        decimator.processBlock (buffer);
    }

    // ── 3. Distortion ────────────────────────────────────────────────────────
    {
        const int   distType       = static_cast<int> (distTypeParam->load (std::memory_order_relaxed));
        const float drive          = distDriveParam->load (std::memory_order_relaxed);
        const float threshold      = distThresholdParam->load (std::memory_order_relaxed);
        const int   oversampleMode = static_cast<int> (distOversampleParam->load (std::memory_order_relaxed));
        const bool  enabled        = distEnabledParam->load (std::memory_order_relaxed) >= 0.5f;

        distortion.setDistortionType (distType);
        distortion.setDrive (drive);
        distortion.setThreshold (threshold);
        distortion.setOversampleMode (oversampleMode);
        distortion.setBypassed (! enabled);
        distortion.processBlock (buffer);
    }

    // ── 4. Noise ─────────────────────────────────────────────────────────────
    {
        const int   noiseMode     = static_cast<int> (noiseModeParam->load (std::memory_order_relaxed));
        const float level         = noiseLevelParam->load (std::memory_order_relaxed);
        const float rate          = popRateParam->load (std::memory_order_relaxed);
        const float intensity     = popIntensityParam->load (std::memory_order_relaxed);
        const bool  enabled       = noiseEnabledParam->load (std::memory_order_relaxed) >= 0.5f;

        noiseGen.setNoiseMode (noiseMode);
        noiseGen.setLevel (level);
        noiseGen.setPopRate (rate);
        noiseGen.setPopIntensity (intensity);
        noiseGen.setBypassed (! enabled);
        noiseGen.processBlock (buffer);
    }

    // ── 5. Output Stage (Filter + Mix + Gain) ─────────────────────────────────
    {
        const int   filterType = static_cast<int> (filterTypeParam->load (std::memory_order_relaxed));
        const float filterFreq = filterFreqParam->load (std::memory_order_relaxed);
        const float filterQ    = filterQParam->load (std::memory_order_relaxed);
        const float mix        = mixParam->load (std::memory_order_relaxed);
        const float outputGain = outputGainParam->load (std::memory_order_relaxed);

        outputStage.setFilterType (filterType);
        outputStage.setFilterFreq (filterFreq);
        outputStage.setFilterQ (filterQ);
        outputStage.setMix (mix);
        outputStage.setOutputGain (outputGain);
        outputStage.processBlock (buffer, dryBuffer);
    }
}

//==============================================================================
juce::AudioProcessorEditor* BrittLizerAudioProcessor::createEditor()
{
    return new BrittLizerAudioProcessorEditor (*this);
}

//==============================================================================
void BrittLizerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, false);
    apvts.state.writeToStream (mos);
}

void BrittLizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
        apvts.state = tree;
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BrittLizerAudioProcessor();
}
