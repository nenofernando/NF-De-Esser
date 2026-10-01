#pragma once
// NF De-Esser DSP. JUCE-free so the unit tests can run it anywhere.
//
// Split-band de-esser: a band-pass around the chosen frequency is both the detector and the part that gets turned down,
// so everything outside the sibilance band passes through untouched.
//   band  = bandpass(x, freq)                      (unity gain at the centre)
//   level = envelope of max(|bandL|, |bandR|)      (stereo linked)
//   red   = soft-knee reduction above Threshold, never more than Range
//   Split (default): y = x + (g - 1) * band,   g = 10^(-red/20)   -> only the band is turned down
//   Wide:            y = x * g                                      -> the whole signal is turned down (same detector)
// Listen outputs the band itself (what the detector hears).
#include <algorithm>
#include <cmath>

namespace nfdeesser
{
inline constexpr double kMinFreqHz = 2000.0, kMaxFreqHz = 12000.0;
inline constexpr double kMaxRangeDb = 20.0;

struct Parameters
{
    double freqHz = 6500.0;
    double thresholdDb = -20.0;
    double rangeDb = 8.0;     // maximum reduction
    bool listen = false;
    bool wide = false;        // false = Split (only the band), true = Wide (whole signal)
};

class DeEsser
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        attackCoef = std::exp(-1.0 / (0.0004 * sr));    // 0.4 ms
        releaseCoef = std::exp(-1.0 / (0.040 * sr));    // 40 ms
        reset();
        updateFilter();
    }
    void reset()
    {
        for (auto& f : bp) f.z1 = f.z2 = 0.0;
        env = 0.0; reductionDb = 0.0;
    }
    void setParameters(const Parameters& p)
    {
        params = p;
        params.freqHz = std::min(kMaxFreqHz, std::max(kMinFreqHz, params.freqHz));
        params.rangeDb = std::min(kMaxRangeDb, std::max(0.0, params.rangeDb));
        if (std::abs(params.freqHz - filterFreq) > 0.5) updateFilter();
    }

    void processSample(float& left, float& right)
    {
        const double xl = left, xr = right;
        const double bl = bp[0].process(xl), br = bp[1].process(xr);

        // detector: peak envelope of the band, stereo linked
        const double level = std::max(std::abs(bl), std::abs(br));
        env = level > env ? attackCoef * env + (1.0 - attackCoef) * level
                          : releaseCoef * env + (1.0 - releaseCoef) * level;
        const double envDb = 20.0 * std::log10(env + 1.0e-9);

        // gain computer: soft knee (6 dB), ratio 4:1 on the part above the threshold, limited by Range
        const double over = envDb - params.thresholdDb;
        double kneed = 0.0;
        if (over >= 3.0) kneed = over;
        else if (over > -3.0) kneed = (over + 3.0) * (over + 3.0) / 12.0;
        reductionDb = std::min(params.rangeDb, kneed * 0.75);

        if (params.listen) { left = (float) bl; right = (float) br; return; }

        const double g = std::pow(10.0, -reductionDb / 20.0);
        if (params.wide) { left = (float) (xl * g); right = (float) (xr * g); return; }
        left = (float) (xl + (g - 1.0) * bl);
        right = (float) (xr + (g - 1.0) * br);
    }

    double gainReductionDb() const { return reductionDb; }
    // Level the detector hears (dB, band-passed, smoothed): drives the input meter next to the Threshold fader.
    double detectorLevelDb() const { return 20.0 * std::log10(env + 1.0e-9); }

private:
    struct Biquad
    {
        double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process(double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void updateFilter()
    {
        // RBJ band-pass, constant 0 dB peak gain
        constexpr double Q = 1.4;
        const double w0 = 2.0 * 3.14159265358979323846 * params.freqHz / sr;
        const double alpha = std::sin(w0) / (2.0 * Q), cw = std::cos(w0), a0 = 1.0 + alpha;
        for (auto& f : bp)
        {
            f.b0 = alpha / a0; f.b1 = 0.0; f.b2 = -alpha / a0;
            f.a1 = -2.0 * cw / a0; f.a2 = (1.0 - alpha) / a0;
        }
        filterFreq = params.freqHz;
    }

    double sr = 48000.0, attackCoef = 0.0, releaseCoef = 0.0, env = 0.0, reductionDb = 0.0, filterFreq = -1.0;
    Parameters params;
    Biquad bp[2];
};
}
