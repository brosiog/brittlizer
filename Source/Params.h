/*
  ==============================================================================

   BrittLizer — Parameter definitions
   All parameter IDs, ranges, enums, and default values as constexpr structures.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

// ══════════════════════════════════════════════════════════════════════════════
//  Module enums (stored as integer parameters in APVTS)
// ══════════════════════════════════════════════════════════════════════════════

enum class DistortionType : int
{
    hardClip = 0,
    foldback = 1,
    asymmetric = 2
};

enum class OversampleMode : int
{
    off = 0,
    twice = 1,
    fourX = 2
};

enum class NoiseMode : int
{
    tpdf = 0,
    pink = 1,
    brown = 2,
    white = 3,
    crackle = 4
};

enum class FilterMode : int
{
    off = 0,
    lowPass = 1,
    bandPass = 2,
    highPass = 3
};

// ══════════════════════════════════════════════════════════════════════════════
//  Parameter IDs — used to construct juce::ParameterID in APVTS
// ══════════════════════════════════════════════════════════════════════════════

namespace ParamID
{
    // Bit Crusher
    inline constexpr const char* bitDepth   = "bitDepth";
    inline constexpr const char* bitEnabled = "bitEnabled";

    // Decimator
    inline constexpr const char* sampRate    = "sampRate";
    inline constexpr const char* aaPre       = "aaPre";
    inline constexpr const char* aaPost      = "aaPost";
    inline constexpr const char* decEnabled  = "decEnabled";

    // Distortion
    inline constexpr const char* distType        = "distType";
    inline constexpr const char* distDrive       = "distDrive";
    inline constexpr const char* distThreshold   = "distThreshold";
    inline constexpr const char* distOversample  = "distOversample";
    inline constexpr const char* distEnabled     = "distEnabled";

    // Noise
    inline constexpr const char* noiseMode       = "noiseMode";
    inline constexpr const char* noiseLevel      = "noiseLevel";
    inline constexpr const char* popRate         = "popRate";
    inline constexpr const char* popIntensity    = "popIntensity";
    inline constexpr const char* noiseEnabled    = "noiseEnabled";

    // Output
    inline constexpr const char* filterType  = "filterType";
    inline constexpr const char* filterFreq  = "filterFreq";
    inline constexpr const char* filterQ     = "filterQ";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* outputGain  = "outputGain";
}

// ══════════════════════════════════════════════════════════════════════════════
//  Parameter ranges (NormalisableRange arguments)
// ══════════════════════════════════════════════════════════════════════════════

namespace ParamRange
{
    // Int-like: min, max, step
    inline auto bitDepth       = juce::NormalisableRange<float>(1.0f,   32.0f,   1.0f);

    // Float
    inline auto sampRate       = juce::NormalisableRange<float>(100.0f, 48000.0f, 1.0f);
    inline auto distDrive      = juce::NormalisableRange<float>(0.0f,   10.0f,   0.01f);
    inline auto distThreshold  = juce::NormalisableRange<float>(0.01f,  1.0f,    0.001f);
    inline auto noiseLevel     = juce::NormalisableRange<float>(0.0f,   1.0f,    0.001f);
    inline auto popRate        = juce::NormalisableRange<float>(0.0f,   10.0f,   0.1f);
    inline auto popIntensity   = juce::NormalisableRange<float>(0.0f,   1.0f,    0.001f);
    inline auto filterFreq     = juce::NormalisableRange<float>(20.0f,  20000.0f, 1.0f);
    inline auto filterQ        = juce::NormalisableRange<float>(0.1f,   10.0f,   0.01f);
    inline auto mix            = juce::NormalisableRange<float>(0.0f,   1.0f,    0.001f);

    // Output gain in dB: -24 to +24 in 0.1 dB steps (common for plugin gain)
    inline auto outputGain     = juce::NormalisableRange<float>(-24.0f, 24.0f,   0.1f);
}

// ══════════════════════════════════════════════════════════════════════════════
//  Default values
// ══════════════════════════════════════════════════════════════════════════════

namespace ParamDefault
{
    // Bit Crusher
    inline constexpr float bitDepth      = 16.0f;
    inline constexpr bool  bitEnabled    = false;

    // Decimator
    inline constexpr float sampRate      = 44100.0f;
    inline constexpr bool  aaPre         = true;
    inline constexpr bool  aaPost        = false;
    inline constexpr bool  decEnabled    = false;

    // Distortion
    inline constexpr float  distType       = static_cast<float>(DistortionType::hardClip);
    inline constexpr float  distDrive      = 1.0f;
    inline constexpr float  distThreshold  = 0.5f;
    inline constexpr float  distOversample = static_cast<float>(OversampleMode::twice);
    inline constexpr bool   distEnabled    = false;

    // Noise
    inline constexpr float noiseMode      = static_cast<float>(NoiseMode::pink);
    inline constexpr float noiseLevel     = 0.0f;
    inline constexpr float popRate        = 0.0f;
    inline constexpr float popIntensity   = 0.5f;
    inline constexpr bool  noiseEnabled   = false;

    // Output
    inline constexpr float filterType = static_cast<float>(FilterMode::off);
    inline constexpr float filterFreq = 8000.0f;
    inline constexpr float filterQ    = 0.707f;
    inline constexpr float mix        = 0.5f;
    inline constexpr float outputGain = 0.0f;
}

// ══════════════════════════════════════════════════════════════════════════════
//  Display names
// ══════════════════════════════════════════════════════════════════════════════

namespace ParamName
{
    // Bit Crusher
    inline constexpr const char* bitDepth   = "Bit Depth";
    inline constexpr const char* bitEnabled = "Bit Crusher";

    // Decimator
    inline constexpr const char* sampRate    = "Sample Rate";
    inline constexpr const char* aaPre       = "Anti-Alias Pre";
    inline constexpr const char* aaPost      = "Anti-Alias Post";
    inline constexpr const char* decEnabled  = "Decimator";

    // Distortion
    inline constexpr const char* distType       = "Distortion Type";
    inline constexpr const char* distDrive      = "Drive";
    inline constexpr const char* distThreshold  = "Threshold";
    inline constexpr const char* distOversample = "Oversample";
    inline constexpr const char* distEnabled    = "Distortion";

    // Noise
    inline constexpr const char* noiseMode      = "Noise Mode";
    inline constexpr const char* noiseLevel     = "Noise Level";
    inline constexpr const char* popRate        = "Pop Rate";
    inline constexpr const char* popIntensity   = "Pop Intensity";
    inline constexpr const char* noiseEnabled   = "Noise";

    // Output
    inline constexpr const char* filterType = "Filter Type";
    inline constexpr const char* filterFreq = "Filter Freq";
    inline constexpr const char* filterQ    = "Filter Q";
    inline constexpr const char* mix        = "Mix";
    inline constexpr const char* outputGain = "Output Gain";
}
