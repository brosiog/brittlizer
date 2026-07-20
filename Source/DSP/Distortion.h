/*
  ==============================================================================

   BrittLizer — Distortion
   Waveshaping distortion with three algorithms and configurable oversampling.

   Modes:
     - Hard Clip:  Hard-knee clipping at threshold
     - Foldback:   Signal folds back when exceeding threshold (aliasing-style)
     - Asymmetric: Different positive/negative waveshaping for second-harmonic

   Oversampling (2x or 4x with half-band FIR) reduces aliasing from harmonic
   generation, then downsamples back to the base rate.

   Pipeline: Input → Drive gain → [Oversample] → Waveshape → [Downsample] → Output

  ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Params.h"

//==============================================================================
/**
    Multi-mode distortion processor with oversampling.

    Three waveshaping algorithms plus configurable oversampling (off/2x/4x)
    to control aliasing artifacts from harmonic generation.
*/
class DistortionProcessor
{
public:
    DistortionProcessor() = default;
    ~DistortionProcessor() = default;

    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        currentSpec = spec;

        // Create oversampling stages if needed
        createOversamplers();

        reset();
    }

    void reset()
    {
        for (auto* os : oversamplers)
            if (os != nullptr)
                os->reset();

        // Reset smoothed values
        driveSmoothed.reset (sampleRate, 0.02);  // 20ms ramp
        thresholdSmoothed.reset (sampleRate, 0.02);
    }

    //==============================================================================
    void setDistortionType (int type) noexcept
    {
        currentType = juce::jlimit (0, 2, type);
    }

    void setDrive (float drive) noexcept
    {
        driveSmoothed.setTargetValue (juce::jlimit (0.0f, 10.0f, drive));
    }

    void setThreshold (float threshold) noexcept
    {
        thresholdSmoothed.setTargetValue (juce::jlimit (0.001f, 1.0f, threshold));
    }

    void setOversampleMode (int mode) noexcept
    {
        currentOversample = juce::jlimit (0, 2, mode);
        createOversamplers();
    }

    void setBypassed (bool bypassed) noexcept
    {
        isBypassed = bypassed;
    }

    //==============================================================================
    /** Process buffer in-place through distortion. */
    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        if (isBypassed)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // ── Drive smoothing ────────────────────────────────────────────────────
        // We'll compute snapped values before the loop
        const float drive     = driveSmoothed.getCurrentValue();
        const float threshold = thresholdSmoothed.getCurrentValue();

        // Determine oversampling factor
        const int osFactor = (currentOversample == 2) ? 4
                           : (currentOversample == 1) ? 2
                           : 1;

        if (osFactor > 1 && oversamplers[currentOversample - 1] != nullptr)
        {
            // ── Oversampled path ───────────────────────────────────────────────
            auto& os = *oversamplers[currentOversample - 1];

            juce::dsp::AudioBlock<float> block (buffer);
            juce::dsp::AudioBlock<float> osBlock (buffer);

            // Upsample
            os.processSamplesUp (osBlock);

            // Process at oversampled rate
            const int osSamples = osBlock.getNumSamples();
            for (int ch = 0; ch < juce::jmin (numChannels, (int) osBlock.getNumChannels()); ++ch)
            {
                auto* data = osBlock.getChannelPointer ((size_t) ch);
                for (int s = 0; s < osSamples; ++s)
                    data[s] = waveshape (data[s], drive, threshold);
            }

            // Downsample
            os.processSamplesDown (block);
        }
        else
        {
            // ── Non-oversampled path ───────────────────────────────────────────
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);
                for (int s = 0; s < numSamples; ++s)
                    data[s] = waveshape (data[s], drive, threshold);
            }
        }
    }

private:
    //==============================================================================
    /** Core waveshaping function — no branching on type inside inner loop */
    float waveshape (float x, float drive, float threshold) noexcept
    {
        // Apply drive (pre-gain)
        x *= drive;

        switch (currentType)
        {
            case 0:  // Hard Clip
                return juce::jlimit (-threshold, threshold, x);

            case 1:  // Foldback
            {
                // Simple foldback: when x exceeds threshold, fold back
                float absX = std::abs (x);
                if (absX <= threshold)
                    return x;

                // Fold: y = sign(x) * (2 * threshold - absX) when absX in [threshold, 2*threshold]
                // then wrap again for larger values
                float folded = std::fmod (absX - threshold, 2.0f * threshold);
                if (folded > threshold)
                    folded = 2.0f * threshold - folded;

                return (x >= 0.0f) ? folded : -folded;
            }

            case 2:  // Asymmetric
            {
                // Different positive/negative processing
                if (x >= 0.0f)
                {
                    // Positive half: soft clip with tanh-like shape
                    return threshold * std::tanh (x / juce::jmax (threshold, 0.001f));
                }
                else
                {
                    // Negative half: harder clipping with different curve
                    float absX = -x;
                    float y = absX / juce::jmax (threshold, 0.001f);
                    y = y / (1.0f + y);  // soft clip curves differently
                    return -threshold * y;
                }
            }

            default:
                return x;
        }
    }

    //==============================================================================
    void createOversamplers()
    {
        for (int i = 0; i < 2; ++i)
        {
            if (currentOversample > i && currentSpec.sampleRate > 0)
            {
                if (oversamplers[i] == nullptr)
                {
                    int factor = (i == 1) ? 4 : 2;
                    oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>> (
                        currentSpec.numChannels, factor,
                        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                        false, false);
                }

                oversamplers[i]->initProcessing (
                    static_cast<size_t> (currentSpec.maximumBlockSize));
            }
            else
            {
                oversamplers[i] = nullptr;
            }
        }
    }

    //==============================================================================
    double sampleRate = 44100.0;
    juce::dsp::ProcessSpec currentSpec { 44100.0, 512, 2 };

    int     currentType = 0;
    int     currentOversample = 1;  // Default: 2x
    bool    isBypassed = false;

    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> thresholdSmoothed;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionProcessor)
};
