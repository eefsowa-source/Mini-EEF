#include "PluginEditor.h"
#include "FactoryPresets.h"

#if JUCE_MAC
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace
{
const juce::Colour bg       (0xff07171c);
const juce::Colour panel    (0xff183f49);
const juce::Colour edge     (0xff62bfd0);
const juce::Colour ivory    (0xffe1faff);
const juce::Colour mint     (0xff39d4d8);
const juce::Colour mintGlow (0xff8cf4ff);
const juce::Colour mintDeep (0xff0b5365);
constexpr int headerY = 9;
constexpr int headerHeight = 78;
constexpr int contentTop = 96;
constexpr int contentMargin = 16;
constexpr int moduleGap = 10;
constexpr int oscillatorHeight = 430;
constexpr int oscillatorCardsHeight = 342;
constexpr int oscillatorStripHeight = 78;
constexpr int lowerSectionHeight = 180;
constexpr int fxHeight = 108;
constexpr int bottomMargin = 16;

struct HeaderLayout
{
    int meterX = 0;
    int qualityX = 0;
    int qualityWidth = 110;
    int patchX = 0;
    int patchWidth = 180;
    int actionsX = 0;
    int actionWidth = 58;
};

HeaderLayout makeHeaderLayout (int width)
{
    HeaderLayout layout;
    layout.meterX = width - 170;
    const int controlsRight = layout.meterX - 16;

    constexpr int actionsWidth = 3 * 58 + 2 * 6;
    layout.actionsX = controlsRight - actionsWidth;
    layout.patchX = layout.actionsX - 14 - layout.patchWidth;
    layout.qualityX = layout.patchX - 12 - layout.qualityWidth;

    return layout;
}
}

EonMiniEEFEditor::EonMiniEEFEditor(EonMiniEEFProcessor& p):AudioProcessorEditor(p),processor(p)
{
    setLookAndFeel (&analogLookAndFeel);
    presetMenu.addItemList (FactoryPresets::names(), 1);
    presetMenu.setSelectedItemIndex (0, juce::dontSendNotification);
    presetMenu.setColour(juce::ComboBox::backgroundColourId, panel);
    presetMenu.setColour(juce::ComboBox::textColourId, ivory);
    presetMenu.setColour(juce::ComboBox::outlineColourId, edge);
    presetMenu.onChange = [this] { applyPreset (presetMenu.getSelectedItemIndex()); };
    addAndMakeVisible (presetMenu);
    oversamplingMode.addItemList ({ "1x Eco", "2x Quality", "4x High" }, 1);
    oversamplingMode.setColour (juce::ComboBox::backgroundColourId, panel);
    oversamplingMode.setColour (juce::ComboBox::textColourId, ivory);
    oversamplingMode.setColour (juce::ComboBox::outlineColourId, edge);
    addAndMakeVisible (oversamplingMode);
    for (auto* button : { &initButton, &saveButton, &loadButton })
    {
        button->setColour (juce::TextButton::buttonColourId, panel);
        button->setColour (juce::TextButton::textColourOffId, ivory);
        button->setColour (juce::TextButton::textColourOnId, mint);
        addAndMakeVisible (button);
    }
    initButton.onClick = [this] { applyPreset (0); presetMenu.setSelectedItemIndex (0, juce::dontSendNotification); };
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };
    osc1Wave.addItemList({"Saw","Square","Triangle","Sine"},1); osc2Wave.addItemList({"Saw","Square","Triangle","Sine"},1); osc3Wave.addItemList({"Saw","Square","Triangle","Sine"},1); osc4Wave.addItemList({"Saw","Square","Triangle","Sine"},1); voiceMode.addItemList({"Poly","Mono"},1); filterMode.addItemList({"LPF","HPF","BPF"},1);
    for(auto* c:{&osc1Wave,&osc2Wave,&osc3Wave,&osc4Wave,&voiceMode,&filterMode}){c->setColour(juce::ComboBox::backgroundColourId,panel);c->setColour(juce::ComboBox::textColourId,ivory);c->setColour(juce::ComboBox::outlineColourId,edge);addAndMakeVisible(c);}
    osc1Attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::osc1Wave,osc1Wave); osc2Attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::osc2Wave,osc2Wave); osc3Attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::osc3Wave,osc3Wave); osc4Attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::osc4Wave,osc4Wave); modeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::voiceMode,voiceMode); filterModeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::filterMode,filterMode);
    oversamplingAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::oversampling,oversamplingMode);
    auto bind=[&](juce::Slider& s,const char* id,const juce::String& name){knob(s,name);return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts,id,s);};
    constexpr std::array<const char*, 4> levelIds { ParamIDs::osc1Level, ParamIDs::osc2Level, ParamIDs::osc3Level, ParamIDs::osc4Level };
    constexpr std::array<const char*, 4> coarseIds { ParamIDs::osc1Coarse, ParamIDs::osc2Coarse, ParamIDs::osc3Coarse, ParamIDs::osc4Coarse };
    constexpr std::array<const char*, 4> fineIds { ParamIDs::osc1Fine, ParamIDs::osc2Fine, ParamIDs::osc3Fine, ParamIDs::osc4Fine };
    constexpr std::array<const char*, 4> phaseIds { ParamIDs::osc1Phase, ParamIDs::osc2Phase, ParamIDs::osc3Phase, ParamIDs::osc4Phase };
    constexpr std::array<const char*, 4> panIds { ParamIDs::osc1Pan, ParamIDs::osc2Pan, ParamIDs::osc3Pan, ParamIDs::osc4Pan };
    constexpr std::array<const char*, 4> pulseWidthIds { ParamIDs::osc1PulseWidth, ParamIDs::osc2PulseWidth, ParamIDs::osc3PulseWidth, ParamIDs::osc4PulseWidth };
    for (size_t oscillator = 0; oscillator < 4; ++oscillator)
    {
        oscLevelAttachment[oscillator] = bind (oscLevel[oscillator], levelIds[oscillator], "LEVEL");
        oscCoarseAttachment[oscillator] = bind (oscCoarse[oscillator], coarseIds[oscillator], "COARSE");
        oscFineAttachment[oscillator] = bind (oscFine[oscillator], fineIds[oscillator], "FINE");
        oscPhaseAttachment[oscillator] = bind (oscPhase[oscillator], phaseIds[oscillator], "PHASE");
        oscPanAttachment[oscillator] = bind (oscPan[oscillator], panIds[oscillator], "PAN");
        oscPulseWidthAttachment[oscillator] = bind (oscPulseWidth[oscillator], pulseWidthIds[oscillator], "PULSE WIDTH");
    }
    noiseMixAttachment=bind(noiseMix,ParamIDs::noiseMix,"NOISE"); amDepthAttachment=bind(amDepth,ParamIDs::amDepth,"AM DEPTH"); unisonVoicesAttachment=bind(unisonVoices,ParamIDs::unisonVoices,"UNISON"); unisonDetuneAttachment=bind(unisonDetune,ParamIDs::unisonDetune,"DETUNE"); unisonSpreadAttachment=bind(unisonSpread,ParamIDs::unisonSpread,"SPREAD"); unisonPhaseAttachment=bind(unisonPhase,ParamIDs::unisonPhase,"UNI PHASE"); attackAttachment=bind(attack,ParamIDs::attack,"ATTACK"); decayAttachment=bind(decay,ParamIDs::decay,"DECAY"); sustainAttachment=bind(sustain,ParamIDs::sustain,"SUSTAIN"); releaseAttachment=bind(release,ParamIDs::release,"RELEASE"); cutoffAttachment=bind(cutoff,ParamIDs::cutoff,"CUTOFF"); resonanceAttachment=bind(resonance,ParamIDs::resonance,"RESONANCE"); gainAttachment=bind(gain,ParamIDs::gain,"OUTPUT"); driveAttachment=bind(drive,ParamIDs::drive,"DRIVE");
    lfoRateAttachment=bind(lfoRate,ParamIDs::lfoRate,"LFO RATE"); lfoDepthAttachment=bind(lfoDepth,ParamIDs::lfoDepth,"LFO CUTOFF"); lfoPitchAttachment=bind(lfoPitch,ParamIDs::lfoPitch,"LFO PITCH"); velocityAttachment=bind(velocityAmount,ParamIDs::velocityAmount,"VELOCITY");
    fxWetAttachment=bind(fxWet,ParamIDs::fxWet,"FX WET"); delayTimeAttachment=bind(delayTime,ParamIDs::delayTime,"DLY TIME"); delayFeedbackAttachment=bind(delayFeedback,ParamIDs::delayFeedback,"DLY FDBK"); chorusDepthAttachment=bind(chorusDepth,ParamIDs::chorusDepth,"CHO DEPTH"); chorusRateAttachment=bind(chorusRate,ParamIDs::chorusRate,"CHO RATE"); chorusMixAttachment=bind(chorusMix,ParamIDs::chorusMix,"CHO MIX"); reverbMixAttachment=bind(reverbMix,ParamIDs::reverbMix,"REVERB");
    // Keep every physical control visible, but never shrink the panel into the
    // former overlapping layout. Hosts can still scale beyond this minimum.
    setResizable (true, true);
    setResizeLimits (1200, 800, 1600, 1000);
    setSize (1320, 860);
    startTimerHz (30);
}
void EonMiniEEFEditor::timerCallback()
{
#if JUCE_MAC
    if (! windowSharingConfigured)
    {
        if (auto* peer = getPeer())
        {
            using ObjCId = void*;
            using SendId = ObjCId (*) (ObjCId, SEL);
            using SendSharing = void (*) (ObjCId, SEL, unsigned long);
            auto* view = static_cast<ObjCId> (peer->getNativeHandle());
            if (view != nullptr)
            {
                auto* window = reinterpret_cast<SendId> (objc_msgSend) (view, sel_registerName ("window"));
                auto* target = window != nullptr ? window : view;
                reinterpret_cast<SendSharing> (objc_msgSend) (target, sel_registerName ("setSharingType:"), 1UL);
                windowSharingConfigured = true;
            }
        }
    }
#endif
    meterLeft = juce::jmax (processor.consumePeakLeft(), meterLeft * 0.82f);
    meterRight = juce::jmax (processor.consumePeakRight(), meterRight * 0.82f);
    repaint (getWidth() - 190, 16, 176, 58);
}
void EonMiniEEFEditor::applyPreset (int index)
{
    FactoryPresets::apply (processor, index);
}
void EonMiniEEFEditor::savePreset()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Save EON preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.eonpreset");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser) {
            auto file = chooser.getResult();
            if (file != juce::File()) { juce::MemoryBlock state; processor.getStateInformation (state); file.replaceWithData (state.getData(), state.getSize()); }
            fileChooser.reset();
        });
}
void EonMiniEEFEditor::loadPreset()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load EON preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.eonpreset");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser) {
            auto file = chooser.getResult();
            if (file.existsAsFile()) { juce::MemoryBlock state; if (file.loadFileAsData (state)) processor.setStateInformation (state.getData(), (int) state.getSize()); }
            fileChooser.reset();
        });
}
void EonMiniEEFEditor::knob(juce::Slider& s,const juce::String& name)
{
    s.setName (name);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    // The custom renderer engraves the parameter name at rest and swaps it
    // for the exact value only while a deliberate gesture is in progress.
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.onDragStart = [&s] { s.getProperties().set ("eon.dragging", true); s.repaint(); };
    s.onDragEnd = [&s] { s.getProperties().set ("eon.dragging", false); s.repaint(); };
    s.setColour (juce::Slider::rotarySliderFillColourId, mint);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, edge);
    s.setColour (juce::Slider::thumbColourId, ivory);
    s.setColour (juce::Slider::textBoxTextColourId, ivory);
    addAndMakeVisible (s);
}
void EonMiniEEFEditor::paint (juce::Graphics& g)
{
    const auto w = getWidth();
    const auto h = getHeight();

    juce::ColourGradient chassis (juce::Colour (0xff275b68), 0.0f, 0.0f,
                                  bg, 0.0f, (float) h, false);
    g.setGradientFill (chassis);
    g.fillRect (getLocalBounds());
    // A small, deterministic brushing pass keeps the chassis tactile without
    // image assets or per-frame random noise.
    for (int y = 6; y < h; y += 4)
    {
        g.setColour ((y / 4) % 2 == 0 ? juce::Colours::white.withAlpha (0.018f)
                                      : juce::Colours::black.withAlpha (0.026f));
        g.fillRect (0, y, w, 1);
    }

    const auto header = juce::Rectangle<float> (12.0f, (float) headerY, (float) w - 24.0f, (float) headerHeight);
    g.setColour (juce::Colours::black.withAlpha (0.58f));
    g.fillRoundedRectangle (header.translated (0.0f, 0.8f), 6.0f);
    juce::ColourGradient headerMetal (juce::Colour (0xff34707e), header.getCentreX(), header.getY(),
                                      juce::Colour (0xff0a252d), header.getCentreX(), header.getBottom(), false);
    g.setGradientFill (headerMetal);
    g.fillRoundedRectangle (header, 6.0f);
    g.setColour (juce::Colour (0xff78c3d0).withAlpha (0.75f));
    g.drawRoundedRectangle (header, 6.0f, 1.0f);
    g.setColour (juce::Colour (0xff090a0b).withAlpha (0.88f));
    g.drawRoundedRectangle (header.reduced (1.6f), 5.0f, 0.65f);
    juce::ColourGradient topRail (mintGlow, 0.0f, 0.0f,
                                  mintDeep, (float) w, 0.0f, false);
    g.setGradientFill (topRail);
    g.fillRect (0, 0, w, 4);

    const auto headerLayout = makeHeaderLayout (w);
    auto headerCaption = [&] (const juce::String& text, int x, int width, int y)
    {
        g.setColour (juce::Colour (0xffd6fff4).withAlpha (0.90f));
        g.setFont (EonAnalogLookAndFeel::panelFont (11.0f, true, 0.94f));
        g.drawText (text, x, y, width, 12, juce::Justification::left);
    };

    g.setColour (ivory);
    g.setFont (EonAnalogLookAndFeel::displayFont (30.0f));
    g.drawText ("EEF-JP8000", 24, 16, 286, 31, juce::Justification::left);
    g.setColour (mintGlow.withAlpha (0.92f));
    g.setFont (EonAnalogLookAndFeel::signatureFont (21.0f));
    g.drawText ("aoi yume", 25, 50, 275, 25, juce::Justification::left);
    headerCaption ("QUALITY", headerLayout.qualityX, headerLayout.qualityWidth, 13);
    headerCaption ("PATCH", headerLayout.patchX, headerLayout.patchWidth, 13);
    headerCaption ("ACTIONS", headerLayout.actionsX, 3 * headerLayout.actionWidth + 12, 13);
    g.fillRect (headerLayout.meterX - 12, 18, 1, 54);
    g.setColour (mint);
    g.setFont (EonAnalogLookAndFeel::panelFont (11.8f, true, 0.92f));
    g.drawText ("OUTPUT", headerLayout.meterX, 20, 145, 14, juce::Justification::right);
    auto meter = [&] (int y, float value, const juce::String& label)
    {
        constexpr int segments = 12;
        constexpr float segmentW = 8.0f;
        constexpr float segmentGap = 2.5f;
        const float clamped = juce::jlimit (0.0f, 1.0f, value);
        for (int segment = 0; segment < segments; ++segment)
        {
            const bool lit = clamped * (float) segments > (float) segment;
            const auto lamp = segment >= 10 ? mintGlow
                            : (segment >= 8 ? mint : mintDeep.brighter (0.20f));
            const auto lampBounds = juce::Rectangle<float> ((float) headerLayout.meterX + 28.0f + (float) segment * (segmentW + segmentGap),
                                                             (float) y, segmentW, 5.0f);
            g.setColour (juce::Colours::black.withAlpha (0.62f));
            g.fillRoundedRectangle (lampBounds.translated (0.0f, 1.0f), 1.5f);
            g.setColour (lit ? lamp : lamp.darker (0.75f).withAlpha (0.55f));
            g.fillRoundedRectangle (lampBounds, 1.5f);
        }
        g.setColour (ivory.withAlpha (0.78f));
        g.setFont (EonAnalogLookAndFeel::readoutFont (10.5f));
        g.drawText (label, headerLayout.meterX, y - 3, 20, 12, juce::Justification::left);
    };
    meter (47, meterLeft, "L"); meter (62, meterRight, "R");

    auto box = [&] (juce::Rectangle<int> r, const juce::String& title, juce::Colour accent = mint)
    {
        const auto outer = r.toFloat();
        const auto inner = outer.reduced (2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.fillRoundedRectangle (outer.translated (0.0f, 3.0f), 8.0f);
        juce::ColourGradient bezel (juce::Colour (0xff65b4c2), outer.getCentreX(), outer.getY(),
                                    juce::Colour (0xff071b21), outer.getCentreX(), outer.getBottom(), false);
        g.setGradientFill (bezel);
        g.fillRoundedRectangle (outer, 8.0f);
        juce::ColourGradient module (panel.brighter (0.11f), inner.getCentreX(), inner.getY(),
                                     panel.darker (0.14f), inner.getCentreX(), inner.getBottom(), false);
        g.setGradientFill (module);
        g.fillRoundedRectangle (inner, 6.2f);
        g.setColour (juce::Colours::black.withAlpha (0.70f));
        g.drawRoundedRectangle (inner, 6.2f, 0.7f);
        g.setColour (accent.withAlpha (0.62f));
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) r.getX() + 8.0f,
                                                         (float) r.getY() + 7.0f,
                                                         (float) r.getWidth() - 16.0f, 2.5f), 1.2f);
        g.setColour (accent);
        g.setFont (EonAnalogLookAndFeel::panelFont (14.8f, true, 0.93f));
        g.drawText (title, r.getX() + 13, r.getY() + 11, r.getWidth() - 26, 19, juce::Justification::left);
        g.setColour (juce::Colour (0xffa6e4ed).withAlpha (0.46f));
        g.fillRect (r.getX() + 12, r.getY() + 38, r.getWidth() - 24, 1);
        for (const auto corner : { juce::Point<int> (r.getX() + 9, r.getY() + 9),
                                   juce::Point<int> (r.getRight() - 9, r.getY() + 9),
                                   juce::Point<int> (r.getX() + 9, r.getBottom() - 9),
                                   juce::Point<int> (r.getRight() - 9, r.getBottom() - 9) })
        {
            g.setColour (juce::Colours::black.withAlpha (0.80f));
            g.fillEllipse ((float) corner.x - 2.4f, (float) corner.y - 1.7f, 4.8f, 4.8f);
            g.setColour (juce::Colour (0xff5796a2));
            g.fillEllipse ((float) corner.x - 2.1f, (float) corner.y - 2.2f, 4.2f, 4.2f);
            g.setColour (bg);
            g.fillEllipse ((float) corner.x - 1.4f, (float) corner.y - 1.5f, 2.8f, 2.8f);
            g.setColour (juce::Colour (0xffbdf7ff).withAlpha (0.50f));
            g.drawLine ((float) corner.x - 0.8f, (float) corner.y - 0.3f,
                        (float) corner.x + 0.8f, (float) corner.y - 0.3f, 0.7f);
        }
    };
    const int m = contentMargin, gap = moduleGap, top = contentTop, topH = oscillatorHeight;
    const int cardW = (w - 2 * m - gap) / 2;
    const int cardH = (oscillatorCardsHeight - gap) / 2;
    box ({ m, top, cardW, cardH }, "OSCILLATOR 1  |  PRIMARY");
    box ({ m + cardW + gap, top, cardW, cardH }, "OSCILLATOR 2  |  HARMONIC");
    box ({ m, top + cardH + gap, cardW, cardH }, "OSCILLATOR 3  |  LAYER");
    box ({ m + cardW + gap, top + cardH + gap, cardW, cardH }, "OSCILLATOR 4  |  LAYER");
    box ({ m, top + oscillatorCardsHeight + gap, w - 2 * m, oscillatorStripHeight }, "OSCILLATOR GLOBAL  |  VOICE  |  UNISON  |  NOISE", mintGlow);
    const int fxY = getHeight() - bottomMargin - fxHeight;
    const int lowerY = top + topH + gap;
    const int lowerH = lowerSectionHeight;
    const int envW = (w - 2 * m - 2 * gap) * 36 / 100;
    const int filterW = (w - 2 * m - 2 * gap) * 24 / 100;
    box ({ m, lowerY, envW, lowerH }, "AMP ENVELOPE  |  SHAPE");
    box ({ m + envW + gap, lowerY, filterW, lowerH }, "FILTER  |  OUTPUT");
    box ({ m + envW + filterW + 2 * gap, lowerY, w - m - (m + envW + filterW + 2 * gap), lowerH }, "MODULATION  |  MOTION", mintGlow);
    box ({ m, fxY, w - 2 * m, fxHeight }, "GLOBAL FX  |  DELAY  |  CHORUS  |  REVERB", mintGlow);
    g.setColour (ivory.withAlpha (0.76f));
    g.setFont (EonAnalogLookAndFeel::panelFont (12.5f, true, 0.93f));
    g.drawText ("VCO A + VCO B  ->  FILTER  ->  AMP  ->  MOD  ->  FX", m + 10, fxY - 16, 520, 14, juce::Justification::left);
}
void EonMiniEEFEditor::resized()
{
    const int w = getWidth(), h = getHeight(), m = contentMargin, gap = moduleGap, top = contentTop, topH = oscillatorHeight;
    const auto headerLayout = makeHeaderLayout (w);
    oversamplingMode.setBounds (headerLayout.qualityX, 27, headerLayout.qualityWidth, 28);
    presetMenu.setBounds (headerLayout.patchX, 27, headerLayout.patchWidth, 28);
    initButton.setBounds (headerLayout.actionsX, 27, headerLayout.actionWidth, 28);
    saveButton.setBounds (headerLayout.actionsX + headerLayout.actionWidth + 6, 27, headerLayout.actionWidth, 28);
    loadButton.setBounds (headerLayout.actionsX + 2 * (headerLayout.actionWidth + 6), 27, headerLayout.actionWidth, 28);
    const int cardW = (w - 2 * m - gap) / 2;
    const int cardH = (oscillatorCardsHeight - gap) / 2;
    std::array<juce::ComboBox*, 4> waveBoxes { &osc1Wave, &osc2Wave, &osc3Wave, &osc4Wave };
    for (int oscillator = 0; oscillator < 4; ++oscillator)
    {
        const int column = oscillator % 2;
        const int row = oscillator / 2;
        const int cardX = m + column * (cardW + gap);
        const int cardY = top + row * (cardH + gap);
        waveBoxes[static_cast<size_t> (oscillator)]->setBounds (cardX + 12, cardY + 43, 150, 26);
        constexpr int controlGap = 3;
        const int controlsX = cardX + 170;
        const int controlW = (cardW - 182 - 5 * controlGap) / 6;
        std::array<juce::Slider*, 6> controls {
            &oscLevel[oscillator], &oscCoarse[oscillator], &oscFine[oscillator],
            &oscPhase[oscillator], &oscPan[oscillator], &oscPulseWidth[oscillator]
        };
        for (int control = 0; control < 6; ++control)
            controls[static_cast<size_t> (control)]->setBounds (
                controlsX + control * (controlW + controlGap), cardY + 48, controlW, cardH - 57);
    }
    const int stripY = top + oscillatorCardsHeight + gap;
    voiceMode.setBounds (m + 14, stripY + 40, 142, 26);
    const int stripKnobX = m + 170;
    const int stripKnobW = (w - stripKnobX - m - 12) / 6;
    std::array<juce::Slider*, 6> stripControls {
        &unisonVoices, &unisonDetune, &unisonSpread, &unisonPhase, &noiseMix, &amDepth
    };
    for (int control = 0; control < 6; ++control)
        stripControls[static_cast<size_t> (control)]->setBounds (
            stripKnobX + control * stripKnobW, stripY + 30, stripKnobW - 4, 44);
    const int fxY = h - bottomMargin - fxHeight;
    const int lowerY = top + topH + gap;
    const int lowerH = lowerSectionHeight;
    const int envW = (w - 2 * m - 2 * gap) * 36 / 100, filterW = (w - 2 * m - 2 * gap) * 24 / 100;
    int x = m + 18;
    // Keep four envelope controls inside the panel at the supported 1040 px minimum
    // editor width while allowing them to grow on wider layouts.
    const int knobW = juce::jlimit (64, 118, (envW - 64) / 4);
    for (auto* s : { &attack, &decay, &sustain, &release }) { s->setBounds (x, lowerY + 45, knobW, lowerH - 54); x += knobW + 8; }
    x = m + envW + gap + 18;
    const int filterInnerW = juce::jmax (150, filterW - 36), filterGap = 10;
    const int filterKnobW = juce::jlimit (64, 108, (filterInnerW - filterGap) / 2);
    filterMode.setBounds (x, lowerY + 44, filterInnerW, 26);
    const int filterSmallW = juce::jmax (52, (filterInnerW - 18) / 4);
    cutoff.setBounds (x, lowerY + 78, filterSmallW, lowerH - 86);
    resonance.setBounds (x + filterSmallW + 6, lowerY + 78, filterSmallW, lowerH - 86);
    gain.setBounds (x + 2 * (filterSmallW + 6), lowerY + 78, filterSmallW, lowerH - 86);
    drive.setBounds (x + 3 * (filterSmallW + 6), lowerY + 78, filterSmallW, lowerH - 86);
    const int modX = m + envW + filterW + 2 * gap + 18;
    const int modW = w - modX - m - 12;
    x = modX; const int modKnobW = juce::jmax (52, (modW - 24) / 4);
    for (auto* s : { &lfoRate, &lfoDepth, &lfoPitch, &velocityAmount }) { s->setBounds (x, lowerY + 45, modKnobW, lowerH - 54); x += modKnobW + 8; }
    const int fxW = (w - 2 * m - 60) / 7; x = m + 12;
    for (auto* s : { &fxWet, &delayTime, &delayFeedback, &chorusDepth, &chorusRate, &chorusMix, &reverbMix }) { s->setBounds (x, fxY + 30, fxW, 74); x += fxW + 6; }
}
