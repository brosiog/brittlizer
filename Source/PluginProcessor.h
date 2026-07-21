/*
  ==============================================================================

   BrittLizer — Plugin Processor
   AudioProcessor with APVTS and full 5-module DSP chain.

   Pipeline: Input → Bit Crusher → Decimator → Distortion → Noise → Output (SVF + Mix + Gain)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Params.h"
#include "DSP/BitCrusher.h"
#include "DSP/Decimator.h"
#include "DSP/Distortion.h"
#include "DSP/NoiseGenerator.h"
#include "DSP/OutputStage.h"

//==============================================================================
class BrittLizerAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    BrittLizerAudioProcessor();
    ~BrittLizerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources()                                      override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool                        hasEditor()   const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "Carmy BrittLizer"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int  getNumPrograms()                override { return 1; }
    int  getCurrentProgram()             override { return 0; }
    void setCurrentProgram (int)         override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Cached raw pointers to APVTS parameters (set in prepareToPlay)
    std::atomic<float>* bitDepthParam      { nullptr };
    std::atomic<float>* bitEnabledParam    { nullptr };
    std::atomic<float>* sampRateParam      { nullptr };
    std::atomic<float>* aaPreParam         { nullptr };
    std::atomic<float>* aaPostParam        { nullptr };
    std::atomic<float>* decEnabledParam    { nullptr };
    std::atomic<float>* distTypeParam      { nullptr };
    std::atomic<float>* distDriveParam     { nullptr };
    std::atomic<float>* distThresholdParam { nullptr };
    std::atomic<float>* distOversampleParam{ nullptr };
    std::atomic<float>* distEnabledParam   { nullptr };
    std::atomic<float>* noiseModeParam     { nullptr };
    std::atomic<float>* noiseLevelParam    { nullptr };
    std::atomic<float>* popRateParam       { nullptr };
    std::atomic<float>* popIntensityParam  { nullptr };
    std::atomic<float>* noiseEnabledParam  { nullptr };
    std::atomic<float>* filterTypeParam    { nullptr };
    std::atomic<float>* filterFreqParam    { nullptr };
    std::atomic<float>* filterQParam       { nullptr };
    std::atomic<float>* mixParam           { nullptr };
    std::atomic<float>* outputGainParam    { nullptr };

    //==============================================================================
    //  DSP modules
    //==============================================================================
    BitCrusher          bitCrusher;
    Decimator           decimator;
    DistortionProcessor distortion;
    NoiseGenerator      noiseGen;
    OutputStage         outputStage;

    // Dry signal buffer (captured before any processing for dry/wet mix)
    juce::AudioBuffer<float> dryBuffer;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrittLizerAudioProcessor)
};
