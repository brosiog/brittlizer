/*
  ==============================================================================

   BrittLizer — Plugin Editor implementation
   Five module strips (Bit Crusher, Decimator, Distortion, Noise, Output)
   with horizontal sliders, combo boxes, and toggle buttons for all 23
   parameters. APVTS attachments bind each control to the parameter tree.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
//  Helper: style a slider uniformly
//==============================================================================
static void styleSlider (juce::Slider& s)
{
    s.setColour (juce::Slider::backgroundColourId,        juce::Colour (0xFF2A2A2A));
    s.setColour (juce::Slider::trackColourId,             juce::Colour (0xFFCC5500));
    s.setColour (juce::Slider::thumbColourId,             juce::Colour (0xFFFF7722));
    s.setColour (juce::Slider::textBoxTextColourId,       juce::Colours::white);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x44000000));
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
}

//==============================================================================
//  Helper: style a toggle (power) button
//==============================================================================
static void styleToggle (juce::ToggleButton& b)
{
    b.setColour (juce::ToggleButton::textColourId, juce::Colour (0xFFCC5500));
    b.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xFFFF7722));
    b.setLookAndFeel (&b.getLookAndFeel()); // refresh
}

//==============================================================================
//  Helper: style a combo box
//==============================================================================
static void styleCombo (juce::ComboBox& c)
{
    c.setColour (juce::ComboBox::backgroundColourId,   juce::Colour (0xFF2A2A2A));
    c.setColour (juce::ComboBox::textColourId,          juce::Colours::white);
    c.setColour (juce::ComboBox::outlineColourId,       juce::Colour (0xFFCC5500));
    c.setColour (juce::ComboBox::arrowColourId,         juce::Colour (0xFFCC5500));
    c.setColour (juce::ComboBox::buttonColourId,        juce::Colour (0xFF333333));
}

//==============================================================================
BrittLizerAudioProcessorEditor::BrittLizerAudioProcessorEditor (BrittLizerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // ── Title ────────────────────────────────────────────────────────────────
    titleLabel.setText ("BRITTLIZER", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (28.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::orangered);
    addAndMakeVisible (titleLabel);

    // ── Bit Crusher ──────────────────────────────────────────────────────────
    bitCrusherTitle.setText ("Bit Crusher", juce::dontSendNotification);
    bitCrusherTitle.setFont (juce::Font (12.0f, juce::Font::bold));
    bitCrusherTitle.setJustificationType (juce::Justification::centredLeft);
    bitCrusherTitle.setColour (juce::Label::textColourId, juce::Colour (0xFFCC5500));
    bitCrusherPower.setButtonText ("ON");
    styleToggle (bitCrusherPower);
    bitDepthLabel.setText ("Bit Depth", juce::dontSendNotification);
    bitDepthLabel.setFont (juce::Font (11));
    bitDepthLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    bitDepthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    bitDepthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (bitDepthSlider);
    addAndMakeVisible (bitCrusherTitle);
    addAndMakeVisible (bitCrusherPower);
    addAndMakeVisible (bitDepthLabel);
    addAndMakeVisible (bitDepthSlider);

    // ── Decimator ────────────────────────────────────────────────────────────
    decimatorTitle.setText ("Decimator", juce::dontSendNotification);
    decimatorTitle.setFont (juce::Font (12.0f, juce::Font::bold));
    decimatorTitle.setJustificationType (juce::Justification::centredLeft);
    decimatorTitle.setColour (juce::Label::textColourId, juce::Colour (0xFFCC5500));
    decimatorPower.setButtonText ("ON");
    styleToggle (decimatorPower);
    sampRateLabel.setText ("Sample Rate", juce::dontSendNotification);
    sampRateLabel.setFont (juce::Font (11));
    sampRateLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    sampRateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sampRateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (sampRateSlider);
    aaPreButton.setButtonText ("AA Pre");
    aaPreButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    styleToggle (aaPreButton);
    aaPostButton.setButtonText ("AA Post");
    aaPostButton.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    styleToggle (aaPostButton);
    addAndMakeVisible (decimatorTitle);
    addAndMakeVisible (decimatorPower);
    addAndMakeVisible (sampRateLabel);
    addAndMakeVisible (sampRateSlider);
    addAndMakeVisible (aaPreButton);
    addAndMakeVisible (aaPostButton);

    // ── Distortion ───────────────────────────────────────────────────────────
    distortionTitle.setText ("Distortion", juce::dontSendNotification);
    distortionTitle.setFont (juce::Font (12.0f, juce::Font::bold));
    distortionTitle.setJustificationType (juce::Justification::centredLeft);
    distortionTitle.setColour (juce::Label::textColourId, juce::Colour (0xFFCC5500));
    distortionPower.setButtonText ("ON");
    styleToggle (distortionPower);
    distTypeLabel.setText ("Type", juce::dontSendNotification);
    distTypeLabel.setFont (juce::Font (11));
    distTypeLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    distTypeCombo.addItemList ({ "Hard Clip", "Foldback", "Asymmetric" }, 1);
    styleCombo (distTypeCombo);
    distDriveLabel.setText ("Drive", juce::dontSendNotification);
    distDriveLabel.setFont (juce::Font (11));
    distDriveLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    distDriveSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    distDriveSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (distDriveSlider);
    distThresholdLabel.setText ("Thresh", juce::dontSendNotification);
    distThresholdLabel.setFont (juce::Font (11));
    distThresholdLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    distThresholdSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    distThresholdSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (distThresholdSlider);
    distOversampleLabel.setText ("OS", juce::dontSendNotification);
    distOversampleLabel.setFont (juce::Font (11));
    distOversampleLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    distOversampleCombo.addItemList ({ "Off", "2x", "4x" }, 1);
    styleCombo (distOversampleCombo);
    addAndMakeVisible (distortionTitle);
    addAndMakeVisible (distortionPower);
    addAndMakeVisible (distTypeLabel);
    addAndMakeVisible (distTypeCombo);
    addAndMakeVisible (distDriveLabel);
    addAndMakeVisible (distDriveSlider);
    addAndMakeVisible (distThresholdLabel);
    addAndMakeVisible (distThresholdSlider);
    addAndMakeVisible (distOversampleLabel);
    addAndMakeVisible (distOversampleCombo);

    // ── Noise ────────────────────────────────────────────────────────────────
    noiseTitle.setText ("Noise", juce::dontSendNotification);
    noiseTitle.setFont (juce::Font (12.0f, juce::Font::bold));
    noiseTitle.setJustificationType (juce::Justification::centredLeft);
    noiseTitle.setColour (juce::Label::textColourId, juce::Colour (0xFFCC5500));
    noisePower.setButtonText ("ON");
    styleToggle (noisePower);
    noiseModeLabel.setText ("Mode", juce::dontSendNotification);
    noiseModeLabel.setFont (juce::Font (11));
    noiseModeLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    noiseModeCombo.addItemList ({ "TPDF", "Pink", "Brown", "White", "Crackle" }, 1);
    styleCombo (noiseModeCombo);
    noiseLevelLabel.setText ("Level", juce::dontSendNotification);
    noiseLevelLabel.setFont (juce::Font (11));
    noiseLevelLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    noiseLevelSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    noiseLevelSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (noiseLevelSlider);
    popRateLabel.setText ("Pops/s", juce::dontSendNotification);
    popRateLabel.setFont (juce::Font (11));
    popRateLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    popRateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    popRateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (popRateSlider);
    popIntensityLabel.setText ("Pop Amt", juce::dontSendNotification);
    popIntensityLabel.setFont (juce::Font (11));
    popIntensityLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    popIntensitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    popIntensitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (popIntensitySlider);
    addAndMakeVisible (noiseTitle);
    addAndMakeVisible (noisePower);
    addAndMakeVisible (noiseModeLabel);
    addAndMakeVisible (noiseModeCombo);
    addAndMakeVisible (noiseLevelLabel);
    addAndMakeVisible (noiseLevelSlider);
    addAndMakeVisible (popRateLabel);
    addAndMakeVisible (popRateSlider);
    addAndMakeVisible (popIntensityLabel);
    addAndMakeVisible (popIntensitySlider);

    // ── Output ───────────────────────────────────────────────────────────────
    outputTitle.setText ("Output", juce::dontSendNotification);
    outputTitle.setFont (juce::Font (12.0f, juce::Font::bold));
    outputTitle.setJustificationType (juce::Justification::centredLeft);
    outputTitle.setColour (juce::Label::textColourId, juce::Colour (0xFFCC5500));
    filterTypeLabel.setText ("Filter", juce::dontSendNotification);
    filterTypeLabel.setFont (juce::Font (11));
    filterTypeLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    filterTypeCombo.addItemList ({ "Off", "Low Pass", "Band Pass", "High Pass" }, 1);
    styleCombo (filterTypeCombo);
    filterFreqLabel.setText ("Freq", juce::dontSendNotification);
    filterFreqLabel.setFont (juce::Font (11));
    filterFreqLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    filterFreqSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    filterFreqSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (filterFreqSlider);
    filterQLabel.setText ("Q", juce::dontSendNotification);
    filterQLabel.setFont (juce::Font (11));
    filterQLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    filterQSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    filterQSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (filterQSlider);
    mixLabel.setText ("Mix", juce::dontSendNotification);
    mixLabel.setFont (juce::Font (11));
    mixLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (mixSlider);
    outputGainLabel.setText ("Gain", juce::dontSendNotification);
    outputGainLabel.setFont (juce::Font (11));
    outputGainLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    outputGainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    styleSlider (outputGainSlider);
    addAndMakeVisible (outputTitle);
    addAndMakeVisible (filterTypeLabel);
    addAndMakeVisible (filterTypeCombo);
    addAndMakeVisible (filterFreqLabel);
    addAndMakeVisible (filterFreqSlider);
    addAndMakeVisible (filterQLabel);
    addAndMakeVisible (filterQSlider);
    addAndMakeVisible (mixLabel);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (outputGainLabel);
    addAndMakeVisible (outputGainSlider);

    // ── Create all APVTS attachments ─────────────────────────────────────────
    createAttachments();

    // ── Window setup ─────────────────────────────────────────────────────────
    setSize (760, 440);
    setResizable (true, false);
    setResizeLimits (600, 380, 1400, 800);
}

BrittLizerAudioProcessorEditor::~BrittLizerAudioProcessorEditor() = default;

//==============================================================================
void BrittLizerAudioProcessorEditor::createAttachments()
{
    auto& apvts = processorRef.apvts;

    // Bit Crusher
    bitDepthAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::bitDepth,   bitDepthSlider);
    bitEnabledAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (apvts, ParamID::bitEnabled, bitCrusherPower);

    // Decimator
    sampRateAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::sampRate,   sampRateSlider);
    aaPreAttach       = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (apvts, ParamID::aaPre,      aaPreButton);
    aaPostAttach      = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (apvts, ParamID::aaPost,     aaPostButton);
    decEnabledAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (apvts, ParamID::decEnabled, decimatorPower);

    // Distortion
    distTypeAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (apvts, ParamID::distType,       distTypeCombo);
    distDriveAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::distDrive,      distDriveSlider);
    distThresholdAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::distThreshold,  distThresholdSlider);
    distOversampleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (apvts, ParamID::distOversample, distOversampleCombo);
    distEnabledAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (apvts, ParamID::distEnabled,    distortionPower);

    // Noise
    noiseModeAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (apvts, ParamID::noiseMode,    noiseModeCombo);
    noiseLevelAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::noiseLevel,   noiseLevelSlider);
    popRateAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::popRate,      popRateSlider);
    popIntensityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::popIntensity, popIntensitySlider);
    noiseEnabledAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (apvts, ParamID::noiseEnabled, noisePower);

    // Output
    filterTypeAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        (apvts, ParamID::filterType, filterTypeCombo);
    filterFreqAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::filterFreq, filterFreqSlider);
    filterQAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::filterQ,    filterQSlider);
    mixAttach         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::mix,        mixSlider);
    outputGainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (apvts, ParamID::outputGain, outputGainSlider);
}

//==============================================================================
void BrittLizerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF1A1A1A));

    // Subtle strip separator lines
    g.setColour (juce::Colour (0xFF2A2A2A));
    auto area = getLocalBounds().reduced (10);

    // Draw horizontal separators between module strips
    auto stripArea = area.withTop (area.getY() + 45);  // below title
    const int stripH = 76;
    for (int i = 0; i < 5; ++i)
    {
        auto strip = stripArea.removeFromTop (stripH);
        if (i > 0)
        {
            g.setColour (juce::Colour (0xFF2A2A2A));
            g.fillRect (strip.withHeight (1));
        }
    }
}

//==============================================================================
void BrittLizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    // ── Title row ────────────────────────────────────────────────────────
    auto titleArea = area.removeFromTop (40);
    titleLabel.setBounds (titleArea);

    // Constants for module strip layout
    const int stripH    = 76;
    const int labelW    = 55;   // width of a parameter label
    const int powerW    = 38;   // width of the power toggle
    const int pad       = 4;    // padding between controls
    const int comboW    = 100;  // width of a combo box

    // ── Bit Crusher ──────────────────────────────────────────────────────
    {
        auto strip = area.removeFromTop (stripH);
        auto left = strip.removeFromLeft (110);  // title section
        bitCrusherTitle.setBounds (left.removeFromTop (20));
        bitCrusherPower.setBounds (left.withSizeKeepingCentre (powerW, 24));
        strip = strip.reduced (0, 8);
        bitDepthLabel.setBounds   (strip.removeFromLeft (labelW));
        bitDepthSlider.setBounds  (strip);
    }

    // ── Decimator ────────────────────────────────────────────────────────
    {
        auto strip = area.removeFromTop (stripH);
        auto left = strip.removeFromLeft (110);
        decimatorTitle.setBounds (left.removeFromTop (20));
        decimatorPower.setBounds (left.withSizeKeepingCentre (powerW, 24));
        strip = strip.reduced (0, 8);
        sampRateLabel.setBounds (strip.removeFromLeft (labelW));
        sampRateSlider.setBounds (strip.removeFromLeft (proportionOfWidth (0.45f)));
        strip.removeFromLeft (pad);
        aaPreButton.setBounds  (strip.removeFromLeft (62));
        strip.removeFromLeft (pad);
        aaPostButton.setBounds (strip.removeFromLeft (68));
    }

    // ── Distortion ───────────────────────────────────────────────────────
    {
        auto strip = area.removeFromTop (stripH);
        auto left = strip.removeFromLeft (110);
        distortionTitle.setBounds (left.removeFromTop (20));
        distortionPower.setBounds (left.withSizeKeepingCentre (powerW, 24));
        strip = strip.reduced (0, 8);
        distTypeLabel.setBounds   (strip.removeFromLeft (labelW));
        distTypeCombo.setBounds   (strip.removeFromLeft (comboW));
        strip.removeFromLeft (pad);
        distDriveLabel.setBounds  (strip.removeFromLeft (labelW - 10));
        distDriveSlider.setBounds (strip.removeFromLeft (juce::jmax (100, strip.getWidth() / 5)));
        strip.removeFromLeft (pad);
        distThresholdLabel.setBounds  (strip.removeFromLeft (labelW - 6));
        distThresholdSlider.setBounds (strip.removeFromLeft (juce::jmax (100, strip.getWidth() / 4)));
        strip.removeFromLeft (pad);
        distOversampleLabel.setBounds (strip.removeFromLeft (labelW - 20));
        distOversampleCombo.setBounds (strip.removeFromLeft (60));
    }

    // ── Noise ────────────────────────────────────────────────────────────
    {
        auto strip = area.removeFromTop (stripH);
        auto left = strip.removeFromLeft (110);
        noiseTitle.setBounds (left.removeFromTop (20));
        noisePower.setBounds (left.withSizeKeepingCentre (powerW, 24));
        strip = strip.reduced (0, 8);
        noiseModeLabel.setBounds    (strip.removeFromLeft (labelW));
        noiseModeCombo.setBounds    (strip.removeFromLeft (comboW));
        strip.removeFromLeft (pad);
        noiseLevelLabel.setBounds   (strip.removeFromLeft (labelW - 10));
        noiseLevelSlider.setBounds  (strip.removeFromLeft (juce::jmax (80, strip.getWidth() / 5)));
        strip.removeFromLeft (pad);
        popRateLabel.setBounds      (strip.removeFromLeft (labelW - 10));
        popRateSlider.setBounds     (strip.removeFromLeft (juce::jmax (80, strip.getWidth() / 5)));
        strip.removeFromLeft (pad);
        popIntensityLabel.setBounds (strip.removeFromLeft (labelW - 6));
        popIntensitySlider.setBounds(strip.removeFromLeft (juce::jmax (80, strip.getWidth() / 4)));
    }

    // ── Output ───────────────────────────────────────────────────────────
    {
        auto strip = area.removeFromTop (stripH);
        auto left = strip.removeFromLeft (110);
        outputTitle.setBounds (left.removeFromTop (20));
        // Output has no bypass toggle, fill the space
        strip = strip.reduced (0, 8);
        filterTypeLabel.setBounds  (strip.removeFromLeft (labelW));
        filterTypeCombo.setBounds  (strip.removeFromLeft (comboW));
        strip.removeFromLeft (pad);
        filterFreqLabel.setBounds  (strip.removeFromLeft (labelW - 10));
        filterFreqSlider.setBounds (strip.removeFromLeft (juce::jmax (80, strip.getWidth() / 6)));
        strip.removeFromLeft (pad);
        filterQLabel.setBounds     (strip.removeFromLeft (labelW - 20));
        filterQSlider.setBounds    (strip.removeFromLeft (juce::jmax (80, strip.getWidth() / 6)));
        strip.removeFromLeft (pad);
        mixLabel.setBounds         (strip.removeFromLeft (labelW - 10));
        mixSlider.setBounds        (strip.removeFromLeft (juce::jmax (80, strip.getWidth() / 6)));
        strip.removeFromLeft (pad);
        outputGainLabel.setBounds  (strip.removeFromLeft (labelW - 6));
        outputGainSlider.setBounds (strip);
    }
}
