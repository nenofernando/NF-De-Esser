#undef NDEBUG   // the checks must run in Release builds too
#include "../Source/DSP/DeEsser.h"
#include "../Source/FactoryPresets.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace nfdeesser;
static constexpr double kPi = 3.14159265358979323846;

// Level (dB) of one frequency in a buffer (single-bin DFT over the second half, steady state).
static double toneDb(const std::vector<float>& x, double f, double sr)
{
    const size_t n0 = x.size() / 2;
    double re = 0.0, im = 0.0;
    for (size_t i = n0; i < x.size(); ++i) { const double w = 2.0 * kPi * f * (double) i / sr; re += x[i] * std::cos(w); im -= x[i] * std::sin(w); }
    return 20.0 * std::log10(2.0 * std::sqrt(re * re + im * im) / (double)(x.size() - n0) + 1e-12);
}

// Runs a two-tone signal (a low tone and a sibilance tone) through the de-esser; returns the left output.
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
    Parameters p; p.freqHz = 5000.0; p.thresholdDb = -30.0; p.rangeDb = 12.0; p.highPass = true;   // the default side-chain: high-pass

    // TARGET (Split): a loud "s" above Frequency is turned down, never by more than Range; the low part is left alone.
    {
        double red = 0.0;
        auto y = run(p, 200.0, -20.0, 9000.0, -12.0, sr, 1.0, &red);
        const double sibOut = toneDb(y, 9000.0, sr);
        assert(sibOut < -12.0 - 8.0);                 // well down
        assert(sibOut > -12.0 - 12.0 - 0.8);          // not beyond Range
        assert(red > 6.0 && red <= 12.0 + 1e-9);
        assert(std::abs(toneDb(y, 200.0, sr) - (-20.0)) < 0.3);   // the low tone passes untouched
    }
    // Below the threshold nothing happens at all.
    {
        double red = 0.0;
        auto y = run(p, 200.0, -30.0, 9000.0, -60.0, sr, 1.0, &red);
        assert(std::abs(toneDb(y, 9000.0, sr) - (-60.0)) < 0.2);
        assert(red < 0.01);
    }
    // Range 0 = unity; Range follows the knob.
    {
        Parameters q = p; q.rangeDb = 0.0;
        auto y = run(q, 200.0, -20.0, 9000.0, -12.0, sr, 1.0);
        assert(std::abs(toneDb(y, 9000.0, sr) - (-12.0)) < 0.2);
        Parameters s = p; s.rangeDb = 4.0;
        auto z = run(s, 200.0, -20.0, 9000.0, -12.0, sr, 1.0);
        const double d = toneDb(z, 9000.0, sr);
        assert(d < -12.0 - 3.0 && d > -12.0 - 4.7);
    }
    // Side-chain filter: BAND-PASS isolates one kind of "s", HIGH-PASS looks at everything above Frequency.
    {
        Parameters band = p; band.highPass = false; band.freqHz = 6500.0; band.thresholdDb = -10.0;
        double redFar = 0.0, redAt = 0.0;
        run(band, 200.0, -60.0, 14000.0, -12.0, sr, 1.0, &redFar);   // far above the band
        run(band, 200.0, -60.0, 6500.0, -12.0, sr, 1.0, &redAt);     // inside the band
        assert(redFar < 0.01 && redAt > 3.0);
        Parameters high = band; high.highPass = true;
        double redHigh = 0.0;
        run(high, 200.0, -60.0, 14000.0, -12.0, sr, 1.0, &redHigh);  // the high-pass does see it
        assert(redHigh > 3.0);
    }
    // The Frequency range is 500 Hz - 16 kHz.
    assert(kMinFreqHz == 500.0 && kMaxFreqHz == 16000.0);
    // BAND listen: the band sits where the knob says.
    {
        for (double f0 : { 500.0, 1000.0, 2500.0, 6500.0, 12000.0, 16000.0 })
        {
            Parameters q; q.freqHz = f0; q.listen = true; q.highPass = false; q.thresholdDb = 0.0;
            auto at = run(q, 50.0, -120.0, f0, -20.0, sr, 0.5);
            auto below = run(q, 50.0, -120.0, f0 * 0.6, -20.0, sr, 0.5);
            auto above = run(q, 50.0, -120.0, std::min(f0 * 1.6, 21000.0), -20.0, sr, 0.5);
            assert(toneDb(at, f0, sr) > toneDb(below, f0 * 0.6, sr) + 6.0);
            assert(toneDb(at, f0, sr) > toneDb(above, std::min(f0 * 1.6, 21000.0), sr) + 6.0);
        }
    }
    // HIGH listen: plays everything ABOVE Frequency, nothing below.
    {
        for (double f0 : { 500.0, 1000.0, 2500.0, 5000.0, 9000.0 })
        {
            Parameters q; q.freqHz = f0; q.listen = true; q.highPass = true; q.thresholdDb = 0.0;
            auto above = run(q, 50.0, -120.0, f0 * 2.0, -20.0, sr, 0.5);
            auto below = run(q, 50.0, -120.0, f0 * 0.5, -20.0, sr, 0.5);
            assert(toneDb(above, f0 * 2.0, sr) > -20.0 - 1.0);                       // audible (lifted)
            assert(toneDb(below, f0 * 0.5, sr) < toneDb(above, f0 * 2.0, sr) - 20.0); // the part below is far down
        }
    }
    // LISTEN is the classic side-chain monitor: it ALWAYS plays what the side-chain hears, whatever the Threshold is.
    for (bool high : { true, false })
    {
        Parameters q = p; q.listen = true; q.highPass = high; q.freqHz = 6500.0;
        q.thresholdDb = 0.0;   // nothing is reduced at this threshold
        auto a = run(q, 200.0, -10.0, 6500.0, -30.0, sr, 1.0);
        q.thresholdDb = -40.0; // a lot is reduced at this one
        auto b = run(q, 200.0, -10.0, 6500.0, -30.0, sr, 1.0);
        const double la = toneDb(a, 6500.0, sr), lb = toneDb(b, 6500.0, sr);
        assert(la > -30.0 - 3.0);                    // audible (lifted by the Listen gain), not silent
        assert(std::abs(la - lb) < 0.3);             // independent of the Threshold
        assert(toneDb(a, 200.0, sr) < -10.0 - 40.0); // the low tone is not in the side-chain
    }
    // BAND listen does not bring the body of the voice: 1 kHz and 2 kHz are far down for a 6.5 kHz band.
    {
        Parameters q = p; q.listen = true; q.highPass = false; q.freqHz = 6500.0;
        auto y = run(q, 1000.0, -10.0, 6500.0, -10.0, sr, 1.0);
        assert(toneDb(y, 1000.0, sr) < -10.0 - 25.0);
        auto z = run(q, 2000.0, -10.0, 6500.0, -10.0, sr, 1.0);
        assert(toneDb(z, 2000.0, sr) < -10.0 - 12.0);
    }
    // Sweeping Frequency down into the mids with Listen on (high-pass), the mids are heard; with Frequency high they are not.
    {
        Parameters q; q.listen = true; q.highPass = true; q.freqHz = 1000.0;
        auto y = run(q, 2500.0, -20.0, 9000.0, -60.0, sr, 1.0);
        assert(toneDb(y, 2500.0, sr) > -20.0 - 1.0);
        q.freqHz = 6500.0;
        auto z = run(q, 2500.0, -20.0, 9000.0, -60.0, sr, 1.0);
        assert(toneDb(z, 2500.0, sr) < -20.0 - 20.0);
    }
    // The Threshold scale is calibrated (+22 dB): an "s" at -20 dBFS reads in the upper-middle of the scale on both side-chains.
    for (bool high : { true, false })
    {
        DeEsser d; d.prepare(sr); Parameters q = p; q.thresholdDb = 0.0; q.highPass = high; q.freqHz = high ? 5000.0 : 6500.0; d.setParameters(q);
        for (int i = 0; i < (int)(sr * 0.5); ++i) { float l = (float)(0.1 * std::sin(2.0 * kPi * 6500.0 * i / sr)), r = l; d.processSample(l, r); }
        assert(d.detectorLevelDb() > -4.0 && d.detectorLevelDb() < 6.0);
    }
    // FULL (wideband) turns the WHOLE signal down (the low tone follows the reduction); TARGET leaves it alone.
    {
        Parameters q = p; q.wide = true;
        double red = 0.0;
        auto y = run(q, 200.0, -20.0, 9000.0, -12.0, sr, 1.0, &red);
        assert(red > 6.0);
        assert(std::abs(toneDb(y, 200.0, sr) - (-20.0 - red)) < 0.6);
        auto z = run(q, 200.0, -30.0, 9000.0, -60.0, sr, 1.0);
        assert(std::abs(toneDb(z, 200.0, sr) - (-30.0)) < 0.2);   // below the threshold FULL is transparent too
    }
    // The reduction must be AUDIBLE on a real "sss": broadband noise limited to 4-10 kHz drops by most of what the meter shows,
    // and the body of the voice (1 kHz) is left alone while it is being reduced.
    for (bool high : { true, false })
    {
        struct BQ { double b0,b1,b2,a1,a2,z1=0,z2=0; double p(double x){ double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return y; } };
        auto hp = [&](double f){ double w=2*kPi*f/sr, al=std::sin(w)/(1.4), c=std::cos(w), a0=1+al; return BQ{(1+c)/2/a0,-(1+c)/a0,(1+c)/2/a0,-2*c/a0,(1-al)/a0}; };
        auto lp = [&](double f){ double w=2*kPi*f/sr, al=std::sin(w)/(1.4), c=std::cos(w), a0=1+al; return BQ{(1-c)/2/a0,(1-c)/a0,(1-c)/2/a0,-2*c/a0,(1-al)/a0}; };
        BQ h1=hp(4000.0), h2=hp(4000.0), l1=lp(10000.0), l2=lp(10000.0);
        DeEsser d; d.prepare(sr); Parameters q = p; q.highPass = high; q.freqHz = high ? 4500.0 : 6300.0; q.thresholdDb = -30.0; q.rangeDb = 8.0; d.setParameters(q);
        unsigned s = 1u; double in2 = 0.0, out2 = 0.0, red = 0.0; int cnt = 0; const int n = (int)(sr * 2.0);
        const int lat = d.latencySamples();
        std::vector<double> xin((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            s = s * 1664525u + 1013904223u;
            const double x = 0.25 * l2.p(l1.p(h2.p(h1.p(((s >> 8) & 0xffff) / 32768.0 - 1.0))));
            xin[(size_t) i] = x;
            float l = (float) x, r = l; d.processSample(l, r);
            if (i > n / 2 && i >= lat) { in2 += xin[(size_t)(i - lat)] * xin[(size_t)(i - lat)]; out2 += (double) l * l; red += d.gainReductionDb(); ++cnt; }
        }
        const double real = -10.0 * std::log10(out2 / in2), meter = red / cnt;
        assert(meter > 5.0 && real > 0.65 * meter);   // most of what the meter shows is really heard on the sibilance
        Parameters q2 = q; q2.thresholdDb = -30.0;
        auto y = run(q2, 1000.0, -20.0, 8000.0, -12.0, sr, 1.0);
        assert(std::abs(toneDb(y, 1000.0, sr) - (-20.0)) < 0.5);   // body of the voice untouched during reduction
    }
    // Look-ahead: the reduction is already in place when a burst arrives, so the first millisecond does not slip through.
    {
        DeEsser d; d.prepare(sr); Parameters q = p; q.thresholdDb = -30.0; q.rangeDb = 12.0; d.setParameters(q);
        const int lat = d.latencySamples();
        assert(lat == (int) std::lround(kLookAheadMs * 0.001 * sr));
        const int onset = (int)(0.1 * sr), n = (int)(0.4 * sr);
        std::vector<float> y((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            float l = i >= onset ? (float)(0.1 * std::sin(2.0 * kPi * 8000.0 * (i - onset) / sr)) : 0.0f, r = l;
            d.processSample(l, r); y[(size_t) i] = l;
        }
        // the audio arrives `lat` samples late: look from the very first delayed sample up to 0.6 ms later
        double firstPeak = 0.0, steadyPeak = 0.0;
        for (int i = onset + lat; i < onset + lat + (int)(0.0006 * sr); ++i) firstPeak = std::max(firstPeak, (double) std::abs(y[(size_t) i]));
        for (int i = onset + (int)(0.2 * sr); i < onset + (int)(0.3 * sr); ++i) steadyPeak = std::max(steadyPeak, (double) std::abs(y[(size_t) i]));
        assert(steadyPeak < 0.1 * 0.5);   // reduced in the steady state (> 6 dB)
        assert(firstPeak < 0.1 * 0.5);    // and already reduced by more than 6 dB from the very first sample of the burst (no look-ahead: no reduction yet)
    }
    // Power off (bypassSample) is the untouched input with the SAME delay, so the timing never jumps.
    {
        DeEsser d; d.prepare(sr); d.setParameters(p);
        const int lat = d.latencySamples();
        std::vector<float> out(512, 0.0f);
        for (int i = 0; i < 512; ++i) { float l = i == 10 ? 1.0f : 0.0f, r = l; d.bypassSample(l, r); out[(size_t) i] = l; }
        assert(out[(size_t)(10 + lat)] == 1.0f);
        int nonZero = 0; for (float v : out) if (v != 0.0f) ++nonZero;
        assert(nonZero == 1);
    }
    // Stability: loud noise at every frequency setting and side-chain stays finite and bounded, reduction never above Range.
    for (bool high : { true, false })
        for (double f : { 500.0, 5000.0, 16000.0 })
        {
            DeEsser d; d.prepare(sr); Parameters q = p; q.freqHz = f; q.highPass = high; q.thresholdDb = -40.0; q.rangeDb = 20.0; d.setParameters(q);
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
    // Silence in = silence out (also in Listen).
    for (bool listen : { false, true })
    {
        DeEsser d; d.prepare(sr); Parameters q = p; q.listen = listen; d.setParameters(q);
        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < 1000; ++i) { l = 0.0f; r = 0.0f; d.processSample(l, r); assert(l == 0.0f && r == 0.0f); }
    }

    // VOICE vs INSTR: a bright tone riding on a strong low body (a guitar pick) is left alone by the sibilance gate (VOICE)
    // and reduced by the plain level detector (INSTR); a tone with no body (an "s") is reduced either way.
    {
        Parameters q = p; q.freqHz = 6500.0; q.thresholdDb = -30.0; q.rangeDb = 8.0;
        double redVoice = 0.0, redInstr = 0.0, redSVoice = 0.0, redSInstr = 0.0;
        q.smart = true;  run(q, 200.0, -6.0, 8000.0, -22.0, sr, 1.0, &redVoice);   run(q, 200.0, -90.0, 8000.0, -22.0, sr, 1.0, &redSVoice);
        q.smart = false; run(q, 200.0, -6.0, 8000.0, -22.0, sr, 1.0, &redInstr);   run(q, 200.0, -90.0, 8000.0, -22.0, sr, 1.0, &redSInstr);
        assert(redVoice < 1.0);
        assert(redInstr > 3.0);
        assert(redSVoice > 3.0 && redSInstr > 3.0);
    }
    // Turning Frequency must not click: a steady 3 kHz tone in Listen while the cutoff steps through it (100 Hz every 40 ms).
    for (bool high : { true, false })
    {
        DeEsser d; d.prepare(sr); Parameters q; q.listen = true; q.highPass = high; q.freqHz = 800.0; d.setParameters(q);
        std::vector<double> y;
        for (int i = 0; i < (int)(sr * 3.0); ++i)
        {
            if (i % 1920 == 0) { q.freqHz = std::min(5000.0, 800.0 + 100.0 * (i / 1920)); d.setParameters(q); }
            float l = (float)(0.2 * std::sin(2.0 * kPi * 3000.0 * i / sr)), r = l; d.processSample(l, r); y.push_back(l);
        }
        std::vector<double> pk; for (size_t i = 7000; i + 16 < y.size(); i += 16) { double m = 0; for (int k = 0; k < 16; ++k) m = std::max(m, std::abs(y[i + k])); pk.push_back(m); }
        double worstDev = 0; for (size_t i = 2; i + 2 < pk.size(); ++i) { const double nb = 0.5 * (pk[i - 2] + pk[i + 2]); worstDev = std::max(worstDev, std::abs(pk[i] - nb) / nb); }
        assert(worstDev < 0.02);
    }
    // Factory presets: unique names, every value inside its control's range, "Default" first.
    assert(kNumFactoryPresets >= 10 && std::strcmp(kFactoryPresets[0].name, "Default") == 0);
    for (int i = 0; i < kNumFactoryPresets; ++i)
    {
        const auto& f = kFactoryPresets[i];
        assert(f.freqHz >= kMinFreqHz && f.freqHz <= kMaxFreqHz);
        assert(f.thresholdDb >= -40.0f && f.thresholdDb <= 0.0f);
        assert(f.rangeDb >= 0.0f && f.rangeDb <= (float) kMaxRangeDb);
        for (int j = i + 1; j < kNumFactoryPresets; ++j) assert(std::strcmp(f.name, kFactoryPresets[j].name) != 0);
    }
    std::cout << "NF De-Esser DSP tests passed\n";
    return 0;
}
