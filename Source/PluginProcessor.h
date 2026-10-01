#pragma once
#include <JuceHeader.h>
#include "DSP/DeEsser.h"

class NFDeEsserAudioProcessor final : public juce::AudioProcessor
{
public:
    NFDeEsserAudioProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    juce::AudioProcessorValueTreeState apvts;
    // Current gain reduction in dB (>= 0), read by the editor's meter.
    std::atomic<float> gainReductionDb { 0.0f };
    // Levels in dB for the meters: what the detector hears (input meter beside the Threshold fader) and the output peaks (L / R).
    std::atomic<float> detectorLevelDb { -100.0f }, outputLevelDb[2] { { -100.0f }, { -100.0f } };

private:
    void measureOutput(const juce::AudioBuffer<float>&);
    nfdeesser::DeEsser deEsser;
    std::atomic<float> *freqParam = nullptr, *thresholdParam = nullptr, *rangeParam = nullptr,
                       *listenParam = nullptr, *fullParam = nullptr, *powerParam = nullptr;
    bool wasPowered = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFDeEsserAudioProcessor)
};
