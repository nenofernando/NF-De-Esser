#pragma once
// The plug-in's starting values: ONE place, shared by the parameters, the editor (double-click) and the "Default" factory preset.
// FactoryPresets.h is checked against these at compile time, so the two can never drift apart again.
#include "FactoryPresets.h"
namespace nfdeesser
{
inline constexpr float kDefaultFreqHz = 5500.0f;
inline constexpr float kDefaultThresholdDb = -20.0f;
inline constexpr float kDefaultRangeDb = 12.0f;
inline constexpr bool  kDefaultHighPass = true;    // HIGH
inline constexpr bool  kDefaultVoice = true;       // VOICE

constexpr bool sameValue(float a, float b) { return !(a < b) && !(b < a); }   // exact equality without -Wfloat-equal noise
static_assert(sameValue(kFactoryPresets[0].freqHz, kDefaultFreqHz), "preset Default: Frequency differs from the plug-in's starting value");
static_assert(sameValue(kFactoryPresets[0].thresholdDb, kDefaultThresholdDb), "preset Default: Threshold differs from the plug-in's starting value");
static_assert(sameValue(kFactoryPresets[0].rangeDb, kDefaultRangeDb), "preset Default: Range differs from the plug-in's starting value");
static_assert(kFactoryPresets[0].highPass == kDefaultHighPass, "preset Default: side-chain filter differs from the plug-in's starting value");
}
