// MFR-Cowboy - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "CowboyDSP.h"

class CowboyProcessor : public juce::AudioProcessor
{
public:
    CowboyProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MFR-Cowboy"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }   // the spring's tail

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> distMeter { 0.0f };

private:
    static constexpr int kOversampleStages = 2;   // 4x
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::dsp::Oversampling<float> oversampling { 2, kOversampleStages,
                                                  juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    cowboy::Chain chain;
    juce::AudioBuffer<float> work;   // ch 0 carries the guitar in; L/R come out (the spring is stereo)
    double hostRate = 44100.0;

    std::atomic<float>* pDistOn; std::atomic<float>* pDist; std::atomic<float>* pDistTone; std::atomic<float>* pDistLevel;
    std::atomic<float>* pPulse; std::atomic<float>* pAmpOn; std::atomic<float>* pVolume; std::atomic<float>* pTreble;
    std::atomic<float>* pMiddle; std::atomic<float>* pBass; std::atomic<float>* pBright; std::atomic<float>* pReverb;
    std::atomic<float>* pTape; std::atomic<float>* pOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CowboyProcessor)
};
