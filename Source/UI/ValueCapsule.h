#pragma once
#include <JuceHeader.h>

// Same cream capsule as NF Q3's frequency control, but for a plain value: shows a formatted
// number, optional up/down arrows (stepped knobs), and the range caption underneath.
class ValueCapsule final:public juce::Component
{
public:
    ValueCapsule(const juce::String& rangeText,double defaultValue,bool showArrows,std::function<juce::String(double)> formatter);
    void resized() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::Slider slider;
private:
    void nudge(int direction);
    juce::String caption;
    double defaultValue=0.0,dragStartValue=0.0;
    bool arrows=false;
    std::function<juce::String(double)> format;
};
