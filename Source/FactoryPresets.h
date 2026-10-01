#pragma once
// Factory presets for NF De-Esser. JUCE-free so the unit tests can validate the values.
// They are starting points (not tuned by ear): adjust Threshold until the Reduction meter flickers only on the "s" sounds.
// Thresholds are on the calibrated detector scale (an "s" reads around -10..-5 dB, the vowels around -25..-20 dB).
// Frequencies follow the usual ranges of the voices (male "s" about 4.5 kHz, "sh" about 3.4 kHz; female "s" about 6.8 kHz, "sh" about 5 kHz).
namespace nfdeesser
{
struct FactoryPreset
{
    const char* name;
    float freqHz;       // 500..16000
    float thresholdDb;  // -40..0
    float rangeDb;      // 0..20 (maximum reduction)
    bool  highPass;     // side-chain filter: true = high-pass (all kinds of "s"), false = band-pass (one kind of "s")
};

inline constexpr FactoryPreset kFactoryPresets[] = {
    { "Default",          5500.0f, -20.0f, 12.0f, true  },   // the plug-in's starting point (also what a fresh instance loads)
    { "Vocal Male",       4500.0f, -18.0f, 10.0f, false },
    { "Vocal Male Sh",    3400.0f, -18.0f,  8.0f, false },
    { "Vocal Female",     6800.0f, -18.0f, 10.0f, false },
    { "Vocal Female Sh",  5100.0f, -18.0f,  8.0f, false },
    { "Vocal Wide",       5000.0f, -20.0f, 10.0f, true  },   // many different "s" sounds in one voice
    { "Vocal Rock",       5500.0f, -20.0f, 12.0f, true  },
    { "Vocal Podcast",    4500.0f, -16.0f,  8.0f, true  },
    { "Acoustic Guitar",  7500.0f, -16.0f,  6.0f, true  },
    { "Hi-Hat Tamer",     9000.0f, -16.0f,  8.0f, true  },
    { "Overheads",        8500.0f, -18.0f,  6.0f, true  },
    { "Mix Bus",          6500.0f, -12.0f,  4.0f, true  },
    { "Master Gentle",    7500.0f, -10.0f,  3.0f, true  },
};
inline constexpr int kNumFactoryPresets = (int)(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]));
}
