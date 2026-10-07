#include "PluginEditor.h"
#include "FactoryPresets.h"

#if JUCE_MAC
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace
{
// Analogue-synth palette: warm walnut cheeks and a sanded aluminium panel
// under cream silk-screening, with amber pilot lamps.  The previous teal
// powder-coat read as digital gear; analogue panels are warmer, lower in
// saturation, and use a cream/brown axis instead of a cyan one.
const juce::Colour bg       (0xff241c16);
const juce::Colour panel    (0xFF4C433A);
const juce::Colour edge     (0xffb9a882);
const juce::Colour ivory    (0xffece0c8);
const juce::Colour mint     (0xffd9a441);
const juce::Colour mintGlow (0xfff6d78c);
const juce::Colour mintDeep (0xff6b4a1c);
const juce::Colour walnut   (0xff3a2a1e);
const juce::Colour brass    (0xffc9a15c);
const juce::Colour cream    (0xfff2e7d2);
constexpr int headerY = 9;
constexpr int headerHeight = 78;
constexpr int contentTop = 90;
// A wider margin than a digital panel would use, so the walnut cheeks and the
// top rail actually read as a cabinet around the instrument, not a border.
constexpr int contentMargin = 26;
constexpr int moduleGap = 10;
constexpr int oscillatorCardsHeight = 258;
constexpr int oscillatorStripHeight = 112;
// Cards hold a primary dial (62) plus its caption.  The strip holds a
// standard dial (50).  Both used to be clamped well below those grades.
constexpr int oscillatorHeight = oscillatorCardsHeight + moduleGap + oscillatorStripHeight;
constexpr int lowerSectionHeight = 190;
// The FX strip only needs one standard row.  The old 152 px floor left an
// empty metal band at 800 px; that height now belongs to the oscillator rows.
constexpr int fxMinimumHeight = 112;
constexpr int fxMaximumHeight = 268;
constexpr int bottomMargin = 8;
// contentTop + oscillator + gap + lower + gap + fx minimum + bottom margin.
static_assert (contentTop + oscillatorHeight + moduleGap + lowerSectionHeight
                   + moduleGap + fxMinimumHeight + bottomMargin
               == 800,
               "minimum editor height drifted from the layout constants");
// The layout constants below add up to a fixed 800 px of height, so this is the
// real floor.  setResizeLimits declares it to the host and resized() enforces
// it, because a resize limit is a request and a host may still deliver less.
constexpr int minimumEditorWidth = 1200;
constexpr int minimumEditorHeight = 800;

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
    driveCurveMode.addItemList ({ "SYM", "ASYM", "TUBE" }, 1);
    driveCurveMode.setColour (juce::ComboBox::backgroundColourId, panel);
    driveCurveMode.setColour (juce::ComboBox::textColourId, ivory);
    driveCurveMode.setColour (juce::ComboBox::outlineColourId, edge);
    addAndMakeVisible (driveCurveMode);
    lfoShapeMode.addItemList ({"SINE","TRI","S&H"}, 1);
    lfoShapeMode.setColour (juce::ComboBox::backgroundColourId, panel);
    lfoShapeMode.setColour (juce::ComboBox::textColourId, ivory);
    lfoShapeMode.setColour (juce::ComboBox::outlineColourId, edge);
    addAndMakeVisible (lfoShapeMode);
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
    osc1Wave.addItemList({"Saw","Square","Triangle","Sine","Analog"},1); osc2Wave.addItemList({"Saw","Square","Triangle","Sine","Analog"},1); osc3Wave.addItemList({"Saw","Square","Triangle","Sine","Analog"},1); osc4Wave.addItemList({"Saw","Square","Triangle","Sine","Analog"},1); voiceMode.addItemList({"Poly","Mono"},1); filterMode.addItemList({"LPF","HPF","BPF","LPF24"},1);
    for(auto* c:{&osc1Wave,&osc2Wave,&osc3Wave,&osc4Wave,&voiceMode,&filterMode}){c->setColour(juce::ComboBox::backgroundColourId,panel);c->setColour(juce::ComboBox::textColourId,ivory);c->setColour(juce::ComboBox::outlineColourId,edge);addAndMakeVisible(c);}
    osc1Attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::osc1Wave,osc1Wave); osc2Attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::osc2Wave,osc2Wave); osc3Attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::osc3Wave,osc3Wave); osc4Attachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::osc4Wave,osc4Wave); modeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::voiceMode,voiceMode); filterModeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::filterMode,filterMode);
    oversamplingAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::oversampling,oversamplingMode);
    driveCurveAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::driveCurve,driveCurveMode); lfoShapeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts,ParamIDs::lfoShape,lfoShapeMode);
    auto bind=[&](juce::Slider& s,const char* id,const juce::String& name,
                 EonAnalogLookAndFeel::DialGrade grade = EonAnalogLookAndFeel::standard)
    { knob(s,name,grade); return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts,id,s); };
    constexpr std::array<const char*, 4> levelIds { ParamIDs::osc1Level, ParamIDs::osc2Level, ParamIDs::osc3Level, ParamIDs::osc4Level };
    constexpr std::array<const char*, 4> coarseIds { ParamIDs::osc1Coarse, ParamIDs::osc2Coarse, ParamIDs::osc3Coarse, ParamIDs::osc4Coarse };
    constexpr std::array<const char*, 4> fineIds { ParamIDs::osc1Fine, ParamIDs::osc2Fine, ParamIDs::osc3Fine, ParamIDs::osc4Fine };
    constexpr std::array<const char*, 4> phaseIds { ParamIDs::osc1Phase, ParamIDs::osc2Phase, ParamIDs::osc3Phase, ParamIDs::osc4Phase };
    constexpr std::array<const char*, 4> panIds { ParamIDs::osc1Pan, ParamIDs::osc2Pan, ParamIDs::osc3Pan, ParamIDs::osc4Pan };
    constexpr std::array<const char*, 4> pulseWidthIds { ParamIDs::osc1PulseWidth, ParamIDs::osc2PulseWidth, ParamIDs::osc3PulseWidth, ParamIDs::osc4PulseWidth };
    for (size_t oscillator = 0; oscillator < 4; ++oscillator)
    {
        constexpr auto primary = EonAnalogLookAndFeel::primary;
        oscLevelAttachment[oscillator] = bind (oscLevel[oscillator], levelIds[oscillator], "LEVEL", primary);
        oscCoarseAttachment[oscillator] = bind (oscCoarse[oscillator], coarseIds[oscillator], "COARSE", primary);
        oscFineAttachment[oscillator] = bind (oscFine[oscillator], fineIds[oscillator], "FINE", primary);
        oscPhaseAttachment[oscillator] = bind (oscPhase[oscillator], phaseIds[oscillator], "PHASE", primary);
        oscPanAttachment[oscillator] = bind (oscPan[oscillator], panIds[oscillator], "PAN", primary);
        oscPulseWidthAttachment[oscillator] = bind (oscPulseWidth[oscillator], pulseWidthIds[oscillator], "PULSE WIDTH", primary);
    }
    // Short engraved labels keep all six filter/output knobs legible at the
    // 1200 px minimum width; the full names stay on the parameters themselves.
    noiseMixAttachment=bind(noiseMix,ParamIDs::noiseMix,"NOISE"); amDepthAttachment=bind(amDepth,ParamIDs::amDepth,"AM DEPTH"); unisonVoicesAttachment=bind(unisonVoices,ParamIDs::unisonVoices,"UNISON"); unisonDetuneAttachment=bind(unisonDetune,ParamIDs::unisonDetune,"DETUNE"); unisonSpreadAttachment=bind(unisonSpread,ParamIDs::unisonSpread,"SPREAD"); unisonPhaseAttachment=bind(unisonPhase,ParamIDs::unisonPhase,"UNI PHASE"); unisonDriftAttachment=bind(unisonDrift,ParamIDs::unisonDrift,"DRIFT"); voiceVarianceAttachment=bind(voiceVariance,ParamIDs::voiceVariance,"VOICE VAR"); attackAttachment=bind(attack,ParamIDs::attack,"ATTACK"); decayAttachment=bind(decay,ParamIDs::decay,"DECAY"); sustainAttachment=bind(sustain,ParamIDs::sustain,"SUSTAIN"); releaseAttachment=bind(release,ParamIDs::release,"RELEASE"); envCurveAttachment=bind(envCurve,ParamIDs::envCurve,"ENV CRV"); cutoffAttachment=bind(cutoff,ParamIDs::cutoff,"CUTOFF",EonAnalogLookAndFeel::compact); resonanceAttachment=bind(resonance,ParamIDs::resonance,"RESO",EonAnalogLookAndFeel::compact); filterDriveAttachment=bind(filterDrive,ParamIDs::filterDrive,"FLT DRV",EonAnalogLookAndFeel::compact); gainAttachment=bind(gain,ParamIDs::gain,"OUTPUT",EonAnalogLookAndFeel::compact); driveAttachment=bind(drive,ParamIDs::drive,"DRIVE",EonAnalogLookAndFeel::compact); ampSatAttachment=bind(ampSaturation,ParamIDs::ampSat,"SAT",EonAnalogLookAndFeel::compact);
    constexpr auto primary = EonAnalogLookAndFeel::primary;
    lfoRateAttachment=bind(lfoRate,ParamIDs::lfoRate,"LFO RATE",primary); lfoDepthAttachment=bind(lfoDepth,ParamIDs::lfoDepth,"LFO CUTOFF",primary); lfoPitchAttachment=bind(lfoPitch,ParamIDs::lfoPitch,"LFO PITCH",primary); velocityAttachment=bind(velocityAmount,ParamIDs::velocityAmount,"VELOCITY",primary);
    for (int slot = 0; slot < 4; ++slot)
    {
        modAmountAttachment[static_cast<size_t> (slot)] = bind (
            modAmount[static_cast<size_t> (slot)], ParamIDs::modAmount (slot),
            "AMT " + juce::String (slot + 1), EonAnalogLookAndFeel::compact);
        modAmount[static_cast<size_t> (slot)].getProperties().set ("eon.raised", true);
    }
    filterAttackAttachment=bind(filterAttack,ParamIDs::filterAttack,"F ATK"); filterDecayAttachment=bind(filterDecay,ParamIDs::filterDecay,"F DEC"); filterSustainAttachment=bind(filterSustain,ParamIDs::filterSustain,"F SUS"); filterReleaseAttachment=bind(filterRelease,ParamIDs::filterRelease,"F REL"); filterEnvAmountAttachment=bind(filterEnvAmount,ParamIDs::filterEnvAmount,"F AMT");
    fxWetAttachment=bind(fxWet,ParamIDs::fxWet,"FX WET"); delayTimeAttachment=bind(delayTime,ParamIDs::delayTime,"DLY TIME"); delayFeedbackAttachment=bind(delayFeedback,ParamIDs::delayFeedback,"DLY FDBK"); delayStereoAttachment=bind(delayStereo,ParamIDs::delayStereo,"DLY WIDE"); chorusDepthAttachment=bind(chorusDepth,ParamIDs::chorusDepth,"CHO DEPTH"); chorusRateAttachment=bind(chorusRate,ParamIDs::chorusRate,"CHO RATE"); chorusMixAttachment=bind(chorusMix,ParamIDs::chorusMix,"CHO MIX"); reverbMixAttachment=bind(reverbMix,ParamIDs::reverbMix,"REVERB"); reverbModulationAttachment=bind(reverbModulation,ParamIDs::reverbModulation,"RVB MOD");
    // Keep every physical control visible, but never shrink the panel into the
    // former overlapping layout. Hosts can still scale beyond this minimum.
    setResizable (true, true);
    setResizeLimits (minimumEditorWidth, minimumEditorHeight, 1600, 1000);
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
void EonMiniEEFEditor::knob(juce::Slider& s, const juce::String& name,
                            EonAnalogLookAndFeel::DialGrade grade)
{
    s.setName (name);
    // Declared here rather than inferred by the renderer, so the layout can
    // size each row from the same constant the renderer draws with.
    s.getProperties().set ("eon.dialGrade", (int) grade);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    // The custom renderer engraves the parameter name at rest and swaps it
    // for the exact value only while a deliberate gesture is in progress.
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.onDragStart = [this, &s] { s.getProperties().set ("eon.dragging", true); knobCaptionRefresh.trigger (s); };
    s.onDragEnd = [this, &s] { s.getProperties().set ("eon.dragging", false); knobCaptionRefresh.trigger (s); };
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

    // Analogue chassis: a walnut-cheeked cabinet with a sanded aluminium
    // centre panel.  The wood is cached into an image on resize rather than
    // re-stroked every frame: the grain needs hundreds of thin lines to read
    // as wood at all, and repainting that on every timer tick would cost more
    // than the rest of the panel put together.
    if (woodCache.getWidth() != w || woodCache.getHeight() != h)
    {
        woodCache = juce::Image (juce::Image::ARGB, w, h, true);
        juce::Graphics wood (woodCache);
        wood.fillAll (bg);
        juce::ColourGradient cheek (juce::Colour (0xFF6B5138), 0.0f, 0.0f,
                                    walnut, 0.0f, (float) h, false);
        wood.setGradientFill (cheek);
        wood.fillRect (0, 0, w, h);

        // Cathedral grain: nested arcs whose centre wanders down the board,
        // which is what gives flat-sawn walnut its arch pattern.
        wood.setColour (juce::Colours::black.withAlpha (0.10f));
        for (int arc = 0; arc < 26; ++arc)
        {
            const float t = (float) arc / 26.0f;
            const float centreY = (float) h * (0.08f + 0.92f * t);
            const float centreX = (float) w * (0.5f + 0.22f * std::sin (t * 5.1f));
            const float radius = (float) w * (0.16f + 0.40f * t);
            juce::Path band;
            band.addCentredArc (centreX, centreY, radius, radius * 2.4f,
                                0.0f, 1.05f, 2.10f, true);
            wood.strokePath (band, juce::PathStrokeType (0.7f + 0.5f * t,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }
        // Fine pore lines running with the grain.
        for (int line = 0; line < 46; ++line)
        {
            const float t = (float) line / 46.0f;
            const float y = (float) h * (0.02f + 0.96f * t);
            const float wobble = 5.0f * std::sin (t * 11.0f) + 2.5f * std::sin (t * 27.0f);
            wood.setColour (juce::Colours::black.withAlpha (0.030f + 0.030f
                                      * (0.5f + 0.5f * std::sin (t * 19.0f))));
            wood.drawLine (0.0f, y + wobble, (float) w, y - wobble, 0.8f);
            wood.setColour (juce::Colour (0xFFB08B5E).withAlpha (0.020f));
            wood.drawLine (0.0f, y + wobble + 1.0f, (float) w, y - wobble + 1.0f, 0.6f);
        }
        // Speckle: the open pores that make a satin lacquer look like wood.
        for (int speck = 0; speck < 320; ++speck)
        {
            const float x = std::fmod ((float) speck * 97.13f, (float) w);
            const float y = std::fmod ((float) speck * 61.77f, (float) h);
            wood.setColour (juce::Colours::black.withAlpha (0.045f));
            wood.fillEllipse (x, y, 1.6f, 0.9f);
        }
        // One consistent light source: a soft sheen falling from the upper left.
        juce::ColourGradient sheen (juce::Colour (0x18FFE0B0), 0.0f, 0.0f,
                                    juce::Colours::transparentBlack, (float) w * 0.7f,
                                    (float) h, false);
        wood.setGradientFill (sheen);
        wood.fillRect (0, 0, w, h);
    }
    g.drawImageAt (woodCache, 0, 0);

    const auto header = juce::Rectangle<float> (12.0f, (float) headerY, (float) w - 24.0f, (float) headerHeight);
    g.setColour (juce::Colours::black.withAlpha (0.58f));
    g.fillRoundedRectangle (header.translated (0.0f, 0.8f), 6.0f);
    juce::ColourGradient headerMetal (juce::Colour (0xff5f4a36), header.getCentreX(), header.getY(),
                                      juce::Colour (0xFF1A140E), header.getCentreX(), header.getBottom(), false);
    g.setGradientFill (headerMetal);
    g.fillRoundedRectangle (header, 6.0f);
    g.setColour (brass.withAlpha (0.70f));
    g.drawRoundedRectangle (header, 6.0f, 1.0f);
    g.setColour (juce::Colour (0xff16110c).withAlpha (0.88f));
    g.drawRoundedRectangle (header.reduced (1.6f), 5.0f, 0.65f);
    juce::ColourGradient topRail (mintGlow, 0.0f, 0.0f,
                                  mintDeep, (float) w, 0.0f, false);
    g.setGradientFill (topRail);
    g.fillRect (0, 0, w, 4);

    const auto headerLayout = makeHeaderLayout (w);
    auto headerCaption = [&] (const juce::String& text, int x, int width, int y)
    {
        g.setColour (cream.withAlpha (0.88f));
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
        juce::ColourGradient bezel (brass.brighter (0.18f), outer.getCentreX(), outer.getY(),
                                    juce::Colour (0xff1b1510), outer.getCentreX(), outer.getBottom(), false);
        g.setGradientFill (bezel);
        g.fillRoundedRectangle (outer, 8.0f);
        juce::ColourGradient module (juce::Colour (0xFF7C6F5E), inner.getCentreX(), inner.getY(),
                                     juce::Colour (0xFF52483C), inner.getCentreX(), inner.getBottom(), false);
        g.setGradientFill (module);
        g.fillRoundedRectangle (inner, 6.2f);
        // Fine horizontal brushing, clipped to the module: sanded aluminium
        // panels scatter light along the grain rather than across it.
        g.saveState();
        juce::Path moduleClip;
        moduleClip.addRoundedRectangle (inner, 6.2f);
        g.reduceClipRegion (moduleClip.getBounds().toNearestInt());
        for (int brushY = (int) inner.getY(); brushY < inner.getBottom(); brushY += 3)
        {
            const float phase = 0.5f + 0.5f * std::sin ((float) brushY * 1.7f);
            g.setColour (juce::Colours::white.withAlpha (0.012f + 0.012f * phase));
            g.fillRect ((int) inner.getX(), brushY, (int) inner.getWidth(), 1);
        }
        g.restoreState();
        g.setColour (juce::Colours::black.withAlpha (0.70f));
        g.drawRoundedRectangle (inner, 6.2f, 0.7f);
        g.setColour (accent.withAlpha (0.62f));
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) r.getX() + 8.0f,
                                                         (float) r.getY() + 7.0f,
                                                         (float) r.getWidth() - 16.0f, 2.5f), 1.2f);
        g.setColour (accent);
        g.setFont (EonAnalogLookAndFeel::panelFont (14.8f, true, 0.93f));
        g.drawText (title, r.getX() + 13, r.getY() + 11, r.getWidth() - 26, 19, juce::Justification::left);
        g.setColour (cream.withAlpha (0.34f));
        g.fillRect (r.getX() + 12, r.getY() + 38, r.getWidth() - 24, 1);
        for (const auto corner : { juce::Point<int> (r.getX() + 9, r.getY() + 9),
                                   juce::Point<int> (r.getRight() - 9, r.getY() + 9),
                                   juce::Point<int> (r.getX() + 9, r.getBottom() - 9),
                                   juce::Point<int> (r.getRight() - 9, r.getBottom() - 9) })
        {
            g.setColour (juce::Colours::black.withAlpha (0.80f));
            g.fillEllipse ((float) corner.x - 2.4f, (float) corner.y - 1.7f, 4.8f, 4.8f);
            g.setColour (brass.darker (0.10f));
            g.fillEllipse ((float) corner.x - 2.1f, (float) corner.y - 2.2f, 4.2f, 4.2f);
            g.setColour (bg);
            g.fillEllipse ((float) corner.x - 1.4f, (float) corner.y - 1.5f, 2.8f, 2.8f);
            g.setColour (cream.withAlpha (0.46f));
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
    // The FX strip takes whatever height is left between the lower modules and
    // the bottom margin, so the cabinet reads as one filled face at any window
    // height instead of leaving a band of bare wood in the middle.
    const int lowerBlockBottom = contentTop + oscillatorHeight + moduleGap
                               + lowerSectionHeight + moduleGap;
    const int fxHeight = juce::jlimit (fxMinimumHeight, fxMaximumHeight,
                                      getHeight() - bottomMargin - lowerBlockBottom);
    const int fxY = getHeight() - bottomMargin - fxHeight;
    const int lowerY = top + topH + gap;
    const int lowerH = lowerSectionHeight;
    const int envW = (w - 2 * m - 2 * gap) * 32 / 100;
    const int filterW = (w - 2 * m - 2 * gap) * 30 / 100;
    box ({ m, lowerY, envW, lowerH }, "AMP ENVELOPE  |  FILTER ENVELOPE");
    box ({ m + envW + gap, lowerY, filterW, lowerH }, "FILTER  |  OUTPUT");
    box ({ m + envW + filterW + 2 * gap, lowerY, w - m - (m + envW + filterW + 2 * gap), lowerH }, "MODULATION  |  MOTION", mintGlow);
    box ({ m, fxY, w - 2 * m, fxHeight }, "GLOBAL FX  |  DELAY  |  CHORUS  |  REVERB", mintGlow);
    g.setColour (ivory.withAlpha (0.76f));
    g.setFont (EonAnalogLookAndFeel::panelFont (12.5f, true, 0.93f));
    g.drawText ("VCO A + VCO B  ->  FILTER  ->  AMP  ->  MOD  ->  FX", m + 10, fxY - 16, 520, 14, juce::Justification::left);
}
void EonMiniEEFEditor::resized()
{
    // The fixed block heights below need 800 px; below that they overlap by
    // construction, so a window the host forced smaller is grown back to the
    // documented minimum instead of being laid out on top of itself.  A host
    // that cannot give 800 px gets a tall editor rather than an unusable one.
    if (getHeight() < minimumEditorHeight || getWidth() < minimumEditorWidth)
    {
        setSize (juce::jmax (minimumEditorWidth, getWidth()),
                 juce::jmax (minimumEditorHeight, getHeight()));
        return;
    }

    const int w = getWidth(), h = getHeight(), m = contentMargin, gap = moduleGap, top = contentTop, topH = oscillatorHeight;
    const auto headerLayout = makeHeaderLayout (w);
    oversamplingMode.setBounds (headerLayout.qualityX, 27, headerLayout.qualityWidth, 28);
    presetMenu.setBounds (headerLayout.patchX, 27, headerLayout.patchWidth, 28);
    initButton.setBounds (headerLayout.actionsX, 27, headerLayout.actionWidth, 28);
    saveButton.setBounds (headerLayout.actionsX + headerLayout.actionWidth + 6, 27, headerLayout.actionWidth, 28);
    loadButton.setBounds (headerLayout.actionsX + 2 * (headerLayout.actionWidth + 6), 27, headerLayout.actionWidth, 28);
    const int cardW = (w - 2 * m - gap) / 2;
    const int cardH = (oscillatorCardsHeight - gap) / 2;
    const int primaryH = EonAnalogLookAndFeel::dialRowHeight (EonAnalogLookAndFeel::primary);
    const int standardH = EonAnalogLookAndFeel::dialRowHeight (EonAnalogLookAndFeel::standard);
    const int compactH = EonAnalogLookAndFeel::dialRowHeight (EonAnalogLookAndFeel::compact);
    std::array<juce::ComboBox*, 4> waveBoxes { &osc1Wave, &osc2Wave, &osc3Wave, &osc4Wave };
    for (int oscillator = 0; oscillator < 4; ++oscillator)
    {
        const int column = oscillator % 2;
        const int row = oscillator / 2;
        const int cardX = m + column * (cardW + gap);
        const int cardY = top + row * (cardH + gap);
        waveBoxes[static_cast<size_t> (oscillator)]->setBounds (cardX + 12, cardY + 42, 104, 26);
        constexpr int controlGap = 3;
        const int controlsX = cardX + 124;
        const int controlW = (cardW - 124 - 8 - 5 * controlGap) / 6;
        std::array<juce::Slider*, 6> controls {
            &oscLevel[oscillator], &oscCoarse[oscillator], &oscFine[oscillator],
            &oscPhase[oscillator], &oscPan[oscillator], &oscPulseWidth[oscillator]
        };
        for (int control = 0; control < 6; ++control)
            controls[static_cast<size_t> (control)]->setBounds (
                controlsX + control * (controlW + controlGap), cardY + 40,
                controlW, juce::jmin (primaryH, cardH - 44));
    }
    const int stripY = top + oscillatorCardsHeight + gap;
    voiceMode.setBounds (m + 14, stripY + 52, 142, 26);
    const int stripKnobX = m + 170;
    const int stripKnobW = (w - stripKnobX - m - 12) / 8;
    std::array<juce::Slider*, 8> stripControls {
        &unisonVoices, &unisonDetune, &unisonSpread, &unisonPhase, &unisonDrift,
        &voiceVariance, &noiseMix, &amDepth
    };
    // Eight standard dials sit under the title rule.  The row height is the
    // grade, not whatever is left after the title.
    const int stripKnobY = stripY + 40;
    const int stripKnobH = juce::jmin (standardH, oscillatorStripHeight - 44);
    for (int control = 0; control < static_cast<int> (stripControls.size()); ++control)
        stripControls[static_cast<size_t> (control)]->setBounds (
            stripKnobX + control * stripKnobW, stripKnobY, stripKnobW - 4, stripKnobH);
    const int lowerY = top + topH + gap;
    const int lowerH = lowerSectionHeight;
    const int lowerBlockBottom = lowerY + lowerH + gap;
    const int fxHeight = juce::jlimit (fxMinimumHeight, fxMaximumHeight,
                                      h - bottomMargin - lowerBlockBottom);
    const int fxY = h - bottomMargin - fxHeight;
    const int envW = (w - 2 * m - 2 * gap) * 32 / 100, filterW = (w - 2 * m - 2 * gap) * 30 / 100;
    int x = m + 18;
    // Keep four envelope controls inside the panel at the 1200 px minimum
    // while allowing them to grow on wider layouts.
    // Two envelope rows share the panel: the amp envelope on top, the
    // dedicated filter envelope underneath.
    const int envRowH = juce::jmin (standardH, juce::jmax (standardH, (lowerH - 48) / 2));
    const int knobW = juce::jlimit (48, 118, (envW - 80) / 5);
    for (auto* s : { &attack, &decay, &sustain, &release, &envCurve })
    {
        s->setBounds (x, lowerY + 42, knobW, envRowH);
        x += knobW + 8;
    }
    x = m + 18;
    for (auto* s : { &filterAttack, &filterDecay, &filterSustain, &filterRelease, &filterEnvAmount })
    {
        s->setBounds (x, lowerY + 42 + envRowH + 4, knobW, envRowH);
        x += knobW + 8;
    }
    x = m + envW + gap + 18;
    const int filterInnerW = juce::jmax (150, filterW - 36);
    const int driveCurveWidth = 74;
    driveCurveMode.setBounds (x + filterInnerW - driveCurveWidth, lowerY + 8, driveCurveWidth, 24);
    filterMode.setBounds (x, lowerY + 44, filterInnerW - driveCurveWidth - 8, 26);
    // Six compact knobs keep the filter drive inside the filter cluster while
    // staying within the panel down to the 1200 px minimum window width.
    // The column is wider than the compact dial so the engraved caption keeps a
    // readable line: "CUTOFF" and "OUTPUT" do not fit in a 40 px column.
    const int filterSmallW = juce::jlimit (44, 72, (filterInnerW - 30) / 6);
    const int filterKnobH = juce::jmin (compactH, lowerH - 86);
    cutoff.setBounds (x, lowerY + 78, filterSmallW, filterKnobH);
    resonance.setBounds (x + filterSmallW + 6, lowerY + 78, filterSmallW, filterKnobH);
    filterDrive.setBounds (x + 2 * (filterSmallW + 6), lowerY + 78, filterSmallW, filterKnobH);
    gain.setBounds (x + 3 * (filterSmallW + 6), lowerY + 78, filterSmallW, filterKnobH);
    drive.setBounds (x + 4 * (filterSmallW + 6), lowerY + 78, filterSmallW, filterKnobH);
    ampSaturation.setBounds (x + 5 * (filterSmallW + 6), lowerY + 78, filterSmallW, filterKnobH);
    const int modX = m + envW + filterW + 2 * gap + 18;
    const int modW = w - modX - m - 12;
    x = modX; const int modKnobW = juce::jmax (52, (modW - 24) / 4);
    const int modKnobH = juce::jmin (primaryH, lowerH - 54);
    for (auto* s : { &lfoRate, &lfoDepth, &lfoPitch, &velocityAmount }) { s->setBounds (x, lowerY + 45, modKnobW, modKnobH); x += modKnobW + 8; }
    const int modAmountY = lowerY + 45 + modKnobH + 2;
    const int modAmountH = juce::jmin (compactH, lowerH - (modAmountY - lowerY) - 4);
    x = modX;
    for (auto& amount : modAmount)
    {
        amount.setBounds (x, modAmountY, modKnobW, modAmountH);
        x += modKnobW + 8;
    }
    // The shape selector belongs in the panel's empty title row, to the right
    // of the caption.  At the panel's left corner it covered the caption text
    // and made the title unreadable.
    lfoShapeMode.setBounds (modX + modW - 78, lowerY + 8, 78, 24);
    // Nine knobs share the FX strip: the two P3 controls (delay width and
    // reverb modulation) join the established seven.  Leaving the loop at
    // seven rendered the two bound but invisible, the same failure the
    // oscillator strip had before its width was corrected.
    const int fxW = (w - 2 * m - 60) / 9; x = m + 12;
    for (auto* s : { &fxWet, &delayTime, &delayFeedback, &delayStereo,
                      &chorusDepth, &chorusRate, &chorusMix,
                      &reverbMix, &reverbModulation })
    { s->setBounds (x, fxY + 40, fxW, juce::jmin (standardH, fxHeight - 44)); x += fxW + 6; }
}
