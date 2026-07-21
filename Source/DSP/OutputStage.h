/*
  ==============================================================================

   BrittLizer — Output Stage
   State Variable Filter + Dry/Wet Mix + Output Gain.

   The output stage is the final processing block in the chain. It provides
   a resonant multi-mode filter (LP/BP/HP), configurable dry/wet blend for
   the entire effect chain, and output gain make-up.

   Pipeline: Input → SVF Filter → Dry/Wet Mix → Output Gain → Output

  ==============================================================================
*/

#pragma once

#include <vector>
#include <juce_dsp/juce_dsp.h>
#include "../Params.h"

//==============================================================================
/**
    Output processing stage with resonant filter, dry/wet mix, and gain.

    The SVF filter uses JUCE's StateVariableTPTFilter for clean self-oscillating
    response. Dry/wet mix blends the original dry signal with the fully processed
    wet signal. Output gain applies final make-up/trim gain.
*/
class OutputStage
{
public:
    OutputStage() = default;
    ~OutputStage() = default;

    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Pre-allocate dry buffer for dry/wet mix
        dryBuffer.setSize (static_cast<int> (spec.numChannels),
                           static_cast<int> (spec.maximumBlockSize));

        // Set up SVF filter per channel
        svfFilters.clear();
        for (unsigned int ch = 0; ch < spec.numChannels; ++ch)
        {
            auto& filter = svfFilters.emplace_back (juce::dsp::StateVariableTPTFilter<float> {});
            filter.prepare (spec);
        }

        reset();
    }

    void reset()
    {
        for (auto& filter : svfFilters)
            filter.reset();

        mixSmoothed.reset (sampleRate, 0.02);  // 20ms ramp
        gainSmoothed.reset (sampleRate, 0.02);

        mixSmoothed.setCurrentAndTargetValue (mixSmoothed.getTargetValue());
        gainSmoothed.setCurrentAndTargetValue (gainSmoothed.getTargetValue());
    }

    //==============================================================================
    void setFilterType (int type) noexcept
    {
        currentFilterType = juce::jlimit (0, 3, type);

        for (auto& filter : svfFilters)
        {
            switch (currentFilterType)
            {
                case 1:  filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);  break;
                case 2:  filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass); break;
                case 3:  filter.setType (juce::dsp::StateVariableTPTFilterType::highpass); break;
                default: break;  // Off
            }
        }
    }

    void setFilterFreq (float freqHz) noexcept
    {
        float cutoff = juce::jlimit (20.0f, 20000.0f, freqHz);
        for (auto& filter : svfFilters)
            filter.setCutoffFrequency (cutoff);
    }

    void setFilterQ (float q) noexcept
    {
        float resonance = juce::jlimit (0.1f, 10.0f, q);
        for (auto& filter : svfFilters)
            filter.setResonance (resonance);
    }

    /** Set dry/wet mix [0, 1] — 0 = dry, 1 = wet. */
    void setMix (float mix) noexcept
    {
        mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, mix));
    }

    /** Set output gain in dB. */
    void setOutputGain (float gainDb) noexcept
    {
        float gainLin = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 24.0f, gainDb));
        gainSmoothed.setTargetValue (gainLin);
    }

    void setBypassed (bool bypassed) noexcept
    {
        isBypassed = bypassed;
    }

    //==============================================================================
    /** Process buffer in-place through filter, mix, and gain. */
    void processBlock (juce::AudioBuffer<float>& buffer,
                       const juce::AudioBuffer<float>& drySignal)
    {
        if (isBypassed)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // ── Filter ─────────────────────────────────────────────────────────────
        if (currentFilterType != 0)  // Filter type "Off"
        {
            for (int ch = 0; ch < juce::jmin (numChannels, (int) svfFilters.size()); ++ch)
            {
                auto block = juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) ch);
                juce::dsp::ProcessContextReplacing<float> context (block);
                svfFilters[(size_t) ch].process (context);
            }
        }

        // ── Dry/Wet Mix ────────────────────────────────────────────────────────
        if (mixSmoothed.isSmoothing() || mixSmoothed.getCurrentValue() < 1.0f)
        {
            // Smoothed mix — per-sample crossfade
            const int dryChannels = juce::jmin (numChannels, drySignal.getNumChannels());

            for (int ch = 0; ch < dryChannels; ++ch)
            {
                const auto* dry = drySignal.getReadPointer (ch);
                auto* wet = buffer.getWritePointer (ch);

                for (int s = 0; s < numSamples; ++s)
                {
                    const float mixVal = mixSmoothed.getNextValue();
                    wet[s] = dry[s] * (1.0f - mixVal) + wet[s] * mixVal;
                }
            }
        }

        // ── Output Gain ────────────────────────────────────────────────────────
        if (gainSmoothed.isSmoothing())
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                for (int s = 0; s < numSamples; ++s)
                    data[s] *= gainSmoothed.getNextValue();
            }
        }
        else
        {
            const float gain = gainSmoothed.getCurrentValue();
            if (gain != 1.0f)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                    buffer.applyGain (ch, 0, numSamples, gain);
            }
        }
    }

private:
    //==============================================================================
    double sampleRate = 44100.0;

    int   currentFilterType = 0;  // Off
    bool  isBypassed = false;

    juce::SmoothedValue<float> mixSmoothed { 1.0f };   // Fully wet by default
    juce::SmoothedValue<float> gainSmoothed { 1.0f };  // Unity gain

    // SVF filters (one per channel)
    std::vector<juce::dsp::StateVariableTPTFilter<float>> svfFilters;

    // Pre-allocated dry buffer
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputStage)
};
