#pragma once
// NF De-Esser DSP. JUCE-free so the unit tests can run it anywhere.
//
// Split-band de-esser. Two band-passes around the chosen frequency:
//   band  = narrow 4th-order band-pass (two cascaded 2nd-order stages): the DETECTOR and the Listen monitor, so the voice body
//           never reaches the detector and Listen plays only the "s"
//   wideBand = wider 2nd-order band-pass (Q 0.5): the part that is TURNED DOWN, so the whole sibilance region (not only a thin slice
//           around the centre) is reduced and the effect is clearly audible
//   level = envelope of max(|bandL|, |bandR|)      (stereo linked)
//   red   = soft-knee reduction above Threshold, never more than Range (the amount at the centre frequency)
//   Split (default): y = x + (g - 1) * wideBand,   g = 10^(-red/20)   -> only the sibilance region is turned down (like a dynamic bell)
//   Wide:            y = x * g                                         -> the whole signal is turned down (same detector)
// Listen outputs what is being REMOVED: (1 - g) * band (the narrow detector band) with a +14 dB lift (soft-limited with tanh), so it follows
// Threshold and Range: silent while nothing is reduced, louder the more is taken out, and it never brings the body of the voice.
#include <algorithm>
#include <cmath>

namespace nfdeesser
{
inline constexpr double kMinFreqHz = 2000.0, kMaxFreqHz = 12000.0;
inline constexpr double kMaxRangeDb = 20.0;
inline constexpr double kApplyQ = 0.5;    // Q of the band that is turned down (lower = wider)
inline constexpr double kSlope = 0.9;     // 1 - 1/ratio: 10:1
inline constexpr double kListenGain = 5.0; // Listen makeup (+14 dB): what is taken out is quiet by nature, so it is lifted to be clearly audible

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
        releaseCoef = std::exp(-1.0 / (0.018 * sr));    // 18 ms: the next vowel is not left dull
        reset();
        updateFilter();
    }
    void reset()
    {
        for (auto& f : bp) f.z1 = f.z2 = 0.0;
        for (auto& f : bp2) f.z1 = f.z2 = 0.0;
        for (auto& f : ap) f.z1 = f.z2 = 0.0;
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
        const double bl = bp2[0].process(bp[0].process(xl)), br = bp2[1].process(bp[1].process(xr));
        const double wl = ap[0].process(xl), wr = ap[1].process(xr);   // the wider band that is actually turned down

        // detector: peak envelope of the band, stereo linked
        const double level = std::max(std::abs(bl), std::abs(br));
        env = level > env ? attackCoef * env + (1.0 - attackCoef) * level
                          : releaseCoef * env + (1.0 - releaseCoef) * level;
        const double envDb = 20.0 * std::log10(env + 1.0e-9);

        // gain computer: soft knee (6 dB), ratio 10:1 on the part above the threshold (it bites: lowering Threshold squeezes the "s" hard), limited by Range
        const double over = envDb - params.thresholdDb;
        double kneed = 0.0;
        if (over >= 3.0) kneed = over;
        else if (over > -3.0) kneed = (over + 3.0) * (over + 3.0) / 12.0;
        reductionDb = std::min(params.rangeDb, kneed * kSlope);

        const double g = std::pow(10.0, -reductionDb / 20.0);
        if (params.listen)   // what is being taken out, lifted by kListenGain and soft-limited so it can never overload
        {
            left = (float) std::tanh((1.0 - g) * bl * kListenGain);
            right = (float) std::tanh((1.0 - g) * br * kListenGain);
            return;
        }

        if (params.wide) { left = (float) (xl * g); right = (float) (xr * g); return; }
        left = (float) (xl + (g - 1.0) * wl);
        right = (float) (xr + (g - 1.0) * wr);
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
        // RBJ band-pass, constant 0 dB peak gain; two identical stages = 4th order (24 dB/oct skirts, about -42 dB at 1 kHz for a 6.5 kHz band)
        constexpr double Q = 1.8;
        const double w0 = 2.0 * 3.14159265358979323846 * params.freqHz / sr;
        const double alpha = std::sin(w0) / (2.0 * Q), cw = std::cos(w0), a0 = 1.0 + alpha;
        for (auto* stage : { &bp[0], &bp[1], &bp2[0], &bp2[1] })
        {
            auto& f = *stage;
            f.b0 = alpha / a0; f.b1 = 0.0; f.b2 = -alpha / a0;
            f.a1 = -2.0 * cw / a0; f.a2 = (1.0 - alpha) / a0;
        }
        // the wider band that is turned down (Q 0.5, 2nd order)
        constexpr double Qa = kApplyQ;
        const double alphaA = std::sin(w0) / (2.0 * Qa), a0a = 1.0 + alphaA;
        for (auto& f : ap)
        {
            f.b0 = alphaA / a0a; f.b1 = 0.0; f.b2 = -alphaA / a0a;
            f.a1 = -2.0 * cw / a0a; f.a2 = (1.0 - alphaA) / a0a;
        }
        filterFreq = params.freqHz;
    }

    double sr = 48000.0, attackCoef = 0.0, releaseCoef = 0.0, env = 0.0, reductionDb = 0.0, filterFreq = -1.0;
    Parameters params;
    Biquad bp[2], bp2[2];   // detector / Listen: two cascaded stages per channel
    Biquad ap[2];           // the band that is turned down
};
}
