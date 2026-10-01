#pragma once
#include <JuceHeader.h>

namespace nfdeesser
{
// Presets are just the existing APVTS state (same XML as DAW session recall) plus two
// validation attributes. Same scheme as NF Q3.
struct PresetManager
{
    static juce::File getPresetsDirectory();
    // Name of the preset in use (kept inside the plug-in state, so it is saved with the project). "Default" until one is chosen.
    static juce::String getCurrentPresetName(juce::AudioProcessorValueTreeState& apvts);
    static juce::Result savePreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
    static void applyFactoryPreset(juce::AudioProcessorValueTreeState& apvts, int index);
    static juce::Result loadPreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
};
}
