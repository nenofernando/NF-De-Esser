#include "../Source/DSP/DeEsser.h"
#include "../Source/FactoryPresets.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace nfdeesser;
static constexpr double kPi = 3.14159265358979323846;

// Level (dB) of one frequency in a buffer (Goertzel-style single-bin DFT over the second half, steady state).
static double toneDb(const std::vector<float>& x, double f, double sr)
{
    const size_t n0 = x.size() / 2;
    double re = 0.0, im = 0.0;
    for (size_t i = n0; i < x.size(); ++i) { const double w = 2.0 * kPi * f * (double) i / sr; re += x[i] * std::cos(w); im -= x[i] * std::sin(w); }
    const double amp = 2.0 * std::sqrt(re * re + im * im) / (double)(x.size() - n0);
    return 20.0 * std::log10(amp + 1e-12);
}

// Runs a two-tone signal (a low tone and a sibilance tone) through the de-esser; returns left output.
static std::vector<float> run(const Parameters& p, double lowHz, double lowDb, double sibHz, double sibDb, double sr, double seconds, double* lastReduction = nullptr)
{
    DeEsser d; d.prepare(sr); d.setParameters(p);
    const int n = (int)(sr * seconds);
    std::vector<float> out((size_t) n);
    const double la = std::pow(10.0, lowDb / 20.0), sa = std::pow(10.0, sibDb / 20.0);
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        float l = (float)(la * std::sin(2.0 * kPi * lowHz * t) + sa * std::sin(2.0 * kPi * sibHz * t)), r = l;
        d.processSample(l, r);
        out[(size_t) i] = l;
    }
    if (lastReduction) *lastReduction = d.gainReductionDb();
    return out;
}

int main()
{
    const double sr = 48000.0;
    Parameters p; p.freqHz = 6500.0; p.thresholdDb = -30.0; p.rangeDb = 12.0;

    // A loud sibilance tone at the chosen frequency is turned down, but never by more than Range.
    {
        double red = 0.0;
        auto y = run(p, 200.0, -20.0, 6500.0, -12.0, sr, 1.0, &red);
        const double sibOut = toneDb(y, 6500.0, sr);
        assert(sibOut < -12.0 - 6.0);                 // at least 6 dB down
        assert(sibOut > -12.0 - 12.0 - 0.7);          // not beyond Range (small tolerance)
        assert(red > 6.0 && red <= 12.0 + 1e-9);
    }
    // The low tone (outside the band) passes untouched.
    {
        auto y = run(p, 200.0, -20.0, 6500.0, -12.0, sr, 1.0);
        assert(std::abs(toneDb(y, 200.0, sr) - (-20.0)) < 0.3);
    }
    // Below the threshold nothing happens at all.
    {
        double red = 0.0;
        auto y = run(p, 200.0, -30.0, 6500.0, -60.0, sr, 1.0, &red);
        assert(std::abs(toneDb(y, 6500.0, sr) - (-60.0)) < 0.2);
        assert(red < 0.01);
    }
    // Range 0 = unity; Range follows the knob.
    {
        Parameters q = p; q.rangeDb = 0.0;
        auto y = run(q, 200.0, -20.0, 6500.0, -12.0, sr, 1.0);
        assert(std::abs(toneDb(y, 6500.0, sr) - (-12.0)) < 0.2);
        Parameters s = p; s.rangeDb = 4.0;
        auto z = run(s, 200.0, -20.0, 6500.0, -12.0, sr, 1.0);
        const double d = toneDb(z, 6500.0, sr);
        assert(d < -12.0 - 3.0 && d > -12.0 - 4.7);
    }
    // The band follows Frequency: a tone far from the set frequency is hardly touched.
    {
        Parameters q = p; q.freqHz = 3000.0;
        auto y = run(q, 200.0, -20.0, 9000.0, -12.0, sr, 1.0);
        assert(std::abs(toneDb(y, 9000.0, sr) - (-12.0)) < 2.0);
    }
    // Listen plays the band: the 6.5 kHz tone passes, the 200 Hz tone is far down.
    {
        Parameters q = p; q.listen = true;
        auto y = run(q, 200.0, -10.0, 6500.0, -10.0, sr, 1.0);
        assert(toneDb(y, 6500.0, sr) > -11.0);
        assert(toneDb(y, 200.0, sr) < -40.0);
    }
    // Stability: loud noise at every frequency setting stays finite and bounded, reduction never above Range.
    {
        for (double f : { 2000.0, 5000.0, 12000.0 })
        {
            DeEsser d; d.prepare(sr); Parameters q = p; q.freqHz = f; q.thresholdDb = -40.0; q.rangeDb = 20.0; d.setParameters(q);
            unsigned s = 12345u; double peak = 0.0;
            for (int i = 0; i < (int)(sr * 2); ++i)
            {
                s = s * 1664525u + 1013904223u;
                float l = ((float)((s >> 8) & 0xffff) / 32768.0f - 1.0f) * 0.8f, r = -l;
                d.processSample(l, r);
                assert(std::isfinite(l) && std::isfinite(r));
                peak = std::max(peak, (double) std::abs(l));
                assert(d.gainReductionDb() <= 20.0 + 1e-9 && d.gainReductionDb() >= 0.0);
            }
            assert(peak < 2.0);
        }
    }
    // Mono-ish safety: silence in = silence out.
    {
        DeEsser d; d.prepare(sr); d.setParameters(p);
        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < 1000; ++i) { l = 0.0f; r = 0.0f; d.processSample(l, r); assert(l == 0.0f && r == 0.0f); }
    }

    // Factory presets: unique names, every value inside its knob's range.
    assert(kNumFactoryPresets >= 10);
    for (int i = 0; i < kNumFactoryPresets; ++i)
    {
        const auto& f = kFactoryPresets[i];
        assert(f.freqHz >= kMinFreqHz && f.freqHz <= kMaxFreqHz);
        assert(f.thresholdDb >= -40.0f && f.thresholdDb <= 0.0f);
        assert(f.rangeDb >= 0.0f && f.rangeDb <= (float) kMaxRangeDb);
        assert(f.outputDb >= -12.0f && f.outputDb <= 12.0f);
        for (int j = i + 1; j < kNumFactoryPresets; ++j) assert(std::strcmp(f.name, kFactoryPresets[j].name) != 0);
    }
    std::cout << "NF De-Esser DSP tests passed\n";
    return 0;
}
