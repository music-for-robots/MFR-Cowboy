// MFR-Cowboy - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using FloatParam = juce::AudioParameterFloat;
using Attrs = juce::AudioParameterFloatAttributes;

std::unique_ptr<FloatParam> tenScale (const juce::String& id, const juce::String& name, float def, float lo = 0.0f)
{
    return std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, 10.0f), def,
                                         Attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); }));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout CowboyProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const cowboy::Params d;

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "diston", 1 }, "Distortion On", d.distOn));
    layout.add (tenScale ("dist", "Distortion", d.dist * 10.0f));
    layout.add (tenScale ("disttone", "Distortion Tone", d.distTone * 10.0f));
    layout.add (tenScale ("distlevel", "Distortion Level", d.distLevel * 10.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "pulse", 1 }, "Pulse",
                                                              juce::StringArray { "Off", "1/4", "1/8", "1/16" }, d.pulse));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "ampon", 1 }, "Amp + Cab On", d.ampOn));
    layout.add (tenScale ("volume", "Volume", d.volume, 1.0f));
    layout.add (tenScale ("treble", "Treble", d.treble));
    layout.add (tenScale ("middle", "Middle", d.middle));
    layout.add (tenScale ("bass", "Bass", d.bass));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bright", 1 }, "Bright", d.bright));
    layout.add (tenScale ("reverb", "Reverb", d.reverb * 10.0f));
    layout.add (tenScale ("tape", "Tape", d.tape * 10.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "output", 1 }, "Output",
                                              juce::NormalisableRange<float> (-36.0f, 12.0f), d.outputDb,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })));
    return layout;
}

CowboyProcessor::CowboyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pDistOn = get ("diston"); pDist = get ("dist"); pDistTone = get ("disttone"); pDistLevel = get ("distlevel");
    pPulse = get ("pulse"); pAmpOn = get ("ampon"); pVolume = get ("volume"); pTreble = get ("treble");
    pMiddle = get ("middle"); pBass = get ("bass"); pBright = get ("bright"); pReverb = get ("reverb");
    pTape = get ("tape"); pOutput = get ("output");
}

bool CowboyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return inOk && out == juce::AudioChannelSet::stereo();
}

void CowboyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate;
    work.setSize (2, samplesPerBlock);
    oversampling.reset();
    oversampling.initProcessing ((size_t) samplesPerBlock);
    chain.prepare (sampleRate * (1 << kOversampleStages));
    setLatencySamples (juce::roundToInt (oversampling.getLatencyInSamples()));
}

void CowboyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (work.getNumSamples() < n)
        work.setSize (2, n, false, false, true);

    work.clear();
    for (int ch = 0; ch < numIn; ++ch)
        work.addFrom (0, 0, buffer, ch, 0, n, 1.0f / (float) numIn);

    cowboy::Params p;
    p.distOn = pDistOn->load() > 0.5f;
    p.dist = pDist->load() / 10.0f; p.distTone = pDistTone->load() / 10.0f; p.distLevel = pDistLevel->load() / 10.0f;
    p.pulse = juce::jlimit (0, 3, (int) pPulse->load());
    p.ampOn = pAmpOn->load() > 0.5f;
    p.volume = pVolume->load(); p.treble = pTreble->load(); p.middle = pMiddle->load(); p.bass = pBass->load();
    p.bright = pBright->load() > 0.5f;
    p.reverb = pReverb->load() / 10.0f;
    p.tape = pTape->load() / 10.0f;
    p.outputDb = pOutput->load();
    chain.setParams (p);

    // PULSE follows the host's tempo and bar position; with no host clock it free-runs at 120 BPM.
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            if (auto q = pos->getPpqPosition()) { ppq = *q; playing = pos->getIsPlaying(); }
        }
    chain.setTransport (playing, ppq, bpm / 60.0 / (hostRate * (1 << kOversampleStages)));

    juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) n);
    auto up = oversampling.processSamplesUp (block);
    float* l = up.getChannelPointer (0);
    float* r = up.getChannelPointer (1);
    for (size_t i = 0; i < up.getNumSamples(); ++i)
        chain.process (l[i], l[i], r[i]);
    oversampling.processSamplesDown (block);

    buffer.copyFrom (0, 0, work, 0, 0, n);
    buffer.copyFrom (1, 0, work, 1, 0, n);
    distMeter = chain.getDistMix();
}

void CowboyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void CowboyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CowboyProcessor::createEditor() { return new CowboyEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CowboyProcessor(); }
