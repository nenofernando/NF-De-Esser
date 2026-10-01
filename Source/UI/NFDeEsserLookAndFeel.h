#pragma once
#include <JuceHeader.h>

class NFDeEsserLookAndFeel final:public juce::LookAndFeel_V4
{
public:
    NFDeEsserLookAndFeel();
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider::SliderStyle,juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&,juce::Button&,const juce::Colour&,bool,bool) override;

    static void drawScrew(juce::Graphics&,juce::Rectangle<float> bounds);
    static void drawPowerBody(juce::Graphics&,juce::Rectangle<float> bounds);
    static void drawLed(juce::Graphics&,juce::Rectangle<float> bounds,bool on);
};
