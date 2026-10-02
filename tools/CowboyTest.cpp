// MFR-Cowboy - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Offline checks (exit 1 on regressions, for CI):
//   1. PULSE: with the transport at 120 BPM and 1/8 pulses, the distortion engages on every
//      other eighth note (the output alternates between two clearly different states)
//   2. voicing: clean rig against the measured balance of the reference's clean intro (--fit searches the EQ)
//   3. held chords: 8 s chords must decay smoothly, pedal off and on
//   4. full chain at defaults: finite, not clipping
// Writes cowboy_demo.wav (stereo): clean arpeggios, then the same with the distortion pulsing in 1/8s.
#include "../Source/CowboyDSP.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

using namespace cowboy;
constexpr double kFs = 48000.0, kOs = kFs * 4;
static bool gFailed = false;

// Target: 1/3-octave balance of the reference's clean intro guitar (mid channel, 0.2-7.6 s),
// smoothed over 3 bands and re-centred at 1 kHz, 200 Hz .. 8 kHz.
static const double kBands[] = { 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000 };
static const double kTarget[] = { 0.9, 1.4, 0.8, 2.1, 2.8, 3.0, 1.6, 0.0, -1.9, -1.1, -1.6, -4.0, -7.5, -16.5, -22.9, -30.7, -31.8 };
constexpr int kNumBands = 17;

struct Pluck
{
    std::vector<float> buf; size_t pos = 0; float damp;
    Pluck (double hz, float amp, uint32_t& rng, float damping) : buf ((size_t) std::lround (kOs / hz)), damp (damping)
    {
        for (auto& v : buf) { rng = rng * 1664525u + 1013904223u; v = amp * ((float) (rng >> 9) / 4194304.0f - 1.0f); }
        float lp = 0; for (auto& v : buf) { lp += 0.6f * (v - lp); v = lp; }   // a bright, single-coil-ish pluck
    }
    float tick() { const size_t nx = (pos + 1) % buf.size(); const float o = buf[pos]; buf[pos] = damp * 0.5f * (buf[pos] + buf[nx]); pos = nx; return o; }
};

// Clean picked arpeggios up the neck (G, C, D, Em shapes), eighth notes at ~140 BPM.
static std::vector<float> arpeggios (double seconds, float amp)
{
    static const double chords[4][4] = { { 196.0, 246.9, 293.7, 392.0 }, { 261.6, 329.6, 392.0, 523.3 },
                                          { 293.7, 370.0, 440.0, 587.3 }, { 329.6, 392.0, 493.9, 659.3 } };
    std::vector<float> y ((size_t) (seconds * kOs), 0.0f);
    const double step = 60.0 / 140.0 / 2.0;
    uint32_t rng = 21;
    std::vector<Pluck> ringing;
    size_t next = 0; int n = 0;
    for (size_t i = 0; i < y.size(); ++i)
    {
        if (i == next)
        {
            const auto& c = chords[(n / 8) % 4];
            ringing.emplace_back (c[n % 4], amp, rng, 0.9993f);
            if (ringing.size() > 6) ringing.erase (ringing.begin());
            ++n; next = (size_t) (n * step * kOs);
        }
        float s = 0; for (auto& p : ringing) s += p.tick();
        y[i] = s;
    }
    return y;
}

static void run (const Params& p, const std::vector<float>& in, std::vector<float>& L, std::vector<float>& R, double bpm = 120.0)
{
    Chain c; c.prepare (kOs); c.setParams (p);
    c.setTransport (true, 0.0, bpm / 60.0 / kOs);
    L.resize (in.size()); R.resize (in.size());
    for (size_t i = 0; i < in.size(); ++i) c.process (in[i], L[i], R[i]);
}

static void bandLevels (const std::vector<float>& x, double* out)
{
    const int N = 16384;
    std::vector<double> acc (kNumBands, 0.0);
    for (size_t s = 0; s + N <= x.size(); s += N / 2)
        for (int b = 0; b < kNumBands; ++b)
        {
            const double lo = kBands[b] / std::pow (2.0, 1.0 / 6), hi = kBands[b] * std::pow (2.0, 1.0 / 6);
            for (double f = lo; f < hi; f += (hi - lo) / 12)
            {
                const double w = 2 * kPi * f / kOs, c = 2 * std::cos (w);
                double s1 = 0, s2 = 0;
                for (int i = 0; i < N; ++i)
                {
                    const double win = 0.5 - 0.5 * std::cos (2 * kPi * i / N);
                    const double s0 = x[s + (size_t) i] * win + c * s1 - s2; s2 = s1; s1 = s0;
                }
                acc[(size_t) b] += s1 * s1 + s2 * s2 - c * s1 * s2;
            }
        }
    const double ref = 10 * std::log10 (acc[7] + 1e-30);
    for (int b = 0; b < kNumBands; ++b) out[b] = 10 * std::log10 (acc[(size_t) b] + 1e-30) - ref;
}

static double fitError (const double* lv)
{
    double e = 0; for (int b = 0; b < kNumBands; ++b) { const double d = lv[b] - kTarget[b]; e += d * d; }
    return std::sqrt (e / kNumBands);
}

static void writeWavStereo (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t bytes = (uint32_t) L.size() * 4;
    f.write ("RIFF", 4); u32 (36 + bytes); f.write ("WAVEfmt ", 8);
    u32 (16); u16 (1); u16 (2); u32 (48000); u32 (48000 * 4); u16 (4); u16 (16);
    f.write ("data", 4); u32 (bytes);
    for (size_t i = 0; i < L.size(); ++i)
        for (float s : { L[i], R[i] })
        { const auto v = (int16_t) std::lround (std::clamp (s, -1.0f, 1.0f) * 32767.0f); f.write ((const char*) &v, 2); }
}

int main (int argc, char** argv)
{
    const bool fit = argc > 1 && std::strcmp (argv[1], "--fit") == 0;

    // 1. PULSE at 120 BPM, 1/8 notes: a steady sine through the rig should alternate state every 0.25 s.
    {
        Params p; p.distOn = true; p.pulse = 2; p.reverb = 0;
        std::vector<float> in ((size_t) (2.0 * kOs));
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.05f * (float) std::sin (2 * kPi * 196.0 * i / kOs);
        std::vector<float> L, R; run (p, in, L, R, 120.0);
        // High-frequency content (above the clean sine's harmonics) in each eighth note.
        const size_t eighth = (size_t) (0.25 * kOs);
        std::printf ("pulse 1/8 @120 BPM, distortion energy per eighth (dB):");
        double minOn = 1e9, maxOff = -1e9;
        for (int k = 1; k < 8; ++k)
        {
            double e = 0, prev = 0;
            for (size_t i = k * eighth + eighth / 5; i < (k + 1) * eighth - eighth / 5; ++i) { const double d = L[i] - prev; prev = L[i]; e += d * d; }
            const double db = 10 * std::log10 (e + 1e-30);
            std::printf (" %.0f", db);
            (k % 2 == 0 ? minOn : maxOff) = (k % 2 == 0) ? std::min (minOn, db) : std::max (maxOff, db);
        }
        std::printf ("\n  engaged minimum vs bypassed maximum: %.1f dB apart\n", minOn - maxOff);
        if (! (minOn - maxOff > 6.0)) gFailed = true;
    }

    // 2. Voicing: clean (pedal off, reverb off).
    const auto riff = arpeggios (3.0, 0.12f);
    {
        Params p; p.reverb = 0;
        std::vector<float> L, R; run (p, riff, L, R);
        double lv[kNumBands]; bandLevels (L, lv);
        std::printf ("voicing at defaults vs reference clean intro (dB re 1 kHz):\n  band  ");
        for (int b = 0; b < kNumBands; ++b) std::printf ("%6.0f", kBands[b]);
        std::printf ("\n  ref   "); for (int b = 0; b < kNumBands; ++b) std::printf ("%+6.1f", kTarget[b]);
        std::printf ("\n  ours  "); for (int b = 0; b < kNumBands; ++b) std::printf ("%+6.1f", lv[b]);
        std::printf ("\n  RMS error: %.1f dB\n", fitError (lv));
        if (! fit && fitError (lv) > 3.0) gFailed = true;
    }
    if (fit)
    {
        double best = 1e9; Params bp;
        for (float t = 2; t <= 10; t += 1)
            for (float m = 2; m <= 8; m += 1)
                for (float b = 0; b <= 8; b += 2)
                    for (int br = 0; br <= 1; ++br)
                    {
                        Params p; p.reverb = 0; p.treble = t; p.middle = m; p.bass = b; p.bright = br == 1;
                        std::vector<float> L, R; run (p, riff, L, R);
                        double lv[kNumBands]; bandLevels (L, lv);
                        const double e = fitError (lv);
                        if (e < best) { best = e; bp = p; }
                    }
        std::printf ("best fit: treble %.0f middle %.0f bass %.0f bright %s -> RMS error %.1f dB\n",
                     bp.treble, bp.middle, bp.bass, bp.bright ? "on" : "off", best);
    }

    // 3. Held chords: a G chord left to ring for 8 s, pedal off and on.
    for (bool dist : { false, true })
    {
        std::vector<float> in ((size_t) (8.0 * kOs), 0.0f);
        uint32_t rng = 9;
        for (double f : { 98.0, 123.5, 146.8, 196.0, 246.9, 392.0 })
        {
            Pluck p (f, 0.1f, rng, 0.99995f);
            for (auto& v : in) v += p.tick();
        }
        Params p; p.distOn = dist; p.reverb = 0;
        Chain c; c.prepare (kOs); c.setParams (p); c.setTransport (false, 0.0, 0.0);
        const size_t win = (size_t) (0.25 * kOs);
        double prev = 0, worst = 0; bool ok = true;
        for (size_t s0 = 0; s0 + win <= in.size(); s0 += win)
        {
            double e = 0;
            for (size_t i = s0; i < s0 + win; ++i) { float l, r; c.process (in[i], l, r); e += (double) l * l; }
            const double db = 10 * std::log10 (e / win + 1e-30);
            if (! std::isfinite (db)) ok = false;
            if (s0 >= 2 * win) worst = std::max (worst, std::abs (db - prev));
            prev = db;
        }
        std::printf ("held G chord, distortion %s: biggest 0.25 s level step %.1f dB\n", dist ? "on " : "off", worst);
        if (! ok || ! (worst < 3.0)) gFailed = true;
    }

    // 4. Level and the demo: clean, then pulsing distortion (with spring).
    {
        const auto in = arpeggios (8.0, 0.12f);
        std::vector<float> L1, R1, L2, R2;
        Params clean; run (clean, std::vector<float> (in.begin(), in.begin() + in.size() / 2), L1, R1, 140.0);
        Params pulsing; pulsing.distOn = true; pulsing.pulse = 2;
        run (pulsing, std::vector<float> (in.begin() + in.size() / 2, in.end()), L2, R2, 140.0);
        L1.insert (L1.end(), L2.begin(), L2.end()); R1.insert (R1.end(), R2.begin(), R2.end());
        std::vector<float> dl, dr; double peak = 0; bool finite = true;
        float a1 = 0, a2 = 0, b1 = 0, b2 = 0;
        for (size_t i = 0; i < L1.size(); ++i)
        {
            a1 += 0.25f * (L1[i] - a1); a2 += 0.25f * (a1 - a2); b1 += 0.25f * (R1[i] - b1); b2 += 0.25f * (b1 - b2);
            if (i % 4 == 3) { dl.push_back (a2); dr.push_back (b2); finite &= std::isfinite (a2) && std::isfinite (b2);
                              peak = std::max ({ peak, (double) std::abs (a2), (double) std::abs (b2) }); }
        }
        std::printf ("defaults + pulse demo: peak %.1f dBFS, %s\n", 20 * std::log10 (peak + 1e-12), finite ? "finite" : "NON-FINITE");
        if (! finite || peak > 1.0) gFailed = true;
        writeWavStereo ("cowboy_demo.wav", dl, dr);
    }

    std::printf ("\n%s\n", gFailed ? "CHECKS FAILED" : "all checks passed");
    return gFailed ? 1 : 0;
}
