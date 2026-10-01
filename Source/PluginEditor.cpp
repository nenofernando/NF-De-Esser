#include "PluginEditor.h"
#include "NFDeEsserBinaryData.h"
#include "FactoryPresets.h"
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "0.0.0-test"
#endif

namespace
{
juce::Image readyAsset(const char* data, int size) { return juce::ImageCache::getFromMemory(data, size); }

// Base layout: square, 800 x 800.
// Square layout (800 x 800 base): four vertical faders evenly spaced (150 apart); the vertical reduction meter sits under the Power button (same centre X).
constexpr float kFreqX = 120.0f, kThresholdX = 270.0f, kRangeX = 420.0f, kOutputX = 570.0f, kMeterX = 712.0f;
constexpr int kDefaultSize = 540;   // 800 x 800 base shown at 0.675, the same text scale as the 810 x 270 plug-ins
constexpr float kFaderW = 56.0f, kFaderY = 112.0f, kFaderH = 440.0f;   // vertical fader box (base units)

juce::String formatOut(double v){ auto s=juce::String(v,1); if(v>0.05) s="+"+s; else if(v>-0.05) s="0.0"; return s+" dB"; }
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
     freqCap("2-12 kHz",6500.0,false,formatFreq),thresholdCap("-40 to 0 dB",-20.0,true,formatDb),
     rangeCap("0 to 20 dB",8.0,false,formatRange),outputCap("-12 to +12 dB",0.0,false,formatOut)
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
    for(auto k:{K{&freqKnob,6500.0},K{&thresholdKnob,-20.0},K{&rangeKnob,8.0},K{&outputKnob,0.0}}){
        addAndMakeVisible(*k.s);
        k.s->setComponentID("fader");
        k.s->setSliderStyle(juce::Slider::LinearVertical);   // vertical faders (like the de-essers people know), same controls
        k.s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        k.s->setSliderSnapsToMousePosition(true);
        k.s->setScrollWheelEnabled(true);
        k.s->setDoubleClickReturnValue(true,k.def);
    }
    for(auto* c:{&freqCap,&thresholdCap,&rangeCap,&outputCap}) addAndMakeVisible(*c);
    addAndMakeVisible(power);power.setClickingTogglesState(true);
    addAndMakeVisible(listenBtn);listenBtn.setClickingTogglesState(true);listenBtn.setTooltip("Listen: plays only the band the de-esser is working on, to find the sibilance");
    for(auto* b:{&freqBubble,&thresholdBubble,&rangeBubble}) addAndMakeVisible(*b);
    addAndMakeVisible(outputBubble);
    addAndMakeVisible(grMeter);

    // Floating value readouts, only while the user is interacting (never on host automation).
    freqKnob.onValueChange      = [this]{ if (freqKnob.isMouseOverOrDragging())      freqBubble.showRaw(formatFreq(freqKnob.getValue())); };
    thresholdKnob.onValueChange = [this]{ if (thresholdKnob.isMouseOverOrDragging()) thresholdBubble.showRaw(formatDb(thresholdKnob.getValue())); };
    rangeKnob.onValueChange     = [this]{ if (rangeKnob.isMouseOverOrDragging())     rangeBubble.showRaw(formatRange(rangeKnob.getValue())); };
    outputKnob.onValueChange    = [this]{ if (outputKnob.isMouseOverOrDragging())    outputBubble.showRaw(formatOut(outputKnob.getValue())); };

    auto& a=processor.apvts;
    freqA=std::make_unique<SA>(a,"freq",freqKnob);thresholdA=std::make_unique<SA>(a,"threshold",thresholdKnob);rangeA=std::make_unique<SA>(a,"range",rangeKnob);
    freqCapA=std::make_unique<SA>(a,"freq",freqCap.slider);thresholdCapA=std::make_unique<SA>(a,"threshold",thresholdCap.slider);rangeCapA=std::make_unique<SA>(a,"range",rangeCap.slider);
    powerA=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a,"power",power);
    listenA=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(a,"listen",listenBtn);
    power.onStateChange=[this]{repaint();};
    outputGainA=std::make_unique<SA>(a,"outputGain",outputKnob);outputCapA=std::make_unique<SA>(a,"outputGain",outputCap.slider);
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

void NFDeEsserAudioProcessorEditor::drawScale(juce::Graphics& g,juce::Point<float> c,const std::vector<Tick>& ticks)
{
    const float radius = 85.0f;
    g.setColour(juce::Colours::white);
    for (const auto& t : ticks)
    {
        const float a = t.deg * juce::MathConstants<float>::pi / 180.0f;
        const juce::Point<float> dir(std::sin(a), -std::cos(a));
        const float len = t.major ? 13.0f : 6.0f;
        g.drawLine(juce::Line<float>(c + dir*radius, c + dir*(radius+len)), t.major ? 2.4f : 1.4f);
        if (t.label.isNotEmpty())
        {
            const auto p = c + dir*(radius+t.labelOffset);
            g.setFont(juce::Font(juce::FontOptions(t.fontSize)));
            g.drawText(t.label, juce::Rectangle<float>(p.x-26.0f, p.y-12.0f, 52.0f, 24.0f), juce::Justification::centred);
        }
    }
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
    g.fillAll(juce::Colour(0xff0a1b11));
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
      if(nfLogo.isValid()) g.drawImage(nfLogo,juce::Rectangle<float>(42.0f,3.0f,108.0f,62.0f),juce::RectanglePlacement::centred); }
    g.drawLine(148.0f,14.0f,148.0f,45.0f,2.0f);
    g.setFont(juce::Font(juce::FontOptions(30.0f,juce::Font::bold)).withExtraKerningFactor(.08f));
    g.drawText("NF DE-ESSER",168,9,230,44,juce::Justification::centredLeft);

    // Tick marks: 270-degree sweep, 0 deg = straight up, clockwise (matches the rotary parameters).
    auto stepped = [](std::initializer_list<const char*> labels){ std::vector<Tick> t; int i=0; const int n=(int)labels.size();
        for(auto* l:labels) t.push_back({-135.0f+(float)i++*270.0f/(float)(n-1), l, true, 15.0f}); return t; };
    auto continuous = [](const juce::String& lo,const juce::String& mid,const juce::String& hi)
    {
        std::vector<Tick> t; for(int i=0;i<=12;++i) t.push_back({-135.0f+i*22.5f, {}, i%3==0, 14.0f});
        t[0].label=lo; t[6].label=mid; t[12].label=hi; t[6].fontSize=16.0f; return t;
    };
    drawFaderScale(g,freqKnob,kFreqX,{{2000,"2k",true},{3000,"3k",true},{4000,"4k",true},{5000,"5k",true},{6000,"6k",true},{8000,"8k",true},{10000,"10k",true},{12000,"12k",true}});
    {   std::vector<FaderTick> t; for(int v=-40; v<=0; v+=5) t.push_back({(double)v, v%10==0 ? juce::String(v) : juce::String(), v%10==0}); drawFaderScale(g,thresholdKnob,kThresholdX,t); }
    {   std::vector<FaderTick> t; for(double v=0; v<=20.001; v+=2.5) { const bool maj = std::fmod(v,5.0) < 0.01; t.push_back({v, maj ? juce::String((int)v) : juce::String(), maj}); } drawFaderScale(g,rangeKnob,kRangeX,t); }
    {   std::vector<FaderTick> t; for(int v=-12; v<=12; v+=3) { const bool maj = v%6==0; t.push_back({(double)v, maj ? (v>0?"+"+juce::String(v):juce::String(v)) : juce::String(), maj}); } drawFaderScale(g,outputKnob,kOutputX,t); }

    g.setColour(juce::Colours::white);g.setFont(juce::Font(juce::FontOptions(20.0f,juce::Font::bold)));
    const std::pair<float,const char*> names[]={{kFreqX,"FREQUENCY"},{kThresholdX,"THRESHOLD"},{kRangeX,"RANGE"},{kOutputX,"OUTPUT"}};
    for (auto& n : names) g.drawText(n.second, juce::Rectangle<int>((int)n.first-80,576,160,24), juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(15.0f,juce::Font::bold)));
    g.drawText("REDUCTION", juce::Rectangle<int>((int)kMeterX-50,576,100,24), juce::Justification::centred);

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

    auto layoutBand = [&](juce::Slider& knob, ValueCapsule& cap, float cx)
    {
        knob.setBounds(scaleBounds({cx-kFaderW*0.5f, kFaderY, kFaderW, kFaderH}));
        cap.setBounds(scaleBounds({cx-65.0f, 608.0f, 130.0f, 54.0f}));
    };
    layoutBand(freqKnob,freqCap,kFreqX);layoutBand(thresholdKnob,thresholdCap,kThresholdX);
    layoutBand(rangeKnob,rangeCap,kRangeX);layoutBand(outputKnob,outputCap,kOutputX);

    listenBtn.setBounds(scaleBounds({kFreqX-30.0f, 676.0f, 60.0f, 28.0f}));
    power.setBounds(scaleBounds({679.0f, 43.0f, 66.0f, 66.0f}));
    // Bar (27px into the 80px box) is centred on the Power button's X.
    grMeter.setBounds(scaleBounds({kMeterX-27.0f, 112.0f, 80.0f, 440.0f}));
    outputBubble.setBounds(scaleBounds({kOutputX+44.0f, 300.0f, 76.0f, 24.0f}));
    logoButton.setBounds(scaleBounds({42.0f, 3.0f, 108.0f, 62.0f}));
    freqBubble.setBounds(scaleBounds({kFreqX+44.0f, 300.0f, 76.0f, 24.0f}));
    thresholdBubble.setBounds(scaleBounds({kThresholdX+44.0f, 300.0f, 76.0f, 24.0f}));
    rangeBubble.setBounds(scaleBounds({kRangeX+44.0f, 300.0f, 76.0f, 24.0f}));
    menuButton.setBounds(scaleBounds({612.0f, 25.0f, 34.0f, 28.0f}));
    presetBar.setBounds(scaleBounds({439.0f, 28.0f, 157.0f, 21.0f}));
}
