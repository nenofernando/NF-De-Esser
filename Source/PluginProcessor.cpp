#include "PluginProcessor.h"
#include "PluginEditor.h"

NFDeEsserAudioProcessor::NFDeEsserAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                     .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      apvts(*this,nullptr,"NF_DEESSER_STATE",createParameters())
{
    freqParam = apvts.getRawParameterValue("freq");
    thresholdParam = apvts.getRawParameterValue("threshold");
    rangeParam = apvts.getRawParameterValue("range");
    listenParam = apvts.getRawParameterValue("listen");
    fullParam = apvts.getRawParameterValue("full");
    powerParam = apvts.getRawParameterValue("power");
}

void NFDeEsserAudioProcessor::prepareToPlay(double sr,int)
{
    deEsser.prepare(sr);
    wasPowered = true;
}

bool NFDeEsserAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();
    return (in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo())&&in==out;
}

// Peak of each output channel in dB (the editor's meters smooth it).
void NFDeEsserAudioProcessor::measureOutput(const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < 2; ++ch)
    {
        const int src = std::min(ch, buffer.getNumChannels() - 1);
        const float peak = src >= 0 ? buffer.getMagnitude(src, 0, buffer.getNumSamples()) : 0.0f;
        outputLevelDb[ch].store(juce::Decibels::gainToDecibels(peak, -100.0f));
    }
}

void NFDeEsserAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals guard;

    // Power off = the untouched input (no latency, so nothing to compensate).
    const bool powered = powerParam->load() > 0.5f;
    if (!powered) { if (wasPowered) deEsser.reset(); wasPowered = false; gainReductionDb.store(0.0f); detectorLevelDb.store(-100.0f); measureOutput(buffer); return; }
    wasPowered = true;

    nfdeesser::Parameters p;
    p.freqHz = freqParam->load();
    p.thresholdDb = thresholdParam->load();
    p.rangeDb = rangeParam->load();
    p.listen = listenParam->load() > 0.5f;
    p.wide = fullParam->load() > 0.5f;   // FULL = whole signal (the DSP calls it "wide")
    deEsser.setParameters(p);

    const int numCh = std::min(2, buffer.getNumChannels());
    if (numCh <= 0) return;
    auto* l = buffer.getWritePointer(0);
    auto* r = numCh > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        float left = l[n], right = r != nullptr ? r[n] : l[n];
        deEsser.processSample(left, right);
        l[n] = left;
        if (r != nullptr) r[n] = right;
    }
    gainReductionDb.store((float) deEsser.gainReductionDb());
    detectorLevelDb.store((float) deEsser.detectorLevelDb());
    measureOutput(buffer);
}

juce::AudioProcessorValueTreeState::ParameterLayout NFDeEsserAudioProcessor::createParameters()
{
    using ID=juce::ParameterID;std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    // Frequency 2-12 kHz (centre of the knob = 5 kHz), Threshold -40..0 dB (centre -20), Range 0..20 dB.
    juce::NormalisableRange<float> freqRange((float) nfdeesser::kMinFreqHz,(float) nfdeesser::kMaxFreqHz,100.0f); freqRange.setSkewForCentre(5000.0f);
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"freq",1},"Frequency",freqRange,6500.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"threshold",1},"Threshold",juce::NormalisableRange<float>(-40.0f,0.0f,1.0f),-20.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"range",1},"Range",juce::NormalisableRange<float>(0.0f,(float) nfdeesser::kMaxRangeDb,0.5f),12.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"listen",1},"Listen",false));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"full",1},"Full band",false));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"power",1},"Power",true));
    return {p.begin(),p.end()};
}

void NFDeEsserAudioProcessor::getStateInformation(juce::MemoryBlock& dest){if(auto xml=apvts.copyState().createXml())copyXmlToBinary(*xml,dest);}
void NFDeEsserAudioProcessor::setStateInformation(const void* data,int size){if(auto xml=getXmlFromBinary(data,size);xml&&xml->hasTagName(apvts.state.getType()))apvts.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* NFDeEsserAudioProcessor::createEditor(){return new NFDeEsserAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new NFDeEsserAudioProcessor();}
