/*
  ==============================================================================

   BrittLizer — Noise Generator
   Multi-mode noise generator for lo-fi texture, dither, and glitch effects.

   Modes:
     - TPDF:    Triangular Probability Density Function dither (sum of 2 uniform)
     - Pink:    Paul Kellet's V-V algorithm (octave-filtered noise)
     - Brown:   Integrate white noise (random walk, leaky)
     - White:   Uniform random [-1, 1]
     - Crackle: Random impulses at configurable rate

   Each mode can be mixed into the audio stream at a controllable level.

   Pipeline: Input → Add Noise (level-scaled) → Output

  ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <random>

//==============================================================================
/**
    Multi-mode noise generator for lo-fi audio processing.

    Generates dither noise (TPDF), colored noise (pink/brown/white), or
    impulse noise (crackle) and adds it to the signal at a controllable level.
*/
class NoiseGenerator
{
public:
    NoiseGenerator()
        : rng (std::random_device{}())
    {
    }

    ~NoiseGenerator() = default;

    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();

        // Pre-allocate buffer for crackle sample-and-hold
        crackleState.resize ((size_t) juce::nextPowerOfTwo (
            static_cast<int> (spec.maximumBlockSize)), false);
    }

    void reset()
    {
        // Pink noise state: Paul Kellet's V-V algorithm
        pinkState = {};
        std::fill (std::begin (pinkState.bins), std::end (pinkState.bins), 0.0f);
        pinkState.index = 0;

        // Brown noise state
        brownValue = 0.0f;

        // Crackle state
        crackleCounter = 0;
        crackleNextPop = 0;
    }

    //==============================================================================
    void setNoiseMode (int mode) noexcept
    {
        currentMode = juce::jlimit (0, 4, mode);
    }

    void setLevel (float level) noexcept
    {
        currentLevel = juce::jlimit (0.0f, 1.0f, level);
    }

    /** Set pop rate in Hz (only for crackle mode). */
    void setPopRate (float rateHz) noexcept
    {
        popRate = juce::jlimit (0.0f, 10.0f, rateHz);
        if (sampleRate > 0.0 && popRate > 0.0f)
            popInterval = static_cast<int> (sampleRate / popRate);
        else
            popInterval = 0;
    }

    /** Set pop intensity [0,1] (only for crackle mode). */
    void setPopIntensity (float intensity) noexcept
    {
        popIntensity = juce::jlimit (0.0f, 1.0f, intensity);
    }

    void setBypassed (bool bypassed) noexcept
    {
        isBypassed = bypassed;
    }

    //==============================================================================
    /** Add noise to buffer in-place at the configured level. */
    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        if (isBypassed || currentLevel <= 0.0f)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();

        switch (currentMode)
        {
            case 0:  // TPDF
                generateTpdf (buffer, numChannels, numSamples);
                break;

            case 1:  // Pink
                generatePink (buffer, numChannels, numSamples);
                break;

            case 2:  // Brown
                generateBrown (buffer, numChannels, numSamples);
                break;

            case 3:  // White
                generateWhite (buffer, numChannels, numSamples);
                break;

            case 4:  // Crackle
                generateCrackle (buffer, numChannels, numSamples);
                break;

            default:
                break;
        }
    }

private:
    //==============================================================================
    /** Uniform random in [-1, 1] */
    float uniformRandom() noexcept
    {
        return 2.0f * (static_cast<float> (rng() - rng.min()) /
                       static_cast<float> (rng.max() - rng.min())) - 1.0f;
    }

    //==============================================================================
    void addNoiseToBuffer (juce::AudioBuffer<float>& buffer,
                           const float* noise, int numChannels, int numSamples) noexcept
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int s = 0; s < numSamples; ++s)
                data[s] += currentLevel * noise[s];
        }
    }

    //==============================================================================
    /** TPDF dither: sum of 2 uniform random values, scaled to [-1, 1] */
    void generateTpdf (juce::AudioBuffer<float>& buffer,
                       int numChannels, int numSamples) noexcept
    {
        // Generate one channel of TPDF and reuse across channels
        for (int s = 0; s < numSamples; ++s)
            tpdfBuffer[s] = (uniformRandom() + uniformRandom()) * 0.5f;

        addNoiseToBuffer (buffer, tpdfBuffer, numChannels, numSamples);
    }

    //==============================================================================
    /** Pink noise: Paul Kellet's V-V refined algorithm */
    void generatePink (juce::AudioBuffer<float>& buffer,
                       int numChannels, int numSamples) noexcept
    {
        for (int s = 0; s < numSamples; ++s)
        {
            // Increment the index and count trailing zeros
            ++pinkState.index;
            int tz = countTrailingZeros (pinkState.index) & 7;  // mask to 0-7

            if (tz < 7)
            {
                // Update the bin: replace with new uniform random
                pinkState.bins[tz] = uniformRandom();
            }
            // tz = 7 means we hit a multiple of 128 — keep all bins (full update)

            // Sum all bins
            float sum = 0.0f;
            for (int b = 0; b < 7; ++b)
                sum += pinkState.bins[b];

            // Normalize to [-1, 1] (7 bins, each [-1,1], sum range [-7,7])
            pinkBuffer[s] = sum * (1.0f / 7.0f);
        }

        addNoiseToBuffer (buffer, pinkBuffer, numChannels, numSamples);
    }

    /** Count trailing zeros in an integer (GCC/Clang builtin). */
    static int countTrailingZeros (int v) noexcept
    {
        return (v == 0) ? 32 : __builtin_ctz (static_cast<unsigned> (v));
    }

    //==============================================================================
    /** Brown noise: random walk (leaky integrator) */
    void generateBrown (juce::AudioBuffer<float>& buffer,
                        int numChannels, int numSamples) noexcept
    {
        for (int s = 0; s < numSamples; ++s)
        {
            // Random step, leaky integration
            brownValue += uniformRandom() * 0.125f;
            brownValue *= 0.99f;  // Leaky integrator prevents DC drift
            brownValue = juce::jlimit (-1.0f, 1.0f, brownValue);
            brownBuffer[s] = brownValue;
        }

        addNoiseToBuffer (buffer, brownBuffer, numChannels, numSamples);
    }

    //==============================================================================
    /** White noise: uniform random */
    void generateWhite (juce::AudioBuffer<float>& buffer,
                        int numChannels, int numSamples) noexcept
    {
        for (int s = 0; s < numSamples; ++s)
            whiteBuffer[s] = uniformRandom();

        addNoiseToBuffer (buffer, whiteBuffer, numChannels, numSamples);
    }

    //==============================================================================
    /** Crackle: random impulses (sample-and-hold noise bursts) */
    void generateCrackle (juce::AudioBuffer<float>& buffer,
                          int numChannels, int numSamples) noexcept
    {
        // Fill per-sample crackle buffer
        for (int s = 0; s < numSamples; ++s)
        {
            --crackleCounter;

            if (crackleCounter <= 0)
            {
                // New pop
                crackleNextPop = (popInterval > 0)
                    ? static_cast<int> (popInterval * (0.5f + 0.5f * juce::Random::getSystemRandom().nextFloat()))
                    : 0;

                crackleCounter = crackleNextPop;

                // Pop amplitude (exponential distribution for crackly feel)
                float amp = juce::Random::getSystemRandom().nextFloat();
                amp = amp * amp * amp;  // Cube for sparse bursts
                crackleAmplitude = amp * popIntensity;
            }

            // Quick decay of the pop
            crackleAmplitude *= 0.8f;
            crackleBuffer[s] = (crackleAmplitude > 0.001f)
                ? crackleAmplitude * uniformRandom()
                : 0.0f;
        }

        addNoiseToBuffer (buffer, crackleBuffer, numChannels, numSamples);
    }

    //==============================================================================
    //  Noise-generation buffer storage (max block size pre-allocated)
    //==============================================================================
    static constexpr int maxBlockSize = 2048;
    float tpdfBuffer[maxBlockSize];
    float pinkBuffer[maxBlockSize];
    float brownBuffer[maxBlockSize];
    float whiteBuffer[maxBlockSize];
    float crackleBuffer[maxBlockSize];

    //==============================================================================
    //  Pink noise state (Paul Kellet's V-V algorithm — 7 bins)
    //==============================================================================
    struct PinkState
    {
        float bins[7] = { 0.0f };
        int   index = 0;
    };
    PinkState pinkState;

    //==============================================================================
    //  Brown noise state
    //==============================================================================
    float brownValue = 0.0f;

    //==============================================================================
    //  Crackle state
    //==============================================================================
    int   crackleCounter = 0;
    int   crackleNextPop = 0;
    float crackleAmplitude = 0.0f;

    //==============================================================================
    //  Parameters
    //==============================================================================
    double sampleRate = 44100.0;
    int    currentMode = 0;  // TPDF
    float  currentLevel = 0.0f;
    float  popRate = 0.0f;
    float  popIntensity = 0.5f;
    int    popInterval = 0;
    bool   isBypassed = false;

    // Random number generator
    std::mt19937 rng;

    // Stack-allocated storage
    juce::Array<float> crackleState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseGenerator)
};
