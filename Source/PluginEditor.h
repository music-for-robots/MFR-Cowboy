// MFR-Cowboy - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace colours
{
const juce::Colour bg       { 0xff2a1e15 };   // dark saddle leather
const juce::Colour panel    { 0xff3a2a1e };
const juce::Colour edge     { 0xff6e5539 };
const juce::Colour ink      { 0xfff1e4c9 };   // sun-bleached bone
const juce::Colour dimInk   { 0xffb49c7a };
const juce::Colour trackOff { 0xff4b3826 };
const juce::Colour turquoise{ 0xff3fbcb0 };
const juce::Colour pedal    { 0xffa04e24 };   // burnt-orange stompbox
} // namespace colours

class CowboyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CowboyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

class FineSlider : public juce::Slider
{
public:
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    bool fineDrag = false;
    juce::Point<float> lastPos;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title);
    void resized() override;
private:
    FineSlider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class CowboyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CowboyEditor (CowboyProcessor&);
    ~CowboyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    Knob& addKnob (std::vector<std::unique_ptr<Knob>>&, const char* id, const char* title);
    void resetToDefaults();

    CowboyProcessor& proc;
    CowboyLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> distKnobs, ampKnobs, springKnobs, tapeKnobs, levelKnobs;
    juce::ToggleButton distToggle { "ON" }, brightToggle { "BRIGHT" }, ampToggle { "AMP + CAB" };
    juce::ComboBox pulseBox;
    juce::Label pulseLabel;
    juce::AudioProcessorValueTreeState::ButtonAttachment distAttachment, brightAttachment, ampAttachment;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment pulseAttachment;
    juce::TextButton initButton { "INIT" };
    juce::Rectangle<int> distPanel, springPanel, tapePanel, ampPanel, levelPanel, distLed;
    float shownDist = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CowboyEditor)
};
