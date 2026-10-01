#pragma once
// Factory presets for NF De-Esser. JUCE-free so the unit tests can validate the values.
// They are starting points (not tuned by ear): adjust Threshold until the Reduction meter flickers only on the "s" sounds.
namespace nfdeesser
{
struct FactoryPreset
{
    const char* name;
    float freqHz;       // 2000..12000
    float thresholdDb;  // -40..0
    float rangeDb;      // 0..20 (maximum reduction)
};

inline constexpr FactoryPreset kFactoryPresets[] = {
    { "Vocal Male",       5500.0f, -26.0f,  8.0f },
    { "Vocal Female",     7500.0f, -26.0f,  8.0f },
    { "Vocal Rock",       6000.0f, -30.0f, 10.0f },
    { "Vocal Pop",        7000.0f, -28.0f,  9.0f },
    { "Vocal Podcast",    5500.0f, -24.0f,  7.0f },
    { "Vocal Heavy",      6500.0f, -34.0f, 14.0f },
    { "Acoustic Guitar",  8000.0f, -26.0f,  5.0f },
    { "Hi-Hat Tamer",     9000.0f, -28.0f,  8.0f },
    { "Overheads",        9500.0f, -30.0f,  6.0f },
    { "Mix Bus",          7000.0f, -24.0f,  4.0f },
    { "Master Gentle",    8000.0f, -22.0f,  3.0f },
};
inline constexpr int kNumFactoryPresets = (int)(sizeof(kFactoryPresets) / sizeof(kFactoryPresets[0]));
}
