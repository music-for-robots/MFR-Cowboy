// MFR-Cowboy - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Late-'90s Northwest indie guitar, run 4x oversampled:
//
//   guitar -> [cheap distortion, optionally PULSEd on/off in time] -> blackface-style clean amp
//          -> spring reverb -> 6x10 cab -> 1969-style tape -> L/R
//
// The amp is a big, clean, bright 100 W combo: the grit comes from a cheap op-amp
// distortion stomped on and off in rhythm (PULSE automates that, synced to the
// host tempo). Six 10-inch speakers, a close mic and old tape put a hard ceiling
// on the top end just past 3 kHz.
//
// Pure C++ (no JUCE).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace cowboy
{

constexpr double kPi = 3.14159265358979323846;
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    void lowpass (double fs, double f, double q)
    { const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q); set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
    void highpass (double fs, double f, double q)
    { const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q); set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
    void peaking (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowshelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sqrt (A) * std::sin (w) * std::sqrt (2.0);
        set (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
             (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
    }
    void highshelf (double fs, double f, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sqrt (A) * std::sin (w) * std::sqrt (2.0);
        set (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
             (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
    }
private:
    void set (double B0, double B1, double B2, double A0, double A1, double A2)
    { b0 = float (B0 / A0); b1 = float (B1 / A0); b2 = float (B2 / A0); a1 = float (A1 / A0); a2 = float (A2 / A0); }
};

struct OnePole
{
    float a = 0, z = 0;
    void setCutoff (double fs, double f) { a = float (1 - std::exp (-2 * kPi * f / fs)); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0; }
};

//==============================================================================
// Passive treble/middle/bass network (Yeh & Smith 2006 analog model, bilinear),
// with blackface-era component values. Double precision: at 4x oversampling
// the poles sit very close to z = 1, and single precision goes unstable.
class ToneStack
{
public:
    void set (double fs, double treble, double middle, double bass)
    {
        const double R1 = 250e3, R2 = 250e3, R3 = 10e3, R4 = 100e3;   // treble, bass, middle pots; slope resistor
        const double C1 = 250e-12, C2 = 100e-9, C3 = 47e-9;
        const double t = std::clamp (treble, 0.0, 1.0), m = std::clamp (middle, 0.0, 1.0);
        const double l = std::exp ((std::clamp (bass, 0.0, 1.0) - 1.0) * 3.4);

        const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                        + t * l * C1 * C2 * C3 * R1 * R2 * R4;
        const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
        const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                        + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
                        + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

        const double c = 2 * fs, c2 = c * c, c3 = c2 * c;
        const double B0 = -b1 * c - b2 * c2 - b3 * c3, B1 = -b1 * c + b2 * c2 + 3 * b3 * c3;
        const double B2 = b1 * c + b2 * c2 - 3 * b3 * c3, B3 = b1 * c - b2 * c2 + b3 * c3;
        const double A0 = -1 - a1 * c - a2 * c2 - a3 * c3, A1 = -3 - a1 * c + a2 * c2 + 3 * a3 * c3;
        const double A2 = -3 + a1 * c + a2 * c2 - 3 * a3 * c3, A3 = -1 + a1 * c - a2 * c2 + a3 * c3;
        nb0 = B0 / A0; nb1 = B1 / A0; nb2 = B2 / A0; nb3 = B3 / A0;
        na1 = A1 / A0; na2 = A2 / A0; na3 = A3 / A0;
    }
    float process (float in)
    {
        const double x = in;
        const double y = nb0 * x + s1;
        s1 = nb1 * x - na1 * y + s2;
        s2 = nb2 * x - na2 * y + s3;
        s3 = nb3 * x - na3 * y;
        if (! std::isfinite (y)) { reset(); return 0.0f; }
        return (float) y;
    }
    void reset() { s1 = s2 = s3 = 0; }
private:
    double nb0 = 0, nb1 = 0, nb2 = 0, nb3 = 0, na1 = 0, na2 = 0, na3 = 0, s1 = 0, s2 = 0, s3 = 0;
};

//==============================================================================
// A tiny spring tank: dispersive allpasses for the 'boing', a recirculating
// delay, and band-limiting.
class Spring
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& a : ap) { a.z = 0; }
        line.assign ((size_t) (0.036 * fs) + 1, 0.0f);
        line2.assign ((size_t) (0.041 * fs) + 1, 0.0f);
        pos = pos2 = 0;
        lp.lowpass (fs, 3800, 0.7); hp.highpass (fs, 300, 0.7);
        reset();
    }
    void reset()
    {
        for (auto& a : ap) a.z = 0;
        std::fill (line.begin(), line.end(), 0.0f);
        std::fill (line2.begin(), line2.end(), 0.0f);
        lp.reset(); hp.reset();
    }
    void process (float x, float& outL, float& outR)
    {
        float v = hp.process (x);
        for (auto& a : ap) v = a.process (v);                      // dispersion: highs arrive first, the chirp
        const float d1 = line[(size_t) pos], d2 = line2[(size_t) pos2];
        line[(size_t) pos] = lp.process (v + 0.62f * d2);
        line2[(size_t) pos2] = v * 0.7f + 0.6f * d1;
        if (++pos == (int) line.size()) pos = 0;
        if (++pos2 == (int) line2.size()) pos2 = 0;
        outL = d1; outR = d2;
    }
private:
    struct Allpass { float z = 0; float process (float x) { const float y = -0.65f * x + z; z = x + 0.65f * y; return y; } };
    double fs = 192000;
    std::array<Allpass, 24> ap {};
    std::vector<float> line, line2;
    int pos = 0, pos2 = 0;
    Biquad lp, hp;
};

//==============================================================================
struct Params
{
    bool  distOn = false;
    float dist = 0.6f, distTone = 0.5f, distLevel = 0.5f;   // 0..1
    int   pulse = 0;                     // 0 off, 1 = 1/4, 2 = 1/8, 3 = 1/16 notes
    bool  ampOn = true;
    float volume = 5.0f;                 // 1..10
    float treble = 6.0f, middle = 5.0f, bass = 4.0f;   // 0..10
    bool  bright = true;
    float reverb = 0.2f;                 // 0..1
    float tape = 0.5f;                   // 0..1
    float outputDb = -6;
};

class Chain
{
public:
    void prepare (double oversampledRate)
    {
        fs = oversampledRate;
        dHP.setCutoff (fs, 100); dLP.lowpass (fs, 5000, 0.7);
        couple1.setCutoff (fs, 20); couple2.setCutoff (fs, 20);
        cabHP.highpass (fs, 80, 0.7); cabPeak.peaking (fs, 550, 1.1, 9.5);   // 10-inch cone body
        cabDip.peaking (fs, 2300, 0.4, -10.0);   // broad upper-mid scoop, fitted to the reference
        cabLP1.lowpass (fs, 2600, 1.25); cabLP2.lowpass (fs, 6000, 0.35); cabLP3.lowpass (fs, 6000, 0.35);   // cone breakup at ~2.6 kHz, then the cliff
        fade = 1.0f / (float) (0.003 * fs);   // 3 ms crossfade for pulse switching
        spring.prepare (fs);
        setParams (params);
        reset();
    }
    void reset()
    {
        dHP.reset(); couple1.reset(); couple2.reset();
        for (auto* b : { &dLP, &dTone, &bright, &cabHP, &cabPeak, &cabDip, &cabLP1, &cabLP2, &cabLP3, &tapeShelf, &tapeR }) b->reset();
        stack.reset(); spring.reset();
        mix = params.distOn ? 1.0f : 0.0f;
    }
    void setParams (const Params& p)
    {
        params = p;
        dGain = 1.0f + 900.0f * p.dist * p.dist;
        dTone.highshelf (fs, 1200, (p.distTone - 0.5f) * 16.0f);
        dLevel = dbToGain (-30.0f + 30.0f * p.distLevel);
        const float v = std::clamp ((p.volume - 1.0f) / 9.0f, 0.0f, 1.0f);
        volGain = 0.15f + 6.0f * v * v;
        bright.highshelf (fs, 2000, p.bright ? 6.0 * (1.0 - v) : 0.0);
        stack.set (fs, p.treble / 10.0, p.middle / 10.0, p.bass / 10.0);
        tapeShelf.highshelf (fs, 6000, -6.0 * p.tape);
        tapeR.highshelf (fs, 6000, -6.0 * p.tape);
        tapeDrive = 1.0f + 3.0f * p.tape;
        outGain = dbToGain (p.outputDb);
    }

    // Host tempo for PULSE: ppq at the next sample and quarter notes per sample.
    void setTransport (bool playing, double ppq, double ppqPerSample) { hostPlaying = playing; ppqNow = ppq; ppqStep = ppqPerSample; }
    float getDistMix() const { return mix; }   // 0 = distortion out, 1 = in (for the UI light)

    void process (float x, float& outL, float& outR)
    {
        // Is the distortion engaged right now? (footswitch, or the pulse pattern)
        bool engaged = params.distOn;
        if (engaged && params.pulse > 0)
        {
            const double div = params.pulse == 1 ? 1.0 : params.pulse == 2 ? 0.5 : 0.25;   // note length in quarters
            // One note length on, the next off: the period is two note lengths.
            const double phase = hostPlaying ? std::fmod (ppqNow / (2.0 * div), 1.0) : std::fmod ((freeRun += ppqStep) / (2.0 * div), 1.0);
            engaged = phase < 0.5;
        }
        ppqNow += ppqStep;
        mix = std::clamp (mix + (engaged ? fade : -fade), 0.0f, 1.0f);

        // Cheap op-amp distortion: big gain into a hard-ish clip.
        float d = 0;
        if (mix > 0.0f)
        {
            d = dHP.hp (x) * dGain;
            d = 0.5f * d / std::pow (1.0f + std::pow (std::abs (d / 0.5f), 4.0f), 0.25f);
            d = dTone.process (dLP.process (d)) * dLevel;
        }
        float y = x + mix * (d - x);

        if (params.ampOn)
        {
            // Blackface-style: first triode, tone stack, volume, second triode, spring, big clean power stage.
            float a = bright.process (y) * 4.0f;
            a = a > 0 ? std::tanh (0.9f * a) / 0.9f : std::tanh (0.7f * a) / 0.7f;
            a = couple1.hp (stack.process (a)) * volGain;
            a = std::tanh (a + 0.05f) - 0.05f;
            a = couple2.hp (a);
            float sl, sr;
            spring.process (a, sl, sr);
            const float w = params.reverb * 1.5f;
            float l = a + w * sl, r = a + w * sr;
            auto power = [] (float s) { return std::tanh (0.6f * s) / 0.6f; };   // lots of headroom
            l = power (l); r = power (r);
            // One cab (mono filter state): the mid goes through it, and the spring's width rides along as a soft side.
            const float m = 0.5f * (l + r), s = 0.5f * (l - r);
            Biquad* chain[6] = { &cabHP, &cabPeak, &cabDip, &cabLP1, &cabLP2, &cabLP3 };
            float mc = m;
            for (auto* f : chain) mc = f->process (mc);
            outL = mc + 0.35f * sideLP.lp (s);
            outR = mc - 0.35f * sideLP.lp2 (s);
        }
        else
        {
            outL = outR = y * 0.5f;
        }

        // 1969 tape: soft saturation and a softened top.
        auto tape = [this] (float s) { return std::tanh (s * tapeDrive) / tapeDrive; };
        outL = tapeShelf.process (tape (outL));
        outR = tapeR.process (tape (outR));
        outL *= outGain * 14.0f; outR *= outGain * 14.0f;   // makeup for the cab and tape losses: defaults peak near -6 dBFS
        if (! std::isfinite (outL) || ! std::isfinite (outR)) { reset(); outL = outR = 0; }
    }

private:
    struct SideLP { float a = 0.05f, z1 = 0, z2 = 0; float lp (float x) { z1 += a * (x - z1); return z1; } float lp2 (float x) { z2 += a * (x - z2); return z2; } };
    double fs = 192000;
    Params params;
    OnePole dHP, couple1, couple2;
    Biquad dLP, dTone, bright, cabHP, cabPeak, cabDip, cabLP1, cabLP2, cabLP3, tapeShelf, tapeR;
    ToneStack stack;
    Spring spring;
    SideLP sideLP;
    float dGain = 300, dLevel = 1, volGain = 2, tapeDrive = 2, outGain = 0.5f, mix = 0, fade = 0.001f;
    bool hostPlaying = false;
    double ppqNow = 0, ppqStep = 0, freeRun = 0;
};

} // namespace cowboy
