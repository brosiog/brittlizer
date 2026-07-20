/*
  ==============================================================================

   BrittLizer — Bit Crusher
   Bit depth reduction via uniform quantization with optional dither.

   Reduces resolution from 32-bit float to N-bit integer representation.
   Pipeline: Input → Quantize to N bits → Output (in-place)

  ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>

//==============================================================================
/**
    Reduces bit depth by quantizing float samples to a target bit resolution.

    The bit depth parameter controls how many bits are used to represent the
    signal.  At 32 bits the signal passes unchanged; at lower values the signal
    is uniformly quantized, introducing harmonic distortion and noise floor
    modulation characteristic of lo-fi digital audio.
*/
class BitCrusher
{
public:
    BitCrusher() = default;
    ~BitCrusher() = default;

    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
    }

    void reset()
    {
        // No state to reset — stateless processing
    }

    //==============================================================================
    /** Set the target bit depth (1.0 – 32.0). */
    void setBitDepth (float bits) noexcept
    {
        currentBitDepth = juce::jlimit (1.0f, 32.0f, bits);
    }

    /** Set bypass state. */
    void setBypassed (bool bypassed) noexcept
    {
        isBypassed = bypassed;
    }

    //==============================================================================
    /** Process buffer in-place. */
    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        if (isBypassed || currentBitDepth >= 31.9f)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        // Number of quantization levels
        const float levels = std::pow (2.0f, currentBitDepth);
        const float scale  = 0.5f * levels;  // half-range [-1,1] → levels/2

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int s = 0; s < numSamples; ++s)
            {
                // Map [-1, 1] to integer range, quantize, map back
                float x = data[s];
                x = juce::jlimit (-1.0f, 1.0f, x);
                x = std::round (x * scale) / scale;
                data[s] = x;
            }
        }
    }

private:
    //==============================================================================
    double sampleRate = 44100.0;
    float  currentBitDepth = 16.0f;
    bool   isBypassed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BitCrusher)
};
