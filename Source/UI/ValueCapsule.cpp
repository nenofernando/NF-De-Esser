#include "ValueCapsule.h"
#include "NFDeEsserBinaryData.h"

namespace
{
juce::Image readyAsset(const char* data, int size) { return juce::ImageCache::getFromMemory(data, size); }
constexpr float kBaseCapsuleHeight = 32.0f;
constexpr float kBaseCapsuleWidth = 130.0f;
constexpr float kBaseTextWidth = 108.0f;
}

ValueCapsule::ValueCapsule(const juce::String& text,double def,bool showArrows,std::function<juce::String(double)> f)
    :caption(text),defaultValue(def),dragStartValue(def),arrows(showArrows),format(std::move(f))
{
    addAndMakeVisible(slider);slider.setSliderStyle(juce::Slider::LinearBar);slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    slider.setAlpha(0.0f);slider.setInterceptsMouseClicks(false,false);slider.onValueChange=[this]{repaint();};
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}
void ValueCapsule::resized()
{
    const float s = getWidth() / kBaseCapsuleWidth;
    slider.setBounds(getLocalBounds().removeFromTop(juce::roundToInt(kBaseCapsuleHeight*s)));
}

void ValueCapsule::nudge(int direction)
{
    const double interval = slider.getInterval();
    if (arrows && interval > 0.0)
        slider.setValue(slider.getValue() + direction*interval, juce::sendNotificationSync);
    else
        slider.setValue(slider.proportionOfLengthToValue(juce::jlimit(0.0,1.0,slider.valueToProportionOfLength(slider.getValue())+direction*0.02)), juce::sendNotificationSync);
}

void ValueCapsule::paint(juce::Graphics& g)
{
    static const juce::Image capsule = readyAsset(NFDeEsserBinaryData::_06_frequency_capsule_130x32_png, NFDeEsserBinaryData::_06_frequency_capsule_130x32_pngSize);
    static const juce::Image arrowUp = readyAsset(NFDeEsserBinaryData::_07_arrow_up_10x7_png, NFDeEsserBinaryData::_07_arrow_up_10x7_pngSize);
    static const juce::Image arrowDown = readyAsset(NFDeEsserBinaryData::_08_arrow_down_10x7_png, NFDeEsserBinaryData::_08_arrow_down_10x7_pngSize);

    const float s = getWidth() / kBaseCapsuleWidth;
    const float capsuleH = kBaseCapsuleHeight*s, capsuleW = kBaseCapsuleWidth*s, textW = arrows ? kBaseTextWidth*s : capsuleW;
    {
        juce::Graphics::ScopedSaveState state(g);
        g.setOpacity(1.0f);
        if (capsule.isValid())
            g.drawImage(capsule, juce::Rectangle<float>(0,0,capsuleW,capsuleH), juce::RectanglePlacement::centred);
    }
    g.setColour(juce::Colour(0xff101510));
    g.setFont(juce::Font(juce::FontOptions(16.0f*s,juce::Font::bold)));
    g.drawText(format(slider.getValue()), juce::Rectangle<float>(0,0,textW,capsuleH), juce::Justification::centred);
    if (arrows)
    {
        juce::Graphics::ScopedSaveState state(g);
        g.setOpacity(1.0f);
        if (arrowUp.isValid())   g.drawImage(arrowUp,   juce::Rectangle<float>(111*s,5*s,10*s,7*s),  juce::RectanglePlacement::centred);
        if (arrowDown.isValid()) g.drawImage(arrowDown, juce::Rectangle<float>(111*s,20*s,10*s,7*s), juce::RectanglePlacement::centred);
    }
    g.setColour(juce::Colour(0xffffffff));
    g.setFont(15.0f*s);g.drawText(caption,juce::Rectangle<float>(0,33*s,capsuleW,18*s),juce::Justification::centred);
}
void ValueCapsule::mouseDown(const juce::MouseEvent& e)
{
    dragStartValue=slider.getValue();
    const float s = getWidth() / kBaseCapsuleWidth;
    if (arrows && e.position.x > kBaseTextWidth*s && e.position.y < kBaseCapsuleHeight*s)
        nudge(e.position.y < (kBaseCapsuleHeight*s)*0.5f ? 1 : -1);
}
void ValueCapsule::mouseDrag(const juce::MouseEvent& e)
{
    const float s = getWidth() / kBaseCapsuleWidth;
    double p=slider.valueToProportionOfLength(dragStartValue)-e.getDistanceFromDragStartY()/(170.0*s);
    p=juce::jlimit(0.0,1.0,p);slider.setValue(slider.proportionOfLengthToValue(p),juce::sendNotificationSync);
}
void ValueCapsule::mouseDoubleClick(const juce::MouseEvent&){slider.setValue(defaultValue,juce::sendNotificationSync);}
void ValueCapsule::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    nudge(wheel.deltaY > 0.0f ? 1 : -1);
}
