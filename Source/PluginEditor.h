#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NFDeEsserLookAndFeel.h"
#include "UI/ValueCapsule.h"
#include "PresetManager.h"

// Reduction meter: vertical LED ladder that hangs from the top (0 dB) down to 20 dB, scale on its right.
class NFDeEsserGainReductionMeter final:public juce::Component, private juce::Timer
{
public:
    explicit NFDeEsserGainReductionMeter(std::atomic<float>& source):src(source){ setInterceptsMouseClicks(false,false); startTimerHz(30); }
    void paint(juce::Graphics& g) override
    {
        const float s = (float)getWidth() / 80.0f;
        constexpr int numSeg = 10; constexpr float maxDb = 20.0f;
        const float barX = 14.0f*s, barW = 26.0f*s, top = 16.0f*s, bottom = (float)getHeight() - 4.0f*s;
        g.setColour(juce::Colour(0x50000000));
        g.fillRoundedRectangle(barX-3.0f*s, top-3.0f*s+2.0f*s, barW+6.0f*s, bottom-top+6.0f*s, 5.0f*s);
        g.setColour(juce::Colour(0xff0d130f));
        g.fillRoundedRectangle(barX-3.0f*s, top-3.0f*s, barW+6.0f*s, bottom-top+6.0f*s, 5.0f*s);
        const float segH = (bottom-top) / (float)numSeg;
        for (int i=0;i<numSeg;++i)
        {
            const bool lit = shown > ((float)i + 0.05f) * (maxDb/(float)numSeg);
            const bool hot = i >= 6;
            juce::Rectangle<float> seg(barX, top + (float)i*segH + 1.2f*s, barW, segH - 2.4f*s);
            g.setColour(lit ? (hot ? juce::Colour(0xffe8863a) : juce::Colour(0xffffdd7a)) : juce::Colour(0xff222c25));
            g.fillRoundedRectangle(seg, 1.5f*s);
            if (lit){ g.setColour(juce::Colours::white.withAlpha(0.35f)); g.fillRoundedRectangle(seg.reduced(1.5f*s).removeFromTop(seg.getHeight()*0.35f), 1.0f*s); }
        }
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(12.5f*s)));
        struct L{ int seg; const char* t; };
        for (auto l : {L{0,"0"},L{3,"6"},L{6,"12"},L{10,"20"}})
            g.drawText(l.t, juce::Rectangle<float>(barX+barW+6.0f*s, top + (float)l.seg*segH - 8.0f*s, 30.0f*s, 16.0f*s), juce::Justification::centredLeft);
        g.drawText("dB", juce::Rectangle<float>(barX-6.0f*s, 0.0f, barW+12.0f*s, 14.0f*s), juce::Justification::centred);
    }
private:
    void timerCallback() override
    {
        const float target = juce::jlimit(0.0f, 20.0f, src.load());
        // Fast attack, slower fall so short reductions stay readable.
        shown = target > shown ? target : shown + (target - shown) * 0.25f;
        if (std::abs(shown - lastPainted) > 0.05f) { lastPainted = shown; repaint(); }
    }
    std::atomic<float>& src;
    float shown = 0.0f, lastPainted = -1.0f;
};

// Cream push-button that lights amber when on (AUDIO / LISTEN / SPLIT-WIDE in the left column). The text can change with the state.
class NFDeEsserToggleButton final:public juce::ToggleButton
{
public:
    explicit NFDeEsserToggleButton(const juce::String& label):text(label){ setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void setLabel(const juce::String& t){ if (t != text) { text = t; repaint(); } }
    void paintButton(juce::Graphics& g,bool over,bool) override
    {
        const float s = (float)getHeight() / 30.0f;
        auto r = getLocalBounds().toFloat().reduced(1.0f*s);
        const bool on = getToggleState();
        g.setColour(juce::Colour(0x50000000));
        g.fillRoundedRectangle(r.translated(0.0f,2.0f*s), 7.0f*s);
        g.setGradientFill(on ? juce::ColourGradient(juce::Colour(0xffffe9a0), 0.0f, r.getY(), juce::Colour(0xffe8b84a), 0.0f, r.getBottom(), false)
                             : juce::ColourGradient(juce::Colour(over ? 0xfffaf8ef : 0xfff2f0e7), 0.0f, r.getY(), juce::Colour(0xffd9d6c9), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle(r, 7.0f*s);
        g.setColour(juce::Colour(0xff111511));
        g.drawRoundedRectangle(r, 7.0f*s, 1.6f*s);
        g.setColour(juce::Colour(0xff101510));
        g.setFont(juce::Font(juce::FontOptions(13.5f*s,juce::Font::bold)));
        g.drawText(text, r, juce::Justification::centred);
    }
private:
    juce::String text;
};

// Vertical LED level meter filling its whole box: minDb at the bottom, maxDb at the top (input meter beside the Threshold fader, output L / R).
class NFDeEsserLevelMeter final:public juce::Component, private juce::Timer
{
public:
    NFDeEsserLevelMeter(std::atomic<float>& source,float minDb_,float maxDb_,int segments):src(source),minDb(minDb_),maxDb(maxDb_),numSeg(segments)
    { setInterceptsMouseClicks(false,false); startTimerHz(30); shown = minDb_; }
    void paint(juce::Graphics& g) override
    {
        const float s = (float)getWidth() / 24.0f;
        auto b = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0x50000000));
        g.fillRoundedRectangle(b.translated(0.0f,2.0f*s), 5.0f*s);
        g.setColour(juce::Colour(0xff0d130f));
        g.fillRoundedRectangle(b, 5.0f*s);
        const auto inner = b.reduced(3.0f*s);
        const float segH = inner.getHeight() / (float)numSeg;
        for (int i=0;i<numSeg;++i)
        {
            const float lowDb = minDb + (maxDb-minDb) * (float)i / (float)numSeg;
            const bool lit = shown > lowDb + 0.05f;
            const bool hot = i >= numSeg - juce::jmax(2, numSeg/5);
            juce::Rectangle<float> seg(inner.getX(), inner.getBottom() - (float)(i+1)*segH + 1.0f*s, inner.getWidth(), segH - 2.0f*s);
            g.setColour(lit ? (hot ? juce::Colour(0xffe8863a) : juce::Colour(0xffffdd7a)) : juce::Colour(0xff222c25));
            g.fillRoundedRectangle(seg, 1.4f*s);
            if (lit){ g.setColour(juce::Colours::white.withAlpha(0.3f)); g.fillRoundedRectangle(seg.reduced(1.2f*s).removeFromTop(seg.getHeight()*0.35f), 1.0f*s); }
        }
    }
private:
    void timerCallback() override
    {
        const float target = juce::jlimit(minDb, maxDb, src.load());
        shown = target > shown ? target : shown + (target - shown) * 0.2f;   // fast attack, slower fall
        if (std::abs(shown - lastPainted) > 0.2f) { lastPainted = shown; repaint(); }
    }
    std::atomic<float>& src;
    float minDb, maxDb, shown, lastPainted = 1000.0f;
    int numSeg;
};

class NFDeEsserPowerButton final:public juce::ToggleButton
{
public:void paintButton(juce::Graphics&,bool,bool) override;
};

// Invisible hit-target over the NF logo: double-click returns the window to its original size.
class NFDeEsserLogoButton final:public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onDoubleClick;
    NFDeEsserLogoButton(){ setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
};

// Three-line hamburger icon opening the presets menu (same as NF Q3).
class NFDeEsserMenuButton final:public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    NFDeEsserMenuButton(){ setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float lineH = juce::jmax(1.6f, b.getHeight()*0.10f);
        g.setColour(juce::Colour(0xffeef2ee));
        for (int i=0;i<3;++i)
        {
            const float y = b.getY() + b.getHeight()*(0.20f + (float)i*0.30f);
            g.fillRoundedRectangle(b.getX(), y, b.getWidth(), lineH, lineH*0.5f);
        }
    }
    void mouseUp(const juce::MouseEvent& e) override { if (contains(e.getPosition()) && onClick) onClick(); }
};

// Small horizontal preset tab: [<]  preset name  [>]. Arrows step through the factory presets, a click on the name opens the list.
class NFDeEsserPresetBar final:public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onPrev, onNext, onMenu;
    NFDeEsserPresetBar(){ setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void setName(const juce::String& n){ if (n != name) { name = n; repaint(); } }
    void paint(juce::Graphics& g) override
    {
        const float s = (float)getHeight() / 29.0f;
        auto r = getLocalBounds().toFloat().reduced(1.0f*s);
        g.setColour(juce::Colour(0x50000000));
        g.fillRoundedRectangle(r.translated(0.0f,2.0f*s), 8.0f*s);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff6f4ec), 0.0f, r.getY(), juce::Colour(0xffdcd9cc), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle(r, 8.0f*s);
        g.setColour(juce::Colour(0xff111511));
        g.drawRoundedRectangle(r, 8.0f*s, 1.6f*s);
        // arrows
        g.setColour(juce::Colour(0xff101510));
        const float cy = r.getCentreY(), ax = 13.0f*s, w = 5.0f*s, h = 6.0f*s;
        juce::Path left, right;
        left.addTriangle(r.getX()+ax+w, cy-h, r.getX()+ax+w, cy+h, r.getX()+ax-w, cy);
        right.addTriangle(r.getRight()-ax-w, cy-h, r.getRight()-ax-w, cy+h, r.getRight()-ax+w, cy);
        g.fillPath(left); g.fillPath(right);
        g.setFont(juce::Font(juce::FontOptions(15.0f*s, juce::Font::bold)));
        g.drawFittedText(name, juce::Rectangle<float>(r.getX()+28.0f*s, r.getY(), r.getWidth()-56.0f*s, r.getHeight()).toNearestInt(), juce::Justification::centred, 1);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!contains(e.getPosition())) return;
        const float s = (float)getHeight() / 29.0f;
        if (e.position.x < 28.0f*s) { if (onPrev) onPrev(); }
        else if (e.position.x > (float)getWidth() - 28.0f*s) { if (onNext) onNext(); }
        else if (onMenu) onMenu();
    }
private:
    juce::String name { "Default" };
};

// Temporary floating value readout over the Threshold fader while it is being moved.
class NFDeEsserGainBubble final:public juce::Component, private juce::Timer
{
public:
    NFDeEsserGainBubble(){ setInterceptsMouseClicks(false,false); setAlpha(0.0f); }
    void showValue(double dB)
    {
        if (std::abs(dB) < 0.05) dB = 0.0;
        juce::String s = juce::String(dB, 1);
        if (dB > 0.0) s = "+" + s;
        showRaw(s + " dB");
    }
    void showRaw(const juce::String& t)
    {
        text = t;
        juce::Desktop::getInstance().getAnimator().cancelAnimation(this, false);
        setAlpha(1.0f);
        setVisible(true);
        repaint();
        startTimer(800);
    }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(0xff0a140d).withAlpha(0.85f));
        g.fillRoundedRectangle(b, 6.0f);
        g.setColour(juce::Colour(0xffd9d4c4).withAlpha(0.6f));
        g.drawRoundedRectangle(b, 6.0f, 1.0f);
        g.setColour(juce::Colour(0xfff2efe4));
        g.setFont(juce::Font(juce::FontOptions(juce::jmax(11.0f, b.getHeight()*0.5f), juce::Font::bold)));
        g.drawText(text, b, juce::Justification::centred);
    }
private:
    void timerCallback() override
    {
        stopTimer();
        juce::Desktop::getInstance().getAnimator().fadeOut(this, 220);
    }
    juce::String text;
};

class NFDeEsserAudioProcessorEditor final:public juce::AudioProcessorEditor, private juce::ValueTree::Listener, private juce::AsyncUpdater
{
public:
    explicit NFDeEsserAudioProcessorEditor(NFDeEsserAudioProcessor&);
    ~NFDeEsserAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;void resized() override;
private:
    struct FaderTick { double value; juce::String label; bool major; };
    void updateMonitorButtons();
    void drawKnobScale(juce::Graphics&,juce::Point<float> centre,float radius,const juce::String& lo,const juce::String& hi);
    void drawFaderScale(juce::Graphics&,juce::Slider&,float centreX,const std::vector<FaderTick>&);
    juce::Rectangle<int> scaleBounds(juce::Rectangle<float> baseBounds) const;
    // the preset name lives in the plug-in state; refresh the tab whenever it changes (may come from a non-message thread)
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& id) override { if (id.toString() == "presetName") triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { presetBar.setName(nfdeesser::PresetManager::getCurrentPresetName(processor.apvts)); }
    void showPresetMenu();
    void stepPreset(int direction);
    void showMainMenu();
    void handleSavePreset();
    void handleLoadPreset();
    void showAbout();

    float layoutScale=1.0f, offsetX=0.0f, offsetY=0.0f;
    NFDeEsserAudioProcessor& processor;NFDeEsserLookAndFeel look;
    juce::TooltipWindow tooltipWindow{this, 500};
    NFDeEsserMenuButton menuButton;
    NFDeEsserPresetBar presetBar;
    NFDeEsserLogoButton logoButton;
    std::unique_ptr<juce::FileChooser> presetFileChooser;
    juce::Slider thresholdKnob;   // the only fader: Threshold
    juce::Slider freqKnob,rangeKnob;   // round knobs in the left column
    NFDeEsserGainBubble thresholdBubble;
    NFDeEsserGainReductionMeter grMeter;   // ATTEN
    NFDeEsserLevelMeter inputMeter, outLMeter, outRMeter;
    ValueCapsule freqCap,thresholdCap,rangeCap;
    NFDeEsserPowerButton power;
    NFDeEsserToggleButton modeBtn{"SPLIT"}, audioBtn{"AUDIO"}, listenBtn{"LISTEN"};
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> thresholdA,freqA,rangeA,freqCapA,thresholdCapA,rangeCapA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerA,listenA,wideA;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFDeEsserAudioProcessorEditor)
};
