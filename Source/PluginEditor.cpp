#include "PluginEditor.h"
#include "NFDeEsserBinaryData.h"
#include "FactoryPresets.h"
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "0.0.0-test"
#endif

namespace
{
juce::Image readyAsset(const char* data, int size) { return juce::ImageCache::getFromMemory(data, size); }

// Base layout: square, 800 x 800. Left column (mode / frequency / range / monitor), then the Threshold fader (the only fader) with its input meter,
// the ATTEN meter, and the L / R output level meters (no output control).
constexpr int kDefaultSize = 540;   // 800 x 800 base shown at 0.675, the same text scale as the 810 x 270 plug-ins
constexpr float kThresholdX = 330.0f, kAttenX = 480.0f, kOutLX = 656.0f, kOutRX = 686.0f;
constexpr float kFaderW = 56.0f, kFaderY = 112.0f, kFaderH = 500.0f;   // vertical fader box (base units)
constexpr float kThumbMargin = 28.0f;                                    // fader thumb travel margin (half the cap height, 0.5 * width): the meters span the same range
constexpr float kLeftX = 137.0f;                                         // centre of the left column
constexpr float kKnobBox = 96.0f, kFreqKnobY = 302.0f, kRangeKnobY = 502.0f;   // round knobs (base units)

juce::String formatDb(double v){ return juce::String(juce::roundToInt(v))+" dB"; }
juce::String formatRange(double v){ return juce::String(v,1)+" dB"; }
juce::String formatFreq(double v){ return v >= 1000.0 ? juce::String(v/1000.0, 1)+" kHz" : juce::String(juce::roundToInt(v))+" Hz"; }
}

void NFDeEsserPowerButton::paintButton(juce::Graphics& g,bool,bool)
{
    NFDeEsserLookAndFeel::drawPowerBody(g, getLocalBounds().toFloat());
}

NFDeEsserAudioProcessorEditor::NFDeEsserAudioProcessorEditor(NFDeEsserAudioProcessor& p)
    :AudioProcessorEditor(&p),processor(p),grMeter(p.gainReductionDb),
     inputMeter(p.detectorLevelDb,-40.0f,0.0f,24),outLMeter(p.outputLevelDb[0],-30.0f,0.0f,20),outRMeter(p.outputLevelDb[1],-30.0f,0.0f,20),
     freqCap("2-12 kHz",6500.0,false,formatFreq),thresholdCap("-40 to 0 dB",-20.0,true,formatDb),
     rangeCap("0 to 20 dB",8.0,false,formatRange)
{
    setLookAndFeel(&look);
    setResizable(true,true);
    getConstrainer()->setFixedAspectRatio(1.0);   // square plug-in
    getConstrainer()->setSizeLimits(405,405,1200,1200);
    {
        const int w = juce::jlimit(405, 1200, (int) p.apvts.state.getProperty("uiWidth", kDefaultSize));   // size chosen with the resize handle survives close / reopen
        setSize(w, w);
    }

    addAndMakeVisible(logoButton);
    logoButton.setTooltip("Double-click: reset UI size");
    logoButton.onDoubleClick = [this]{ setSize(kDefaultSize,kDefaultSize); };

    addAndMakeVisible(menuButton);
    menuButton.setTooltip("About");
    menuButton.onClick = [this]{ showMainMenu(); };
    addAndMakeVisible(presetBar);
    presetBar.setTooltip("Preset: click the name for the list, arrows = previous / next");
    presetBar.onPrev = [this]{ stepPreset(-1); };
    presetBar.onNext = [this]{ stepPreset(+1); };
    presetBar.onMenu = [this]{ showPresetMenu(); };
    presetBar.setName(nfdeesser::PresetManager::getCurrentPresetName(processor.apvts));
    processor.apvts.state.addListener(this);

    struct K{ juce::Slider* s; double def; };
    for(auto k:{K{&thresholdKnob,-20.0}}){
        addAndMakeVisible(*k.s);
        k.s->setComponentID("fader");
        k.s->setSliderStyle(juce::Slider::LinearVertical);
        k.s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        k.s->setSliderSnapsToMousePosition(true);
        k.s->setScrollWheelEnabled(true);
        k.s->setDoubleClickReturnValue(true,k.def);
    }
    for(auto k:{K{&freqKnob,6500.0},K{&rangeKnob,8.0}}){
        addAndMakeVisible(*k.s);
        k.s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.s->setRotaryParameters(juce::MathConstants<float>::pi*1.25f, juce::MathConstants<float>::pi*2.75f, true);   // 270-degree sweep, same as the family
        k.s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        k.s->setDoubleClickReturnValue(true,k.def);
    }
    for(auto* c:{&freqCap,&thresholdCap,&rangeCap}) addAndMakeVisible(*c);
    addAndMakeVisible(power);power.setClickingTogglesState(true);
    for(auto* b:{&modeBtn,&listenBtn}) { addAndMakeVisible(*b); b->setClickingTogglesState(true); }
    addAndMakeVisible(audioBtn);
    modeBtn.setTooltip("SPLIT: only the sibilance band is turned down. WIDE: the whole signal is turned down (same detector)");
    audioBtn.setTooltip("Monitor the normal audio");
    listenBtn.setTooltip("Monitor the band the de-esser hears (side-chain), to find the sibilance");
    audioBtn.onClick = [this]{ if (auto* q = processor.apvts.getParameter("listen")) { q->beginChangeGesture(); q->setValueNotifyingHost(0.0f); q->endChangeGesture(); } };
    addAndMakeVisible(thresholdBubble);
    addAndMakeVisible(grMeter);
    for(auto* m:{&inputMeter,&outLMeter,&outRMeter}) addAndMakeVisible(*m);

    // Floating value readouts, only while the user is interacting (never on host automation).
    thresholdKnob.onValueChange = [this]{ if (thresholdKnob.isMouseOverOrDragging()) thresholdBubble.showRaw(formatDb(thresholdKnob.getValue())); };

    auto& a=processor.apvts;
    thresholdA=std::make_unique<SA>(a,"threshold",thresholdKnob);freqA=std::make_unique<SA>(a,"freq",freqKnob);rangeA=std::make_unique<SA>(a,"range",rangeKnob);
    freqCapA=std::make_unique<SA>(a,"freq",freqCap.slider);thresholdCapA=std::make_unique<SA>(a,"threshold",thresholdCap.slider);rangeCapA=std::make_unique<SA>(a,"range",rangeCap.slider);
    powerA=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a,"power",power);
    listenA=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a,"listen",listenBtn);
    wideA=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a,"wide",modeBtn);
    power.onStateChange=[this]{repaint();};
    listenBtn.onStateChange=[this]{ updateMonitorButtons(); };
    modeBtn.onStateChange=[this]{ updateMonitorButtons(); };
    updateMonitorButtons();
}
void NFDeEsserAudioProcessorEditor::updateMonitorButtons()
{
    audioBtn.setToggleState(!listenBtn.getToggleState(), juce::dontSendNotification);
    modeBtn.setLabel(modeBtn.getToggleState() ? "WIDE" : "SPLIT");
}
NFDeEsserAudioProcessorEditor::~NFDeEsserAudioProcessorEditor(){processor.apvts.state.removeListener(this);cancelPendingUpdate();setLookAndFeel(nullptr);}

juce::Rectangle<int> NFDeEsserAudioProcessorEditor::scaleBounds(juce::Rectangle<float> b) const
{
    return { juce::roundToInt(offsetX + b.getX()*layoutScale), juce::roundToInt(offsetY + b.getY()*layoutScale),
             juce::roundToInt(b.getWidth()*layoutScale), juce::roundToInt(b.getHeight()*layoutScale) };
}

void NFDeEsserAudioProcessorEditor::showMainMenu()
{
    juce::PopupMenu menu;
    menu.addItem(3, "About");
    juce::Component::SafePointer<NFDeEsserAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton),
        [safeThis](int result)
        {
            if (safeThis == nullptr || result == 0) return;
            if (result == 3) safeThis->showAbout();
        });
}

// Preset tab: factory presets, then save / load
void NFDeEsserAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto current = nfdeesser::PresetManager::getCurrentPresetName(processor.apvts);
    for (int i = 0; i < nfdeesser::kNumFactoryPresets; ++i)
        menu.addItem(100 + i, nfdeesser::kFactoryPresets[i].name, true, current == nfdeesser::kFactoryPresets[i].name);
    menu.addSeparator();
    menu.addItem(1, "Save Preset...");
    menu.addItem(2, "Load Preset...");
    juce::Component::SafePointer<NFDeEsserAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBar).withMinimumWidth(presetBar.getWidth()),
        [safeThis](int result)
        {
            if (safeThis == nullptr || result == 0) return;
            if (result == 1) safeThis->handleSavePreset();
            else if (result == 2) safeThis->handleLoadPreset();
            else if (result >= 100) nfdeesser::PresetManager::applyFactoryPreset(safeThis->processor.apvts, result - 100);
        });
}

void NFDeEsserAudioProcessorEditor::stepPreset(int direction)
{
    const auto current = nfdeesser::PresetManager::getCurrentPresetName(processor.apvts);
    int index = -1;
    for (int i = 0; i < nfdeesser::kNumFactoryPresets; ++i) if (current == nfdeesser::kFactoryPresets[i].name) { index = i; break; }
    const int n = nfdeesser::kNumFactoryPresets;
    index = index < 0 ? (direction > 0 ? 0 : n - 1) : (index + direction + n) % n;
    nfdeesser::PresetManager::applyFactoryPreset(processor.apvts, index);
}

void NFDeEsserAudioProcessorEditor::showAbout()
{
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "About NF De-Esser",
        juce::String("NF De-Esser V") + JucePlugin_VersionString + "\nNF Audio Tools - Nenno Fernando");
}

void NFDeEsserAudioProcessorEditor::handleSavePreset()
{
    presetFileChooser = std::make_unique<juce::FileChooser>("Save NF De-Esser Preset", nfdeesser::PresetManager::getPresetsDirectory(), "*.nfdeesserpreset");
    juce::Component::SafePointer<NFDeEsserAudioProcessorEditor> safeThis(this);
    const auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;
    presetFileChooser->launchAsync(flags, [safeThis](const juce::FileChooser& fc)
    {
        if (safeThis == nullptr) return;
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            if (!file.hasFileExtension("nfdeesserpreset")) file = file.withFileExtension("nfdeesserpreset");
            auto result = nfdeesser::PresetManager::savePreset(safeThis->processor.apvts, file);
            if (result.failed())
                juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "NF De-Esser", result.getErrorMessage());
        }
        safeThis->presetFileChooser.reset();
    });
}

void NFDeEsserAudioProcessorEditor::handleLoadPreset()
{
    presetFileChooser = std::make_unique<juce::FileChooser>("Load NF De-Esser Preset", nfdeesser::PresetManager::getPresetsDirectory(), "*.nfdeesserpreset");
    juce::Component::SafePointer<NFDeEsserAudioProcessorEditor> safeThis(this);
    presetFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& fc)
        {
            if (safeThis == nullptr) return;
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                auto result = nfdeesser::PresetManager::loadPreset(safeThis->processor.apvts, file);
                if (result.failed())
                    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "NF De-Esser", result.getErrorMessage());
            }
            safeThis->presetFileChooser.reset();
        });
}

// 270-degree tick ring around a round knob (13 ticks, major every 3rd), end labels at the lower corners.
void NFDeEsserAudioProcessorEditor::drawKnobScale(juce::Graphics& g,juce::Point<float> c,float radius,const juce::String& lo,const juce::String& hi)
{
    g.setColour(juce::Colours::white);
    for (int i = 0; i <= 12; ++i)
    {
        const float a = (-135.0f + (float) i * 22.5f) * juce::MathConstants<float>::pi / 180.0f;
        const juce::Point<float> dir(std::sin(a), -std::cos(a));
        const bool major = i % 3 == 0;
        g.drawLine(juce::Line<float>(c + dir*radius, c + dir*(radius + (major ? 8.0f : 4.0f))), major ? 2.0f : 1.2f);
    }
    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.drawText(lo, juce::Rectangle<float>(c.x - radius - 30.0f, c.y + radius*0.72f, 40.0f, 18.0f), juce::Justification::centred);
    g.drawText(hi, juce::Rectangle<float>(c.x + radius - 10.0f, c.y + radius*0.72f, 40.0f, 18.0f), juce::Justification::centred);
}

// Marks on both sides of a vertical fader, numbers on the left. Positions come from the slider itself so they always line up with the thumb.
void NFDeEsserAudioProcessorEditor::drawFaderScale(juce::Graphics& g,juce::Slider& fader,float cx,const std::vector<FaderTick>& ticks)
{
    g.setColour(juce::Colours::white);
    for (const auto& t : ticks)
    {
        const float y = ((float) fader.getY() + (float) fader.getPositionOfValue(t.value) - offsetY) / layoutScale;
        const float len = t.major ? 9.0f : 5.0f, th = t.major ? 2.0f : 1.2f;
        g.drawLine(cx - 24.0f - len, y, cx - 24.0f, y, th);
        g.drawLine(cx + 24.0f, y, cx + 24.0f + len, y, th);
        if (t.label.isNotEmpty())
        {
            g.setFont(juce::Font(juce::FontOptions(14.0f)));
            g.drawText(t.label, juce::Rectangle<float>(cx - 24.0f - len - 46.0f, y - 9.0f, 42.0f, 18.0f), juce::Justification::centredRight);
        }
    }
}

void NFDeEsserAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0c0e));
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::scale(layoutScale).translated(offsetX, offsetY));

    static const juce::Image chassis = readyAsset(NFDeEsserBinaryData::_01_chassis_800x800_png, NFDeEsserBinaryData::_01_chassis_800x800_pngSize);
    {
        juce::Graphics::ScopedSaveState s(g);
        g.setOpacity(1.0f);
        if (chassis.isValid()) g.drawImage(chassis, {0.0f,0.0f,800.0f,800.0f}, juce::RectanglePlacement::stretchToFit);
    }

    g.setColour(juce::Colour(0xffeef2ee));
    { static const juce::Image nfLogo=readyAsset(NFDeEsserBinaryData::_10_logo_nf_audio_tools_png,NFDeEsserBinaryData::_10_logo_nf_audio_tools_pngSize);
      if(nfLogo.isValid()) g.drawImage(nfLogo,juce::Rectangle<float>(42.0f,17.0f,108.0f,62.0f),juce::RectanglePlacement::centred); }
    g.drawLine(148.0f,28.0f,148.0f,59.0f,2.0f);
    g.setFont(juce::Font(juce::FontOptions(30.0f,juce::Font::bold)).withExtraKerningFactor(.08f));
    g.drawText("NF DE-ESSER",168,23,230,44,juce::Justification::centredLeft);

    // Left column: dark inset panel with four sections (AUDIO mode, FREQUENCY knob, RANGE knob, MONITOR).
    g.setColour(juce::Colour(0x38000000));
    g.fillRoundedRectangle(44.0f, 112.0f, 186.0f, 608.0f, 9.0f);
    g.setColour(juce::Colour(0x40ffffff));
    g.drawRoundedRectangle(44.0f, 112.0f, 186.0f, 608.0f, 9.0f, 1.0f);
    for (float dy : { 208.0f, 408.0f, 608.0f }) g.drawLine(58.0f, dy, 216.0f, dy, 1.0f);
    g.setColour(juce::Colours::white);g.setFont(juce::Font(juce::FontOptions(16.0f,juce::Font::bold)));
    g.drawText("AUDIO",     juce::Rectangle<int>((int)kLeftX-70,122,140,22), juce::Justification::centred);
    g.drawText("FREQUENCY", juce::Rectangle<int>((int)kLeftX-70,216,140,22), juce::Justification::centred);
    g.drawText("RANGE",     juce::Rectangle<int>((int)kLeftX-70,416,140,22), juce::Justification::centred);
    g.drawText("MONITOR",   juce::Rectangle<int>((int)kLeftX-70,616,140,22), juce::Justification::centred);
    drawKnobScale(g,{kLeftX,kFreqKnobY},kKnobBox*0.5f+2.0f,"2k","12k");
    drawKnobScale(g,{kLeftX,kRangeKnobY},kKnobBox*0.5f+2.0f,"0","20");

    {   std::vector<FaderTick> t; for(int v=-40; v<=0; v+=5) t.push_back({(double)v, v%10==0 ? juce::String(v) : juce::String(), v%10==0}); drawFaderScale(g,thresholdKnob,kThresholdX,t); }

    // Output meter scale (0 .. -30 dB), numbers to the left of the L meter, aligned with the meters
    {
        const float mTop = kFaderY + kThumbMargin, mH = kFaderH - 2.0f*kThumbMargin;
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        for (int v = 0; v >= -30; v -= 6)
        {
            const float y = mTop + (float)(-v) / 30.0f * mH;
            g.drawText(juce::String(v), juce::Rectangle<float>(kOutLX - 50.0f, y - 9.0f, 36.0f, 18.0f), juce::Justification::centredRight);
            g.drawLine(kOutLX - 10.0f, y, kOutLX - 4.0f, y, 1.4f);
        }
    }

    g.setColour(juce::Colours::white);g.setFont(juce::Font(juce::FontOptions(20.0f,juce::Font::bold)));
    const std::pair<float,const char*> names[]={{kThresholdX,"THRESHOLD"}};
    for (auto& n : names) g.drawText(n.second, juce::Rectangle<int>((int)n.first-80,636,160,24), juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(16.0f,juce::Font::bold)));
    g.drawText("ATTEN", juce::Rectangle<int>((int)kAttenX-50,636,100,24), juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText("IN",  juce::Rectangle<int>((int)kThresholdX+26,92,48,16), juce::Justification::centred);
    g.drawText("L",   juce::Rectangle<int>((int)kOutLX-8,613,40,16), juce::Justification::centred);
    g.drawText("R",   juce::Rectangle<int>((int)kOutRX-8,613,40,16), juce::Justification::centred);

    NFDeEsserLookAndFeel::drawScrew(g, {7.0f,   14.0f, 46.0f, 46.0f});
    NFDeEsserLookAndFeel::drawScrew(g, {747.0f, 14.0f, 46.0f, 46.0f});
    NFDeEsserLookAndFeel::drawScrew(g, {7.0f,   729.0f, 46.0f, 46.0f});
    NFDeEsserLookAndFeel::drawScrew(g, {747.0f, 729.0f, 46.0f, 46.0f});
    const bool powered = processor.apvts.getRawParameterValue("power")->load() > 0.5f;
    NFDeEsserLookAndFeel::drawLed(g, {699.0f, 24.0f, 20.0f, 20.0f}, powered);

    // Footer signature, flanked by thin lines.
    g.setColour(juce::Colours::white);g.setFont(15.0f);
    g.drawText("NF AUDIO TOOLS", juce::Rectangle<int>(0,758,800,18), juce::Justification::centred);
    juce::GlyphArrangement footerGlyphs;
    footerGlyphs.addLineOfText(g.getCurrentFont(), "NF AUDIO TOOLS", 0.0f, 0.0f);
    const float footerTextWidth = footerGlyphs.getBoundingBox(0,-1,true).getWidth();
    const float midX = 400.0f, lineY = 767.0f, gap = footerTextWidth*0.5f + 14.0f;
    g.drawLine(midX-170.0f, lineY, midX-gap, lineY, 1.4f);
    g.drawLine(midX+gap, lineY, midX+170.0f, lineY, 1.4f);

    // Version, bottom-left; derived from CMakeLists.txt's project(NFDeEsser VERSION ...).
    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.drawText("V" JucePlugin_VersionString, juce::Rectangle<int>(70,757,80,20), juce::Justification::centredLeft);

    g.setFont(juce::Font(juce::FontOptions(15.0f,juce::Font::bold)));
}

void NFDeEsserAudioProcessorEditor::resized()
{
    if (getWidth() > 0) processor.apvts.state.setProperty("uiWidth", getWidth(), nullptr);   // remembered for the next time the window opens
    const float scaleX = getWidth()  / 800.0f, scaleY = getHeight() / 800.0f;
    layoutScale = juce::jmin(scaleX, scaleY);
    offsetX = (getWidth()  - 800.0f * layoutScale) * 0.5f;
    offsetY = (getHeight() - 800.0f  * layoutScale) * 0.5f;

    thresholdKnob.setBounds(scaleBounds({kThresholdX-kFaderW*0.5f, kFaderY, kFaderW, kFaderH}));
    thresholdCap.setBounds(scaleBounds({kThresholdX-65.0f, 668.0f, 130.0f, 54.0f}));

    // left column
    modeBtn.setBounds(scaleBounds({kLeftX-60.0f, 152.0f, 120.0f, 30.0f}));
    freqKnob.setBounds(scaleBounds({kLeftX-kKnobBox*0.5f, kFreqKnobY-kKnobBox*0.5f, kKnobBox, kKnobBox}));
    freqCap.setBounds(scaleBounds({kLeftX-52.0f, 360.0f, 104.0f, 43.0f}));
    rangeKnob.setBounds(scaleBounds({kLeftX-kKnobBox*0.5f, kRangeKnobY-kKnobBox*0.5f, kKnobBox, kKnobBox}));
    rangeCap.setBounds(scaleBounds({kLeftX-52.0f, 560.0f, 104.0f, 43.0f}));
    audioBtn.setBounds(scaleBounds({kLeftX-60.0f, 644.0f, 120.0f, 30.0f}));
    listenBtn.setBounds(scaleBounds({kLeftX-60.0f, 682.0f, 120.0f, 30.0f}));

    power.setBounds(scaleBounds({679.0f, 43.0f, 66.0f, 66.0f}));
    // meters span the same travel as the faders' thumbs, so the Threshold fader reads against the input meter
    const float mTop = kFaderY + kThumbMargin, mH = kFaderH - 2.0f*kThumbMargin;
    inputMeter.setBounds(scaleBounds({kThresholdX+38.0f, mTop, 24.0f, mH}));
    outLMeter.setBounds(scaleBounds({kOutLX, mTop, 24.0f, mH}));
    outRMeter.setBounds(scaleBounds({kOutRX, mTop, 24.0f, mH}));
    // ATTEN: the bar (14px into the 80px box) is centred on kAttenX
    grMeter.setBounds(scaleBounds({kAttenX-27.0f, 112.0f, 80.0f, 500.0f}));
    logoButton.setBounds(scaleBounds({42.0f, 17.0f, 108.0f, 62.0f}));
    thresholdBubble.setBounds(scaleBounds({kThresholdX-38.0f, 84.0f, 76.0f, 24.0f}));
    menuButton.setBounds(scaleBounds({612.0f, 25.0f, 34.0f, 28.0f}));
    presetBar.setBounds(scaleBounds({439.0f, 28.0f, 157.0f, 21.0f}));
}
