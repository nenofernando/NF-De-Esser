#pragma once
// NF De-Esser DSP. JUCE-free so the unit tests can run it anywhere.
//
// Split-band de-esser with look-ahead. Two band-passes around the chosen frequency:
//   band     = narrow 4th-order band-pass (two cascaded 2nd-order stages): the DETECTOR (side-chain) and the Listen monitor, so the body of
//              the voice never reaches the detector
//   wideBand = wider 2nd-order band-pass (Q 0.5): the part that is TURNED DOWN, so the whole sibilance region (not a thin slice around the
//              centre) is reduced and the effect is clearly audible
//   level    = envelope of max(|bandL|, |bandR|), stereo linked, read on a calibrated scale (kDetectorGainDb) so a typical "s" sits in the
//              upper-middle of the Threshold scale (-40..0 dB) instead of at its very bottom
//   red      = soft-knee reduction above Threshold, never more than Range (the amount at the centre frequency)
//   Target (default): y = x + (g - 1) * wideBand,   g = 10^(-red/20)   -> only the sibilance region is turned down (a dynamic bell)
//   Full:             y = x * g                                         -> the whole signal is turned down (same detector)
// The detector sees the signal kLookAheadMs BEFORE the audio is heard (the audio is delayed), so the reduction is already in place when an
// "s" starts: the first milliseconds of the burst no longer slip through. The delay is reported to the host as latency, also when Power is off.
// Listen is the classic side-chain monitor: it always plays what the detector hears (the narrow band at Frequency), whatever the Threshold is.
#include <algorithm>
#include <cmath>
#include <vector>

namespace nfdeesser
{
inline constexpr double kMinFreqHz = 500.0, kMaxFreqHz = 16000.0;   // same span as the Avid De-Esser III (500 Hz - 16 kHz)
inline constexpr double kMaxRangeDb = 20.0;
inline constexpr double kApplyQ = 0.5;          // Q of the band that is turned down (lower = wider)
inline constexpr double kSlope = 0.9;           // 1 - 1/ratio: 10:1 (it bites: lowering Threshold squeezes the "s" hard)
inline constexpr double kListenGain = 3.0;      // Listen lift (+9.5 dB): the side-chain band is quiet by nature; soft-limited with tanh
inline constexpr double kDetectorGainDb = 16.0; // calibration: measured on a real vocal, an "s" read -18..-23 dB raw and the vowels -37..-43 dB
inline constexpr double kLookAheadMs = 1.5;

struct Parameters
{
    double freqHz = 6500.0;
    double thresholdDb = -20.0;
    double rangeDb = 12.0;    // maximum reduction
    bool listen = false;      // side-chain monitor
    bool wide = false;        // false = Target (only the band), true = Full (whole signal)
};

class DeEsser
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        attackCoef = std::exp(-1.0 / (0.0004 * sr));    // 0.4 ms
        releaseCoef = std::exp(-1.0 / (0.018 * sr));    // 18 ms: the next vowel is not left dull
        lookAhead = (int) std::lround(kLookAheadMs * 0.001 * sr);
        for (auto& d : delay) d.assign((size_t) std::max(lookAhead, 1), 0.0);   // read-then-write ring: the delay is exactly its size
        reset();
        filterFreq = -1.0;
        updateFilter();
    }
    // Delay (samples) between the input and the audio: report it to the host as latency.
    int latencySamples() const { return lookAhead; }
    void reset()
    {
        for (auto& f : bp) f.z1 = f.z2 = 0.0;
        for (auto& f : bp2) f.z1 = f.z2 = 0.0;
        for (auto& f : ap) f.z1 = f.z2 = 0.0;
        for (auto& d : delay) std::fill(d.begin(), d.end(), 0.0);
        pos = 0; env = 0.0; reductionDb = 0.0;
    }
    void setParameters(const Parameters& p)
    {
        params = p;
        params.freqHz = std::min(std::min(kMaxFreqHz, 0.45 * sr), std::max(kMinFreqHz, params.freqHz));
        params.rangeDb = std::min(kMaxRangeDb, std::max(0.0, params.rangeDb));
        if (std::abs(params.freqHz - filterFreq) > 0.5) updateFilter();
    }

    // Power off: the untouched input, with the same delay so switching Power never shifts the timing.
    void bypassSample(float& left, float& right)
    {
        double dl = left, dr = right;
        delayed(dl, dr);
        left = (float) dl; right = (float) dr;
        reductionDb = 0.0;
    }

    void processSample(float& left, float& right)
    {
        const double xl = left, xr = right;
        // detector and Listen: the narrow band of the CURRENT input
        const double bl = bp2[0].process(bp[0].process(xl)), br = bp2[1].process(bp[1].process(xr));

        // peak envelope of the band, stereo linked, on the calibrated scale
        const double level = std::max(std::abs(bl), std::abs(br));
        env = level > env ? attackCoef * env + (1.0 - attackCoef) * level
                          : releaseCoef * env + (1.0 - releaseCoef) * level;
        const double envDb = 20.0 * std::log10(env + 1.0e-9) + kDetectorGainDb;

        // gain computer: soft knee (6 dB), then the slope above the threshold, limited by Range
        const double over = envDb - params.thresholdDb;
        double kneed = 0.0;
        if (over >= 3.0) kneed = over;
        else if (over > -3.0) kneed = (over + 3.0) * (over + 3.0) / 12.0;
        reductionDb = std::min(params.rangeDb, kneed * kSlope);

        // the audio that is turned down is the DELAYED one: the reduction is ready before the "s" arrives
        double dl = xl, dr = xr;
        delayed(dl, dr);

        if (params.listen)   // side-chain monitor: what the detector hears, lifted and soft-limited so it can never overload
        {
            left = (float) std::tanh(bl * kListenGain);
            right = (float) std::tanh(br * kListenGain);
            return;
        }

        const double g = std::pow(10.0, -reductionDb / 20.0);
        if (params.wide) { left = (float) (dl * g); right = (float) (dr * g); return; }
        const double wl = ap[0].process(dl), wr = ap[1].process(dr);   // the wider band that is turned down
        left = (float) (dl + (g - 1.0) * wl);
        right = (float) (dr + (g - 1.0) * wr);
    }

    double gainReductionDb() const { return reductionDb; }
    // Level the detector hears (dB, calibrated scale, smoothed): drives the input meter next to the Threshold fader.
    double detectorLevelDb() const { return 20.0 * std::log10(env + 1.0e-9) + kDetectorGainDb; }

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

    // pushes the current samples and returns the ones from lookAhead samples ago
    void delayed(double& l, double& r)
    {
        if (lookAhead <= 0) return;
        const size_t n = delay[0].size();
        const double ol = delay[0][pos], orr = delay[1][pos];
        delay[0][pos] = l; delay[1][pos] = r;
        pos = (pos + 1) % n;
        l = ol; r = orr;
    }

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
        // the wider band that is turned down (2nd order)
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
    int lookAhead = 0;
    size_t pos = 0;
    Parameters params;
    Biquad bp[2], bp2[2];   // detector / Listen: two cascaded stages per channel
    Biquad ap[2];           // the band that is turned down
    std::vector<double> delay[2];
};
}
