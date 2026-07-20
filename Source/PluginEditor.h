/*
  ==============================================================================

   BrittLizer — Plugin Editor
   Minimal UI with sliders/knobs for all 23 parameters, organized in 5
   module strips. All controls connected via AudioProcessorValueTreeState
   attachments for automation and DAW parameter binding.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Params.h"

class BrittLizerAudioProcessor;

//==============================================================================
class BrittLizerAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit BrittLizerAudioProcessorEditor (BrittLizerAudioProcessor&);
    ~BrittLizerAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized() override;

private:
    BrittLizerAudioProcessor& processorRef;

    // Title
    juce::Label titleLabel;

    // ── Bit Crusher ──────────────────────────────────────────────────────────
    juce::Label           bitCrusherTitle;
    juce::ToggleButton    bitCrusherPower;
    juce::Slider          bitDepthSlider;
    juce::Label           bitDepthLabel;

    // ── Decimator ────────────────────────────────────────────────────────────
    juce::Label           decimatorTitle;
    juce::ToggleButton    decimatorPower;
    juce::Slider          sampRateSlider;
    juce::Label           sampRateLabel;
    juce::ToggleButton    aaPreButton;
    juce::ToggleButton    aaPostButton;

    // ── Distortion ───────────────────────────────────────────────────────────
    juce::Label           distortionTitle;
    juce::ToggleButton    distortionPower;
    juce::ComboBox        distTypeCombo;
    juce::Label           distTypeLabel;
    juce::Slider          distDriveSlider;
    juce::Label           distDriveLabel;
    juce::Slider          distThresholdSlider;
    juce::Label           distThresholdLabel;
    juce::ComboBox        distOversampleCombo;
    juce::Label           distOversampleLabel;

    // ── Noise ────────────────────────────────────────────────────────────────
    juce::Label           noiseTitle;
    juce::ToggleButton    noisePower;
    juce::ComboBox        noiseModeCombo;
    juce::Label           noiseModeLabel;
    juce::Slider          noiseLevelSlider;
    juce::Label           noiseLevelLabel;
    juce::Slider          popRateSlider;
    juce::Label           popRateLabel;
    juce::Slider          popIntensitySlider;
    juce::Label           popIntensityLabel;

    // ── Output ───────────────────────────────────────────────────────────────
    juce::Label           outputTitle;
    juce::ComboBox        filterTypeCombo;
    juce::Label           filterTypeLabel;
    juce::Slider          filterFreqSlider;
    juce::Label           filterFreqLabel;
    juce::Slider          filterQSlider;
    juce::Label           filterQLabel;
    juce::Slider          mixSlider;
    juce::Label           mixLabel;
    juce::Slider          outputGainSlider;
    juce::Label           outputGainLabel;

    // ── APVTS Attachments ────────────────────────────────────────────────────
    //  Slider attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        bitDepthAttach, sampRateAttach,
        distDriveAttach, distThresholdAttach,
        noiseLevelAttach, popRateAttach, popIntensityAttach,
        filterFreqAttach, filterQAttach, mixAttach, outputGainAttach;

    //  ComboBox attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        distTypeAttach, distOversampleAttach,
        noiseModeAttach,
        filterTypeAttach;

    //  Button attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        bitEnabledAttach,
        aaPreAttach, aaPostAttach, decEnabledAttach,
        distEnabledAttach,
        noiseEnabledAttach;

    //==============================================================================
    void createAttachments();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrittLizerAudioProcessorEditor)
};
