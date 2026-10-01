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

// The vertical faders (component id "fader"): the thumb travel margin scales with the component so the scale marks drawn by the editor line up.
int NFDeEsserLookAndFeel::getSliderThumbRadius(juce::Slider& slider)
{
    if (slider.getComponentID() == "fader") return juce::roundToInt((float) slider.getWidth() * 0.5f);   // half the cap height (56 base units)
    return juce::LookAndFeel_V4::getSliderThumbRadius(slider);
}

void NFDeEsserLookAndFeel::drawLinearSlider(juce::Graphics& g,int x,int y,int width,int height,float sliderPos,float,float,juce::Slider::SliderStyle style,juce::Slider& slider)
{
    if (slider.getComponentID() != "fader" || style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider(g,x,y,width,height,sliderPos,0.0f,0.0f,style,slider);
        return;
    }

    juce::Graphics::ScopedSaveState state(g);
    const float s = (float)width / 56.0f;   // the component is 56 base units wide
    const float centreX = (float)x + (float)width*0.5f;
    const float trackTop = (float)y + 10.0f*s;
    const float trackBottom = (float)y + (float)height - 10.0f*s;

    // Groove: matte black, rounded, with a soft shadow.
    g.setColour(juce::Colour(0x55000000));
    g.fillRoundedRectangle(centreX-4.5f*s, trackTop+2.0f*s, 9.0f*s, trackBottom-trackTop, 4.5f*s);
    g.setColour(juce::Colour(0xff0d110e));
    g.fillRoundedRectangle(centreX-3.5f*s, trackTop, 7.0f*s, trackBottom-trackTop, 3.5f*s);
    g.setColour(juce::Colour(0xff2a302b));
    g.drawRoundedRectangle(centreX-3.5f*s, trackTop, 7.0f*s, trackBottom-trackTop, 3.5f*s, 0.8f*s);

    // Thumb: silver ribbed fader cap (like the classic console / DAW faders): upper half silver, lower half brighter, a black line at the
    // centre (the value mark) and a dark band along the bottom edge.
    juce::Rectangle<float> thumb(centreX-20.0f*s, sliderPos-28.0f*s, 40.0f*s, 56.0f*s);
    const float midY = thumb.getCentreY();
    g.setColour(juce::Colour(0x80000000));
    g.fillRoundedRectangle(thumb.translated(0.0f,3.5f*s), 3.0f*s);
    {
        juce::Graphics::ScopedSaveState clip(g);
        juce::Path shape; shape.addRoundedRectangle(thumb, 3.0f*s); g.reduceClipRegion(shape);
        // upper half
        auto upper = thumb.withBottom(midY);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe9e9e9), upper.getX(), upper.getY(), juce::Colour(0xffa9a9a9), upper.getX(), upper.getBottom(), false));
        g.fillRect(upper);
        // lower half, brighter, with a darker strip at the very bottom
        auto lower = thumb.withTop(midY);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe2e2e2), lower.getX(), lower.getY(), juce::Colour(0xfffdfdfd), lower.getX(), lower.getY() + lower.getHeight()*0.55f, false));
        g.fillRect(lower);
        // horizontal ribs
        for (int i = 0; i < 9; ++i)
        {
            const float yy = thumb.getY() + 4.0f*s + (float) i * 2.6f*s;
            if (yy < midY - 2.0f*s) { g.setColour(juce::Colour(0x30000000)); g.fillRect(thumb.getX(), yy, thumb.getWidth(), 0.9f*s); g.setColour(juce::Colours::white.withAlpha(0.45f)); g.fillRect(thumb.getX(), yy + 0.9f*s, thumb.getWidth(), 0.8f*s); }
        }
        for (int i = 0; i < 7; ++i)
        {
            const float yy = midY + 4.0f*s + (float) i * 2.8f*s;
            if (yy < thumb.getBottom() - 11.0f*s) { g.setColour(juce::Colour(0x28000000)); g.fillRect(thumb.getX(), yy, thumb.getWidth(), 0.9f*s); g.setColour(juce::Colours::white.withAlpha(0.8f)); g.fillRect(thumb.getX(), yy + 0.9f*s, thumb.getWidth(), 1.0f*s); }
        }
        // dark strip along the bottom
        auto strip = thumb.removeFromBottom(10.0f*s);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff8a8a8a), strip.getX(), strip.getY(), juce::Colour(0xff505050), strip.getX(), strip.getBottom(), false));
        g.fillRect(strip);
        // highlight on the top edge
        g.setColour(juce::Colours::white.withAlpha(0.7f)); g.fillRect(thumb.getX(), thumb.getY(), thumb.getWidth(), 1.6f*s);
    }
    thumb = juce::Rectangle<float>(centreX-20.0f*s, sliderPos-28.0f*s, 40.0f*s, 56.0f*s);
    g.setColour(juce::Colour(0xff0a0a0a));                                     // centre line (the value mark)
    g.fillRect(thumb.getX(), thumb.getCentreY()-1.3f*s, thumb.getWidth(), 2.6f*s);
    g.setColour(juce::Colour(0xff1a1a1a));
    g.drawRoundedRectangle(thumb, 3.0f*s, 1.2f*s);
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
