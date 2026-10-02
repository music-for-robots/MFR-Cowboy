// MFR-Cowboy - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

namespace
{
juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

//==============================================================================
CowboyLookAndFeel::CowboyLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colours::turquoise.withAlpha (0.4f));
    setColour (juce::Slider::textBoxBackgroundColourId, colours::panel.darker (0.2f));
    setColour (juce::Label::textColourId, colours::ink);
    setColour (juce::TextEditor::textColourId, colours::ink);
    setColour (juce::TextButton::textColourOffId, colours::ink);
    setColour (juce::ComboBox::backgroundColourId, colours::panel.darker (0.25f));
    setColour (juce::ComboBox::textColourId, colours::ink);
    setColour (juce::ComboBox::outlineColourId, colours::edge.brighter (0.3f));
    setColour (juce::ComboBox::arrowColourId, colours::turquoise);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::turquoise);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::bg);
}

void CowboyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, endAngle, true);
    g.setColour (colours::trackOff);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, angle, true);
    g.setColour (colours::turquoise);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // A cream amp knob with a dark skirt.
    const float knobR = radius - 8.0f;
    g.setColour (juce::Colour (0xff17100b));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2, knobR * 2).withCentre (centre));
    const float capR = knobR * 0.72f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4ead6), centre.x, centre.y - capR,
                                             juce::Colour (0xffb9a888), centre.x, centre.y + capR, false));
    g.fillEllipse (juce::Rectangle<float> (capR * 2, capR * 2).withCentre (centre));

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR + 2.0f, 3.0f, knobR * 0.55f, 1.5f);
    g.setColour (juce::Colour (0xff17100b));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void CowboyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::turquoise : (highlighted ? colours::panel.brighter (0.1f) : colours::panel.darker (0.2f)));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::turquoise : colours::edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? colours::bg : colours::dimInk);
    g.setFont (font (12.0f, true));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void CowboyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? colours::panel.brighter (0.2f) : highlighted ? colours::panel.brighter (0.1f) : colours::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::edge.brighter (0.3f));
    g.drawRoundedRectangle (r, 4.0f, 1.5f);
}

//==============================================================================
void FineSlider::mouseDown (const juce::MouseEvent& e)
{
    fineDrag = e.mods.isCtrlDown() || e.mods.isShiftDown();
    lastPos = e.position;
    Slider::mouseDown (e);
}

void FineSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! fineDrag || ! isEnabled()) { Slider::mouseDrag (e); return; }
    const float delta = (e.position.x - lastPos.x) + (lastPos.y - e.position.y);
    lastPos = e.position;
    const double pos = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + delta / 2000.0);
    setValue (proportionOfLengthToValue (pos), juce::sendNotificationSync);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& title)
    : attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, colours::panel.darker (0.35f));
    slider.setColour (juce::Slider::textBoxTextColourId, colours::ink);
    if (auto* p = state.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    addAndMakeVisible (slider);

    label.setText (title.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (12.0f, true));
    label.setColour (juce::Label::textColourId, colours::ink);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
CowboyEditor::CowboyEditor (CowboyProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      distAttachment (p.apvts, "diston", distToggle), brightAttachment (p.apvts, "bright", brightToggle),
      ampAttachment (p.apvts, "ampon", ampToggle),
      pulseAttachment (p.apvts, "pulse", (pulseBox.addItemList ({ "Off", "1/4", "1/8", "1/16" }, 1), pulseBox))
{
    setLookAndFeel (&lnf);

    addKnob (distKnobs, "dist", "Dist");
    addKnob (distKnobs, "disttone", "Tone");
    addKnob (distKnobs, "distlevel", "Level");
    addKnob (ampKnobs, "volume", "Volume");
    addKnob (ampKnobs, "treble", "Treble");
    addKnob (ampKnobs, "middle", "Middle");
    addKnob (ampKnobs, "bass", "Bass");
    addKnob (springKnobs, "reverb", "Reverb");
    addKnob (tapeKnobs, "tape", "Tape");
    addKnob (levelKnobs, "output", "Output");

    distToggle.setTooltip ("Stomp the distortion on (with PULSE set, it switches on and off in time)");
    pulseBox.setTooltip ("Switch the distortion on and off in time with the host tempo");
    brightToggle.setTooltip ("Bright switch: extra top at low volume settings");
    ampToggle.setTooltip ("Switch off to use only the distortion, in front of your own amp sim");
    pulseLabel.setText ("PULSE", juce::dontSendNotification);
    pulseLabel.setFont (font (12.0f, true));
    pulseLabel.setJustificationType (juce::Justification::centred);
    pulseLabel.setColour (juce::Label::textColourId, colours::ink);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &distToggle, &pulseBox, &pulseLabel, &brightToggle, &ampToggle })
        addAndMakeVisible (c);

    initButton.onClick = [this] { resetToDefaults(); };
    initButton.setTooltip ("Reset every control to its default");
    addAndMakeVisible (initButton);

    setSize (760, 430);
    startTimerHz (30);
}

CowboyEditor::~CowboyEditor() { setLookAndFeel (nullptr); }

Knob& CowboyEditor::addKnob (std::vector<std::unique_ptr<Knob>>& group, const char* id, const char* title)
{
    auto& k = *group.emplace_back (std::make_unique<Knob> (proc.apvts, id, title));
    addAndMakeVisible (k);
    return k;
}

void CowboyEditor::resetToDefaults()
{
    for (auto* param : proc.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
}

void CowboyEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bg.brighter (0.12f), 0, 0, colours::bg.darker (0.45f), 0, (float) getHeight(), false));
    g.fillAll();

    // Stitching along the edges, like a saddle.
    {
        juce::Path stitch;
        stitch.addRoundedRectangle (getLocalBounds().toFloat().reduced (5.0f), 8.0f);
        juce::Path dashed;
        const float dashes[] = { 6.0f, 5.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, stitch, dashes, 2);
        g.setColour (colours::dimInk.withAlpha (0.45f));
        g.fillPath (dashed);
    }

    g.setColour (colours::ink);
    g.setFont (font (32.0f, true));
    g.drawText ("MFR-COWBOY", 24, 10, 260, 40, juce::Justification::centredLeft);
    g.setColour (colours::dimInk);
    g.setFont (font (12.0f, true));
    g.drawText ("CHEAP DISTORTION  |  CLEAN 100 W COMBO  |  SPRING  |  OLD TAPE", 240, 14, 440, 18, juce::Justification::centredLeft);
    g.setFont (font (10.0f));
    g.drawText ("v" JucePlugin_VersionString "  |  musicforrobots.com", 240, 31, 300, 14, juce::Justification::centredLeft);

    // The distortion as a burnt-orange stompbox.
    {
        const auto r = distPanel.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (2, 3), 6.0f);
        g.setColour (colours::pedal);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (colours::pedal.brighter (0.3f));
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
        g.setColour (colours::ink);
        g.setFont (font (13.0f, true));
        g.drawText ("DISTORTION", distPanel.getX() + 12, distPanel.getY() + 6, 200, 16, juce::Justification::centredLeft);
    }

    const std::pair<juce::Rectangle<int>, const char*> panels[] = {
        { springPanel, "SPRING" }, { tapePanel, "TAPE" }, { ampPanel, "AMP" }, { levelPanel, "OUT" }
    };
    for (auto& [r, name] : panels)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (colours::edge);
        g.drawRoundedRectangle (r.toFloat(), 6.0f, 1.5f);
        g.setColour (colours::ink);
        g.setFont (font (13.0f, true));
        g.drawText (name, r.getX() + 12, r.getY() + 6, 200, 16, juce::Justification::centredLeft);
    }

    // The pedal's light follows the footswitch, and blinks with PULSE.
    const float dl = juce::jlimit (0.0f, 1.0f, shownDist);
    g.setColour (juce::Colour (0xff3a1a0c));
    g.fillEllipse (distLed.toFloat());
    g.setColour (juce::Colour (0xffff5a3c).withAlpha (dl));
    g.fillEllipse (distLed.toFloat().reduced (1.0f));

    g.setColour (colours::dimInk);
    g.setFont (font (10.0f));
    g.drawText ("A big clean amp; the grit is a cheap pedal stomped on and off. PULSE does the stomping in time. Double-click resets a knob.",
                24, getHeight() - 26, getWidth() - 48, 14, juce::Justification::centredLeft);
}

void CowboyEditor::resized()
{
    const int kw = 86, kh = 108, top = 60, rowH = 160;
    auto row = [&] (std::vector<std::unique_ptr<Knob>>& knobs, juce::Rectangle<int> panel)
    {
        const int used = kw * (int) knobs.size();
        auto area = panel.withTrimmedTop (32).withTrimmedLeft ((panel.getWidth() - used) / 2);
        for (auto& k : knobs)
            k->setBounds (area.removeFromLeft (kw).withHeight (kh));
    };

    initButton.setBounds (getWidth() - 96, 18, 80, 26);

    const int side = 130;
    distPanel   = { 16, top, 3 * kw + 24 + side, rowH };
    const int rest = getWidth() - distPanel.getRight() - 10 - 10 - 16;
    springPanel = { distPanel.getRight() + 10, top, rest / 2, rowH };
    tapePanel   = { springPanel.getRight() + 10, top, rest - rest / 2, rowH };

    const int top2 = top + rowH + 10, rowH2 = 150;
    ampPanel   = { 16, top2, 4 * kw + 24 + side, rowH2 };
    levelPanel = { ampPanel.getRight() + 10, top2, getWidth() - ampPanel.getRight() - 26, rowH2 };

    row (distKnobs, distPanel.withTrimmedRight (side));
    distLed = { distPanel.getRight() - 24, distPanel.getY() + 9, 10, 10 };
    const int sx = distPanel.getRight() - side + 4, sw = side - 20;
    distToggle.setBounds (sx, distPanel.getY() + 40, sw, 26);
    pulseLabel.setBounds (sx, distPanel.getY() + 76, sw, 16);
    pulseBox.setBounds (sx, distPanel.getY() + 94, sw, 26);

    row (springKnobs, springPanel);
    row (tapeKnobs, tapePanel);
    row (ampKnobs, ampPanel.withTrimmedRight (side));
    const int ax = ampPanel.getRight() - side + 4;
    brightToggle.setBounds (ax, ampPanel.getY() + 44, sw, 26);
    ampToggle.setBounds (ax, ampPanel.getY() + 80, sw, 26);
    row (levelKnobs, levelPanel);
}

void CowboyEditor::timerCallback()
{
    const float d = proc.distMeter.load();
    if (std::abs (d - shownDist) > 0.02f)
    {
        shownDist = d;
        repaint (distLed.expanded (2));
    }
}
