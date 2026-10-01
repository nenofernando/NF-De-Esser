#include "NFDeEsserLookAndFeel.h"
#include "NFDeEsserBinaryData.h"

namespace
{
juce::Image readyAsset(const char* data, int size)
{
    return juce::ImageCache::getFromMemory(data, size);
}
}

NFDeEsserLookAndFeel::NFDeEsserLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xff101510));
    setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xfff0eee5));
    setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff101510));
}

void NFDeEsserLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider&)
{
    // The slider's own bounds are set (in PluginEditor::resized) to the exact READY-asset
    // rect (e.g. 198,108,145,145 for LOW), so both PNGs are drawn 1:1, no extra scaling.
    juce::Rectangle<float> r((float)x,(float)y,(float)w,(float)h);
    const auto c = r.getCentre();

    static const juce::Image knobBody = readyAsset(NFDeEsserBinaryData::_02_knob_body_145x145_png, NFDeEsserBinaryData::_02_knob_body_145x145_pngSize);
    {
        juce::Graphics::ScopedSaveState state(g);
        g.setOpacity(1.0f);
        if (knobBody.isValid())
            g.drawImage(knobBody, r, juce::RectanglePlacement::centred);
    }

    // Indicator: drawn as a JUCE vector stroke (not the PNG, which would look too thick if
    // scaled up with the bigger knob body), pivoting only about the knob's exact centre.
    // At 0 dB, `pos` == 0.5 (midpoint of the -12..+12 range) so angle == (start+end)/2 and
    // the un-rotated stroke (already vertical, pointing straight up) needs no rotation.
    {
        // The knob's own component bounds already carry the current uiScale (they were
        // set via PluginEditor::scaleBounds from the 190x190 base box), so deriving a
        // local factor from the actual width keeps the stroke proportionally correct
        // at every window size without a second, separate scale calculation.
        const float localScale = (float) w / 190.0f;
        juce::Graphics::ScopedSaveState state(g);
        const float angle = start + pos * (end - start);
        g.addTransform(juce::AffineTransform::rotation(angle, c.x, c.y));
        juce::Path indicator;
        indicator.startNewSubPath(c.x, c.y);
        indicator.lineTo(c.x, c.y - 56.0f*localScale);
        g.setColour(juce::Colour(0xff0a0a0a));
        g.strokePath(indicator, juce::PathStrokeType(8.0f*localScale, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }
}

void NFDeEsserLookAndFeel::drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float sliderPos,float,float,juce::Slider::SliderStyle style,juce::Slider& slider)
{
    if (slider.getComponentID() != "outputSlider" || style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,sliderPos,0.0f,0.0f,style,slider);
        return;
    }

    juce::Graphics::ScopedSaveState state(g);

    // This component's own bounds already carry the current uiScale (set via
    // PluginEditor::scaleBounds from the 50x110 base box), so a local factor derived
    // from the actual width keeps every hand-drawn dimension below proportionally
    // correct at any window size, exactly like the knob indicator does.
    const float s = (float)width / 50.0f;

    const float centreX = (float)x + (float)width*0.5f;
    const float trackTop = (float)y + 5.0f*s;
    const float trackBottom = (float)y + (float)height - 5.0f*s;

    // Track: matte black, rounded, with a subtle inner shadow -- no colour, no numbers.
    g.setColour(juce::Colour(0x50000000));
    g.fillRoundedRectangle(centreX-3.0f*s, trackTop+2.0f*s, 6.0f*s, trackBottom-trackTop, 3.0f*s);
    g.setColour(juce::Colour(0xff101311));
    g.fillRoundedRectangle(centreX-2.0f*s, trackTop, 4.0f*s, trackBottom-trackTop, 2.0f*s);

    // Thumb: horizontal, black-metal, thin silver outline, discreet relief.
    juce::Rectangle<float> thumb(centreX-12.0f*s, sliderPos-4.0f*s, 24.0f*s, 8.0f*s);
    g.setColour(juce::Colour(0x50000000));
    g.fillRoundedRectangle(thumb.translated(0.0f,2.0f*s), 2.0f*s);
    juce::ColourGradient thumbGradient(juce::Colour(0xff343735), thumb.getCentreX(), thumb.getY(),
                                         juce::Colour(0xff0b0d0c), thumb.getCentreX(), thumb.getBottom(), false);
    g.setGradientFill(thumbGradient);
    g.fillRoundedRectangle(thumb, 2.0f*s);

    // Soft whitish sheen across the upper portion, like a brushed-metal highlight.
    auto sheen = thumb.reduced(1.2f*s).removeFromTop(thumb.getHeight()*0.4f);
    g.setColour(juce::Colours::white.withAlpha(0.28f));
    g.fillRoundedRectangle(sheen, 1.4f*s);

    g.setColour(juce::Colour(0xff777b78));
    g.drawRoundedRectangle(thumb, 2.0f*s, 0.8f*s);
}

void NFDeEsserLookAndFeel::drawScrew(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    static const juce::Image screw = readyAsset(NFDeEsserBinaryData::_09_screw_24x24_png, NFDeEsserBinaryData::_09_screw_24x24_pngSize);
    juce::Graphics::ScopedSaveState state(g);
    g.setOpacity(1.0f);
    if (screw.isValid())
        g.drawImage(screw, bounds, juce::RectanglePlacement::centred);
}

void NFDeEsserLookAndFeel::drawPowerBody(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    static const juce::Image btn = readyAsset(NFDeEsserBinaryData::_04_power_56x56_png, NFDeEsserBinaryData::_04_power_56x56_pngSize);
    juce::Graphics::ScopedSaveState state(g);
    g.setOpacity(1.0f);
    if (btn.isValid())
        g.drawImage(btn, bounds, juce::RectanglePlacement::centred);
}

void NFDeEsserLookAndFeel::drawLed(juce::Graphics& g, juce::Rectangle<float> bounds, bool on)
{
    // `bounds` is the halo bounding box (~20x20), centred exactly above Power on the same
    // X axis. Drawn entirely as vector shapes -- no PNG margin to fight with -- so the
    // visible core stays a crisp 10px regardless of the asset's own transparent padding.
    const auto c = bounds.getCentre();
    const float haloR = bounds.getWidth()*0.5f;   // ~10px -> 20px halo diameter
    const float coreR = haloR*0.5f;                // ~5px  -> 10px core diameter

    juce::Graphics::ScopedSaveState state(g);
    g.setOpacity(1.0f);

    // 2. Thin bronze/gold lens ring.
    g.setColour(on ? juce::Colour(0xffb8863a) : juce::Colour(0xff54493c));
    g.fillEllipse(juce::Rectangle<float>(c.x-coreR-1.3f, c.y-coreR-1.3f, (coreR+1.3f)*2.0f, (coreR+1.3f)*2.0f));

    // 3. Luminous core last: warm amber when on, dark brown/grey when off.
    g.setColour(on ? juce::Colour(0xffffdd7a) : juce::Colour(0xff473e35));
    g.fillEllipse(juce::Rectangle<float>(c.x-coreR, c.y-coreR, coreR*2.0f, coreR*2.0f));

    // Small cream-white highlight, top-left, only visible when lit.
    if (on)
    {
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillEllipse(juce::Rectangle<float>(c.x-coreR*0.62f, c.y-coreR*0.68f, coreR*0.55f, coreR*0.55f));
    }
}

juce::Label* NFDeEsserLookAndFeel::createSliderTextBox(juce::Slider& s)
{
    auto* l=LookAndFeel_V4::createSliderTextBox(s);l->setFont(juce::Font(juce::FontOptions(17.0f,juce::Font::bold)));l->setJustificationType(juce::Justification::centred);return l;
}

void NFDeEsserLookAndFeel::drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down)
{
    auto r=b.getLocalBounds().toFloat().reduced(1);g.setColour(juce::Colour(down?0xffb9b8b1:(over?0xfffaf8ef:0xffe8e6dd)));g.fillRoundedRectangle(r,4);g.setColour(juce::Colour(0xff111511));g.drawRoundedRectangle(r,4,1.2f);
}
