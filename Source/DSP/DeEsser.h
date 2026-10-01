#pragma once
// NF De-Esser DSP. JUCE-free so the unit tests can run it anywhere.
//
// Architecture of the classic de-essers (Waves DeEsser, Avid De-Esser III): a SIDE-CHAIN that measures the "s" and an AUDIO path that is turned down.
//
//   SIDE-CHAIN (filter, energy detector, compressor):
//     HIGH-PASS: 4th-order high-pass at Frequency: the detector "looks at" everything above it (all kinds of "s" and "sh")   [default]
//     BAND-PASS: narrow 4th-order band-pass around Frequency: isolates one kind of "s"
//     The reading is peak-detected (stereo linked) on a calibrated scale (kDetectorGainDb) so a typical "s" sits in the upper part of the
//     Threshold scale, then a hard-ish knee compressor turns the energy above Threshold into attenuation, never more than Range.
//   AUDIO path:
//     Target (Split): the audio is split at Frequency (Linkwitz-Riley 4th-order crossover) and only the HIGH part is turned down: low = LP(x),
//                     y = low + g * high. The low part is never touched, the reduction reaches down to roughly 3 kHz for a 5.5 kHz setting.
//     Full (Wideband): y = x * g, the whole signal is turned down.
//   LOOK-AHEAD: the side-chain sees the signal kLookAheadMs BEFORE the audio is heard, so the reduction is already in place when an "s" starts.
//     The delay is reported to the host as latency (also when Power is off).
//   LISTEN: the side-chain monitor, it always plays what the side-chain hears (high-pass: everything above Frequency; band-pass: the band).
#include <algorithm>
#include <cmath>
#include <vector>

namespace nfdeesser
{
inline constexpr double kMinFreqHz = 500.0, kMaxFreqHz = 16000.0;   // same span as the Avid De-Esser III (500 Hz - 16 kHz)
inline constexpr double kMaxRangeDb = 20.0;
inline constexpr double kSlope = 0.9;           // 1 - 1/ratio: 10:1 (it bites: lowering Threshold squeezes the "s" hard)
inline constexpr double kKneeDb = 2.0;          // nearly hard knee
inline constexpr double kDetectorGainDb = 16.0; // calibration: measured on a real vocal, an "s" read -18..-23 dB raw and the vowels -34..-43 dB
inline constexpr double kListenGainBand = 3.0, kListenGainHigh = 1.5;   // Listen lift, soft-limited with tanh
inline constexpr double kLookAheadMs = 1.5;
inline constexpr double kCrossRatio = 0.65;     // Split crossover = Frequency x this (a crossover only reaches its full reduction well above its corner)

struct Parameters
{
    double freqHz = 6500.0;
    double thresholdDb = -20.0;
    double rangeDb = 12.0;    // maximum reduction
    bool listen = false;      // side-chain monitor
    bool wide = false;        // false = Target (Split: only the highs), true = Full (whole signal)
    bool highPass = true;     // side-chain filter: true = high-pass, false = band-pass
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
        for (int c = 0; c < 2; ++c)
        {
            bp[c].clear(); bp2[c].clear(); hd1[c].clear(); hd2[c].clear();
            lo1[c].clear(); lo2[c].clear(); hi1[c].clear(); hi2[c].clear();
            std::fill(delay[c].begin(), delay[c].end(), 0.0);
        }
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

        // ---- side-chain: the filtered CURRENT input ----
        double sl, sr_;
        if (params.highPass) { sl = hd2[0].process(hd1[0].process(xl)); sr_ = hd2[1].process(hd1[1].process(xr)); }
        else                 { sl = bp2[0].process(bp[0].process(xl));  sr_ = bp2[1].process(bp[1].process(xr)); }

        // energy detector: peak envelope, stereo linked, on the calibrated scale
        const double level = std::max(std::abs(sl), std::abs(sr_));
        env = level > env ? attackCoef * env + (1.0 - attackCoef) * level
                          : releaseCoef * env + (1.0 - releaseCoef) * level;
        const double envDb = 20.0 * std::log10(env + 1.0e-9) + kDetectorGainDb;

        // compressor: nearly hard knee, then the slope above the threshold, limited by Range
        const double over = envDb - params.thresholdDb;
        double kneed = 0.0;
        if (over >= kKneeDb * 0.5) kneed = over;
        else if (over > -kKneeDb * 0.5) kneed = (over + kKneeDb * 0.5) * (over + kKneeDb * 0.5) / (2.0 * kKneeDb);
        reductionDb = std::min(params.rangeDb, kneed * kSlope);

        // ---- audio: the DELAYED signal is turned down, the reduction is ready before the "s" arrives ----
        double dl = xl, dr = xr;
        delayed(dl, dr);

        if (params.listen)   // side-chain monitor: what the side-chain hears, lifted and soft-limited so it can never overload
        {
            const double gain = params.highPass ? kListenGainHigh : kListenGainBand;
            left = (float) std::tanh(sl * gain);
            right = (float) std::tanh(sr_ * gain);
            return;
        }

        const double g = std::pow(10.0, -reductionDb / 20.0);
        if (params.wide) { left = (float) (dl * g); right = (float) (dr * g); return; }

        // Split: low part untouched, high part turned down (Linkwitz-Riley 4th-order crossover at Frequency)
        const double ll = lo2[0].process(lo1[0].process(dl)), lr = lo2[1].process(lo1[1].process(dr));
        const double hl = hi2[0].process(hi1[0].process(dl)), hr = hi2[1].process(hi1[1].process(dr));
        left = (float) (ll + g * hl);
        right = (float) (lr + g * hr);
    }

    double gainReductionDb() const { return reductionDb; }
    // Level the side-chain hears (dB, calibrated scale, smoothed): drives the input meter next to the Threshold fader.
    double detectorLevelDb() const { return 20.0 * std::log10(env + 1.0e-9) + kDetectorGainDb; }

private:
    struct Biquad
    {
        double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void clear() { z1 = z2 = 0.0; }
        double process(double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void setBandpass(double w0, double Q)   // constant 0 dB peak gain
        {
            const double alpha = std::sin(w0) / (2.0 * Q), cw = std::cos(w0), a0 = 1.0 + alpha;
            b0 = alpha / a0; b1 = 0.0; b2 = -alpha / a0; a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
        }
        void setHighpass(double w0, double Q)
        {
            const double alpha = std::sin(w0) / (2.0 * Q), cw = std::cos(w0), a0 = 1.0 + alpha;
            b0 = (1.0 + cw) * 0.5 / a0; b1 = -(1.0 + cw) / a0; b2 = b0; a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
        }
        void setLowpass(double w0, double Q)
        {
            const double alpha = std::sin(w0) / (2.0 * Q), cw = std::cos(w0), a0 = 1.0 + alpha;
            b0 = (1.0 - cw) * 0.5 / a0; b1 = (1.0 - cw) / a0; b2 = b0; a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
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
        const double w0 = 2.0 * 3.14159265358979323846 * params.freqHz / sr;
        const double wx = w0 * kCrossRatio;
        constexpr double Qbw = 0.70710678118654752;   // Butterworth: two identical stages = Linkwitz-Riley 4th order
        for (int c = 0; c < 2; ++c)
        {
            bp[c].setBandpass(w0, 1.8);  bp2[c].setBandpass(w0, 1.8);    // band-pass side-chain: 4th order, about -42 dB at 1 kHz for 6.5 kHz
            hd1[c].setHighpass(w0, Qbw); hd2[c].setHighpass(w0, Qbw);    // high-pass side-chain: 4th order
            lo1[c].setLowpass(wx, Qbw);  lo2[c].setLowpass(wx, Qbw);     // crossover, low part
            hi1[c].setHighpass(wx, Qbw); hi2[c].setHighpass(wx, Qbw);    // crossover, high part
        }
        filterFreq = params.freqHz;
    }

    double sr = 48000.0, attackCoef = 0.0, releaseCoef = 0.0, env = 0.0, reductionDb = 0.0, filterFreq = -1.0;
    int lookAhead = 0;
    size_t pos = 0;
    Parameters params;
    Biquad bp[2], bp2[2], hd1[2], hd2[2];   // side-chain filters
    Biquad lo1[2], lo2[2], hi1[2], hi2[2];  // audio crossover
    std::vector<double> delay[2];
};
}
