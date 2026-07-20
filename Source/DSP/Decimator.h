/*
  ==============================================================================

   BrittLizer — Decimator
   Sample rate reduction with optional anti-alias filtering.

   Simulates the sound of low sample rates by dropping samples and applying
   zero-order hold reconstruction.  Optional pre-decimation low-pass filter
   prevents aliasing; optional post-reconstruction filter smooths the output.

   Pipeline: Input → [Anti-alias LPF] → Sample drop → [Post LPF] → Output

  ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

//==============================================================================
/**
    Sample rate reducer (decimator) with anti-aliasing options.

    Reduces the effective sample rate by periodically holding sample values.
    When anti-alias pre-filter is enabled, a low-pass filter at half the target
    frequency is applied before decimation.  When anti-alias post-filter is
    enabled, a reconstruction filter smooths the stairstep waveform.
*/
class Decimator
{
public:
    Decimator() = default;
    ~Decimator() = default;

    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();
    }

    void reset()
    {
        holdSample[0] = 0.0f;
        holdSample[1] = 0.0f;

        // Set up anti-alias pre-filter (2nd-order Butterworth LPF)
        preFilter.reset();
        preFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (
            sampleRate, 20000.0f);  // will be updated per-block

        // Set up post-reconstruction filter (2nd-order Butterworth LPF)
        postFilter.reset();
        postFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (
            sampleRate, 20000.0f);
    }

    //==============================================================================
    /** Set target sample rate in Hz. */
    void setSampleRate (float targetRate) noexcept
    {
        currentTargetRate = juce::jlimit (100.0f, 48000.0f, targetRate);
    }

    void setPreAntiAlias (bool enabled) noexcept
    {
        usePreAA = enabled;
    }

    void setPostAntiAlias (bool enabled) noexcept
    {
        usePostAA = enabled;
    }

    void setBypassed (bool bypassed) noexcept
    {
        isBypassed = bypassed;
    }

    //==============================================================================
    /** Process buffer in-place with sample rate reduction. */
    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        if (isBypassed)
            return;

        const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
        const int numSamples  = buffer.getNumSamples();
        const float step = static_cast<float> (currentTargetRate / sampleRate);

        if (step >= 0.999f)
            return;  // No reduction needed

        // ── Pre-decimation anti-alias filter ──────────────────────────────────
        if (usePreAA && step > 0.0f)
        {
            auto cutoff = juce::jmin (currentTargetRate * 0.45f, 20000.0f);
            *preFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
                sampleRate, cutoff);
            preFilter.reset();

            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> context (block);
            preFilter.process (context);
        }

        // ── Sample rate reduction (zero-order hold) ──────────────────────────
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            float phase = 0.0f;
            float held = holdSample[ch];

            for (int s = 0; s < numSamples; ++s)
            {
                phase += step;
                if (phase >= 1.0f)
                {
                    phase -= 1.0f;
                    held = data[s];
                }
                data[s] = held;
            }

            holdSample[ch] = held;
        }

        // ── Post-reconstruction low-pass filter ──────────────────────────────
        if (usePostAA && step > 0.0f)
        {
            auto cutoff = juce::jmin (currentTargetRate * 0.45f, 20000.0f);
            *postFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
                sampleRate, cutoff);
            postFilter.reset();

            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::ProcessContextReplacing<float> context (block);
            postFilter.process (context);
        }
    }

private:
    //==============================================================================
    double sampleRate = 44100.0;
    float  currentTargetRate = 44100.0f;
    float  holdSample[2] = { 0.0f, 0.0f };
    bool   usePreAA  = true;
    bool   usePostAA = false;
    bool   isBypassed = false;

    juce::dsp::IIR::Filter<float> preFilter;
    juce::dsp::IIR::Filter<float> postFilter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Decimator)
};
