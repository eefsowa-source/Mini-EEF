#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// A hardware-style control layer: compact engraved typography, a powder-coated
// chassis and physically layered knobs make fast sound shaping possible
// without copying a particular commercial synth's artwork or layout.
class EonAnalogLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    EonAnalogLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe1faff));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff07191f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3b8999));
    }

    static juce::String interfaceFontFamily()
    {
#if JUCE_MAC
        return "Avenir Next";
#elif JUCE_WINDOWS
        return "Segoe UI";
#else
        return juce::Font::getDefaultSansSerifFontName();
#endif
    }

    static juce::Font panelFont (float height, bool emphatic = false, float horizontalScale = 0.98f)
    {
        // A humanist sans-serif keeps the hardware hierarchy but feels calmer
        // and more refined than the previous technical monospaced face.
        return juce::Font (juce::FontOptions (interfaceFontFamily(),
                                               height,
                                               emphatic ? juce::Font::bold : juce::Font::plain)
                                   .withHorizontalScale (juce::jlimit (0.97f, 1.02f, horizontalScale))
                                   .withKerningFactor (emphatic ? 0.055f : 0.025f));
    }

    static juce::Font displayFont (float height)
    {
        return juce::Font (juce::FontOptions (interfaceFontFamily(), height, juce::Font::bold)
                                   .withHorizontalScale (0.98f)
                                   .withKerningFactor (0.085f));
    }

    static juce::Font readoutFont (float height)
    {
        return juce::Font (juce::FontOptions (interfaceFontFamily(),
                                               height, juce::Font::bold)
                                   .withHorizontalScale (0.98f)
                                   .withKerningFactor (0.035f));
    }

    static juce::Font signatureFont (float height)
    {
       #if JUCE_MAC
        constexpr auto family = "Snell Roundhand";
       #elif JUCE_WINDOWS
        constexpr auto family = "Segoe Script";
       #else
        constexpr auto family = "URW Chancery L";
       #endif
        return juce::Font (juce::FontOptions (family, height, juce::Font::italic)
                                   .withHorizontalScale (1.04f)
                                   .withKerningFactor (0.045f));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return panelFont (juce::jlimit (12.5f, 17.0f, (float) buttonHeight * 0.50f), true, 0.93f);
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return panelFont (juce::jlimit (13.0f, 17.5f, (float) box.getHeight() * 0.55f), true, 0.93f);
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        return panelFont (juce::jlimit (12.0f, 16.5f, (float) label.getHeight() * 0.72f), true, 0.93f);
    }

    juce::Font getPopupMenuFont() override { return panelFont (16.5f, true, 0.92f); }
    juce::Font getSliderPopupFont (juce::Slider&) override { return readoutFont (15.5f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const float press = down ? 1.0f : 0.0f;
        const auto face = backgroundColour.interpolatedWith (juce::Colour (0xff225462), 0.24f);

        g.setColour (juce::Colour (0x68000000));
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.8f), 4.0f);

        juce::ColourGradient metal (face.brighter (highlighted ? 0.16f : 0.06f),
                                    bounds.getCentreX(), bounds.getY(),
                                    face.darker (down ? 0.10f : 0.32f),
                                    bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (metal);
        g.fillRoundedRectangle (bounds.translated (0.0f, press), 4.0f);
        g.setColour (juce::Colour (0xff6dd2df).withAlpha (highlighted ? 0.82f : 0.55f));
        g.drawRoundedRectangle (bounds.translated (0.0f, press), 4.0f, 0.9f);
        g.setColour (juce::Colours::black.withAlpha (0.60f));
        g.drawRoundedRectangle (bounds.reduced (1.3f).translated (0.0f, press), 3.1f, 0.65f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
        const auto face = juce::Colour (0xff183f49);
        g.setColour (juce::Colour (0x5c000000));
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.4f), 3.5f);
        juce::ColourGradient metal (face.brighter (down ? 0.02f : 0.09f), bounds.getCentreX(), bounds.getY(),
                                    face.darker (0.28f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (metal);
        g.fillRoundedRectangle (bounds, 3.5f);
        g.setColour (juce::Colour (0xff79d0dd).withAlpha (0.78f));
        g.drawRoundedRectangle (bounds, 3.5f, 0.85f);
        g.setColour (juce::Colour (0xff090a0b).withAlpha (0.85f));
        g.drawRoundedRectangle (bounds.reduced (1.4f), 2.5f, 0.6f);

        const auto arrowArea = juce::Rectangle<float> ((float) buttonX, (float) buttonY,
                                                        (float) buttonW, (float) buttonH).reduced (6.0f, 0.0f);
        const float cx = arrowArea.getCentreX();
        const float cy = arrowArea.getCentreY() + 1.0f;
        juce::Path arrow;
        arrow.startNewSubPath (cx - 5.0f, cy - 2.0f);
        arrow.lineTo (cx, cy + 3.0f);
        arrow.lineTo (cx + 5.0f, cy - 2.0f);
        g.setColour (juce::Colour (0xffedfff9).withAlpha (down ? 1.0f : 0.92f));
        g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        constexpr float captionHeight = 17.0f;
        const float dialHeight = juce::jmax (20.0f, (float) height - captionHeight - 2.0f);
        const float diameter = juce::jmax (18.0f, juce::jmin ((float) width - 4.0f, dialHeight) - 4.0f);
        const float cx = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
        const float cy = static_cast<float> (y) + dialHeight * 0.5f + 1.5f;
        const float radius = diameter * 0.5f;
        const auto centre = juce::Point<float> (cx, cy);
        const auto pointOnRing = [&] (float angle, float distance)
        {
            return centre + juce::Point<float> (std::cos (angle - juce::MathConstants<float>::halfPi),
                                                 std::sin (angle - juce::MathConstants<float>::halfPi)) * distance;
        };

        const auto outer = juce::Rectangle<float> (centre.x - radius, centre.y - radius, diameter, diameter);
        const auto rim = outer.reduced (2.0f);
        const auto face = outer.reduced (5.2f);

        // Multiple physically distinct layers make the cap feel machined
        // rather than painted: cast shadow, metal collar, black recess,
        // knurled sidewall, curved gloss and a recessed indicator groove.
        g.setColour (juce::Colour (0x2d000000));
        g.fillEllipse (outer.translated (0.0f, 6.3f));
        g.setColour (juce::Colour (0x78000000));
        g.fillEllipse (outer.translated (0.0f, 3.5f));
        juce::ColourGradient bezel (juce::Colour (0xffa3e3eb), outer.getCentreX(), outer.getY(),
                                    juce::Colour (0xff07232a), outer.getCentreX(), outer.getBottom(), false);
        g.setGradientFill (bezel);
        g.fillEllipse (outer);
        g.setColour (juce::Colour (0xff02110f));
        g.fillEllipse (rim);
        juce::ColourGradient cap (juce::Colour (0xff4d929f), face.getCentreX(), face.getY(),
                                  juce::Colour (0xff0a252d), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill (cap);
        g.fillEllipse (face);
        const auto capInset = face.reduced (2.0f);
        juce::ColourGradient capCore (juce::Colour (0xff2d6875), capInset.getCentreX(), capInset.getY(),
                                      juce::Colour (0xff0a1d24), capInset.getCentreX(), capInset.getBottom(), false);
        g.setGradientFill (capCore);
        g.fillEllipse (capInset);
        g.setColour (juce::Colour (0xffc5fff0).withAlpha (0.42f));
        g.drawEllipse (outer.reduced (0.8f), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.82f));
        g.drawEllipse (outer.reduced (2.1f), 1.25f);
        g.setColour (juce::Colour (0xffc8fff1).withAlpha (0.16f));
        g.drawEllipse (capInset.reduced (0.7f), 0.75f);

        if (radius >= 22.0f)
        {
            const int ridges = juce::jlimit (18, 38, (int) std::round (radius * 1.35f));
            for (int ridge = 0; ridge < ridges; ++ridge)
            {
                const float angle = juce::MathConstants<float>::twoPi * (float) ridge / (float) ridges;
                const auto a = pointOnRing (angle, radius - 3.5f);
                const auto b = pointOnRing (angle, radius - 0.6f);
                g.setColour (juce::Colours::black.withAlpha (0.62f));
                g.drawLine (a.x + 0.45f, a.y + 0.9f, b.x + 0.45f, b.y + 0.9f, 1.15f);
                g.setColour (ridge % 2 == 0 ? juce::Colour (0xffc3fff0).withAlpha (0.22f)
                                             : juce::Colour (0xff287166).withAlpha (0.38f));
                g.drawLine (a.x, a.y, b.x, b.y, 0.72f);
            }
        }

        juce::Path gloss;
        gloss.addCentredArc (cx, cy, radius * 0.72f, radius * 0.72f, 0.0f, -2.52f, -0.58f, true);
        g.setColour (juce::Colour (0xffe2fff6).withAlpha (0.34f));
        g.strokePath (gloss, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        for (int tick = 0; tick <= 10; ++tick)
        {
            const float t = static_cast<float> (tick) / 10.0f;
            const float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            const bool major = tick % 2 == 0;
            const auto tickOuter = pointOnRing (angle, radius + 2.8f);
            const auto tickInner = pointOnRing (angle, radius + (major ? -1.4f : 0.4f));
            g.setColour (juce::Colour (0xffbfffee).withAlpha (major ? 0.74f : 0.42f));
            g.drawLine (tickOuter.x, tickOuter.y, tickInner.x, tickInner.y, major ? 1.1f : 0.7f);
        }

        const float valueAngle = rotaryStartAngle
                               + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path arc;
        arc.addCentredArc (cx, cy, radius + 1.0f, radius + 1.0f,
                           0.0f, rotaryStartAngle, valueAngle, true);
        g.setColour (juce::Colour (0xff35e1e5));
        g.strokePath (arc, juce::PathStrokeType (1.9f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        const float pointerLength = radius * 0.66f;
        const auto pointer = pointOnRing (valueAngle, pointerLength);
        g.setColour (juce::Colours::black.withAlpha (0.84f));
        g.drawLine (centre.x + 1.0f, centre.y + 1.6f, pointer.x + 1.0f, pointer.y + 1.6f, 4.4f);
        g.setColour (juce::Colour (0xff175b6b));
        g.drawLine (centre.x, centre.y + 0.5f, pointer.x, pointer.y + 0.5f, 3.15f);
        g.setColour (juce::Colour (0xffedfff9));
        g.drawLine (centre.x - 0.3f, centre.y - 0.3f, pointer.x - 0.3f, pointer.y - 0.3f, 1.55f);

        const float hubDiameter = juce::jlimit (7.0f, 15.0f, radius * 0.27f);
        const auto hub = juce::Rectangle<float> (centre.x - hubDiameter * 0.5f, centre.y - hubDiameter * 0.5f,
                                                  hubDiameter, hubDiameter);
        g.setColour (juce::Colours::black.withAlpha (0.70f));
        g.fillEllipse (hub.translated (0.0f, 1.0f));
        juce::ColourGradient hubGradient (juce::Colour (0xff98e1e9), hub.getCentreX(), hub.getY(),
                                          juce::Colour (0xff0a232a), hub.getCentreX(), hub.getBottom(), false);
        g.setGradientFill (hubGradient);
        g.fillEllipse (hub);
        g.setColour (juce::Colour (0xffd8fff5).withAlpha (0.52f));
        g.drawEllipse (hub.reduced (0.55f), 0.65f);
        g.setColour (juce::Colours::black.withAlpha (0.36f));
        g.fillEllipse (hub.reduced (hubDiameter * 0.31f));
        g.setColour (juce::Colour (0xffd8fff5).withAlpha (0.32f));
        g.fillEllipse (hub.reduced (hubDiameter * 0.39f).translated (-0.45f, -0.45f));

        const auto caption = juce::Rectangle<int> (x + 1, y + height - 17, width - 2, 16);
        const bool isAdjusting = slider.getProperties().getWithDefault ("eon.dragging", false);
        const auto text = isAdjusting ? slider.getTextFromValue (slider.getValue()).toUpperCase()
                                      : slider.getName();
        g.setFont (isAdjusting ? readoutFont (12.3f) : panelFont (12.0f, true, 0.93f));
        g.setColour (isAdjusting ? juce::Colour (0xffa6f7ff) : juce::Colour (0xffeffcff).withAlpha (0.96f));
        g.drawFittedText (text, caption, juce::Justification::centred, 1, 0.90f);
    }
};

class EonMiniEEFEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EonMiniEEFEditor(EonMiniEEFProcessor&);
    ~EonMiniEEFEditor() override { setLookAndFeel (nullptr); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
private:
    EonAnalogLookAndFeel analogLookAndFeel;
    bool windowSharingConfigured = false;
    void knob(juce::Slider&, const juce::String&);
    void applyPreset (int index);
    void savePreset();
    void loadPreset();
    EonMiniEEFProcessor& processor;
    juce::ComboBox osc1Wave, osc2Wave, osc3Wave, osc4Wave, voiceMode, filterMode, driveCurveMode;
    juce::ComboBox oversamplingMode;
    juce::ComboBox presetMenu;
    juce::TextButton initButton { "INIT" }, saveButton { "SAVE" }, loadButton { "LOAD" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::array<juce::Slider, 4> oscLevel, oscCoarse, oscFine, oscPhase, oscPan, oscPulseWidth;
    juce::Slider noiseMix, amDepth, unisonVoices, unisonDetune, unisonSpread, unisonPhase, attack, decay, sustain, release, cutoff, resonance, gain, drive, ampSaturation;
    juce::Slider lfoRate, lfoDepth, lfoPitch, velocityAmount;
    juce::Slider fxWet, delayTime, delayFeedback, chorusDepth, chorusRate, chorusMix, reverbMix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osc1Attachment, osc2Attachment, osc3Attachment, osc4Attachment, modeAttachment, filterModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> driveCurveAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> oscLevelAttachment, oscCoarseAttachment, oscFineAttachment, oscPhaseAttachment, oscPanAttachment, oscPulseWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseMixAttachment, amDepthAttachment, unisonVoicesAttachment, unisonDetuneAttachment, unisonSpreadAttachment, unisonPhaseAttachment, attackAttachment, decayAttachment, sustainAttachment, releaseAttachment, cutoffAttachment, resonanceAttachment, gainAttachment, driveAttachment, ampSatAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment, lfoDepthAttachment, lfoPitchAttachment, velocityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fxWetAttachment, delayTimeAttachment, delayFeedbackAttachment, chorusDepthAttachment, chorusRateAttachment, chorusMixAttachment, reverbMixAttachment;
    float meterLeft = 0.0f, meterRight = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EonMiniEEFEditor)
};
