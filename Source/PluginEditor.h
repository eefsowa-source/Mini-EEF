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
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xFFECE0C8));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xFF1B1611));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xFF8A7550));
    }

    // Dial sizing is declared, not inferred.  The layout and the renderer both
    // read these three diameters, so a row can be resized without silently
    // changing how big its knobs look.  See
    // docs/superpowers/specs/2026-09-30-dial-grade-adr.md for the reasoning.
    enum DialGrade { compact = 0, standard = 1, primary = 2 };

    static constexpr float dialDiameter (int grade) noexcept
    {
        return grade == compact ? 40.0f : grade == primary ? 62.0f : 50.0f;
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
        const auto face = backgroundColour.interpolatedWith (juce::Colour (0xff2a1f16), 0.24f);

        g.setColour (juce::Colour (0x68000000));
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.8f), 4.0f);

        juce::ColourGradient metal (face.brighter (highlighted ? 0.20f : 0.08f),
                                    bounds.getCentreX(), bounds.getY(),
                                    face.darker (down ? 0.10f : 0.32f),
                                    bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (metal);
        g.fillRoundedRectangle (bounds.translated (0.0f, press), 4.0f);
        g.setColour (juce::Colour (0xffd9a441).withAlpha (highlighted ? 0.86f : 0.55f));
        g.drawRoundedRectangle (bounds.translated (0.0f, press), 4.0f, 0.9f);
        g.setColour (juce::Colours::black.withAlpha (0.60f));
        g.drawRoundedRectangle (bounds.reduced (1.3f).translated (0.0f, press), 3.1f, 0.65f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool down,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
        const auto face = juce::Colour (0xFF2F2A22);
        g.setColour (juce::Colour (0x5c000000));
        g.fillRoundedRectangle (bounds.translated (0.0f, 1.4f), 3.5f);
        juce::ColourGradient metal (face.brighter (down ? 0.02f : 0.09f), bounds.getCentreX(), bounds.getY(),
                                    face.darker (0.28f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (metal);
        g.fillRoundedRectangle (bounds, 3.5f);
        g.setColour (juce::Colour (0xFFC9A15C).withAlpha (0.78f));
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
        g.setColour (juce::Colour (0xFFF6E7C6).withAlpha (down ? 1.0f : 0.92f));
        g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        constexpr float captionHeight = 17.0f;
        const float dialHeight = juce::jmax (20.0f, (float) height - captionHeight - 2.0f);
        // Analogue panels are densely populated: a small dial with a printed
        // pointer reads as a control, while an oversized one reads as a gauge.
        // The 0.84 factor and the 72 px ceiling keep the whole set in that
        // range while preserving the ratio between the large and small dials.
        // The diameter comes from the slider's declared grade, not from its
        // bounds, so a row keeps the same knob size when the window resizes.
        // Bounds still have to be large enough to hold it; resized() sizes the
        // rows from the same constant.
        const int grade = slider.getProperties().getWithDefault ("eon.dialGrade",
                                                                (int) standard);
        const float declared = dialDiameter (grade);
        const float diameter = juce::jmax (14.0f, juce::jmin (declared,
            juce::jmin ((float) width - 2.0f, dialHeight)));
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
        const auto face = outer.reduced (4.6f);

        // A moulded knob is a cylinder, not a disc: draw the barrel wall below
        // the cap so the control reads as a part with height.  One light
        // direction (upper-left) is used for every element on the panel, so
        // the whole face looks lit by the same lamp instead of each part
        // carrying its own highlight.
        const float barrelHeight = juce::jmin (4.5f, radius * 0.16f);

        // Contact shadow on the panel, offset away from the light.
        g.setColour (juce::Colours::black.withAlpha (0.34f));
        g.fillEllipse (outer.reduced (1.0f).translated (radius * 0.10f, radius * 0.16f + barrelHeight));
        g.setColour (juce::Colours::black.withAlpha (0.20f));
        g.fillEllipse (outer.reduced (radius * 0.28f)
                                 .translated (radius * 0.13f, radius * 0.19f + barrelHeight));

        // Barrel wall: the skirt the cap is pressed into, lit from upper-left.
        const auto barrel = juce::Rectangle<float> (centre.x - radius, centre.y - radius + barrelHeight,
                                                     diameter, diameter);
        juce::ColourGradient wall (juce::Colour (0xFF3A342D), centre.x - radius, centre.y,
                                   juce::Colour (0xFF15120F), centre.x + radius, centre.y + diameter, false);
        g.setGradientFill (wall);
        g.fillEllipse (barrel);

        // Dark skirt under the cap.
        juce::ColourGradient skirt (juce::Colour (0xFF37312A), outer.getCentreX(), outer.getY(),
                                    juce::Colour (0xFF100E0C), outer.getCentreX(), outer.getBottom(), false);
        g.setGradientFill (skirt);
        g.fillEllipse (outer);
        g.setColour (juce::Colours::black.withAlpha (0.60f));
        g.drawEllipse (rim, 1.0f);
        // Occlusion where the cap overhangs the skirt: a dark crescent on the
        // shadow side only, which is what sells the cap as raised.
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        {
            juce::Path occlusion;
            const auto r = outer.reduced (0.6f);
            occlusion.addCentredArc (r.getCentreX(), r.getCentreY(), r.getWidth() * 0.5f,
                                      r.getHeight() * 0.5f, 0.0f,
                                      0.15f * juce::MathConstants<float>::pi,
                                      0.70f * juce::MathConstants<float>::pi, true);
            g.strokePath (occlusion, juce::PathStrokeType (1.6f));
        }

        // Knurling on the skirt: fine ridges, low contrast, purely tactile.
        if (radius >= 16.0f)
        {
            const int ridges = juce::jlimit (22, 44, (int) std::round (radius * 1.9f));
            for (int ridge = 0; ridge < ridges; ++ridge)
            {
                const float angle = juce::MathConstants<float>::twoPi * (float) ridge / (float) ridges;
                const auto a = pointOnRing (angle, radius - 2.6f);
                const auto b = pointOnRing (angle, radius - 0.4f);
                // Shade each ridge by its own facing relative to the light.
                const float facing = 0.5f + 0.5f * std::cos (angle - juce::MathConstants<float>::pi * 1.25f);
                g.setColour (juce::Colours::black.withAlpha (0.16f + 0.26f * (1.0f - facing)));
                g.drawLine (a.x, a.y + 0.5f, b.x, b.y + 0.5f, 0.6f);
                g.setColour (juce::Colour (0xFFE6D8BD).withAlpha (0.04f + 0.10f * facing));
                g.drawLine (a.x, a.y, b.x, b.y, 0.5f);
            }
        }

        // The cream cap: warm, slightly domed, matte rather than glossy.  The
        // gradient runs from the lit upper-left to the shaded lower-right.
        juce::ColourGradient cap (juce::Colour (0xFFF7EEDC), face.getX(), face.getY(),
                                  juce::Colour (0xFFBCAC8E), face.getRight(), face.getBottom(), false);
        g.setGradientFill (cap);
        g.fillEllipse (face);
        const auto capInset = face.reduced (1.5f);
        juce::ColourGradient capCore (juce::Colour (0xFFF1E6D0), capInset.getX(), capInset.getY(),
                                      juce::Colour (0xFFCCC0A6), capInset.getRight(), capInset.getBottom(), false);
        g.setGradientFill (capCore);
        g.fillEllipse (capInset);
        // Specular bloom on the lit shoulder, and a terminator on the dark one.
        const auto drawShoulderArc = [&g] (const juce::Rectangle<float>& bounds,
                                          float startAngle, float endAngle,
                                          juce::Colour colour, float thickness)
        {
            juce::Path shoulder;
            shoulder.addCentredArc (bounds.getCentreX(), bounds.getCentreY(),
                                    bounds.getWidth() * 0.5f, bounds.getHeight() * 0.5f,
                                    0.0f, startAngle, endAngle, true);
            g.setColour (colour);
            g.strokePath (shoulder, juce::PathStrokeType (thickness,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded));
        };
        drawShoulderArc (capInset, 0.80f * juce::MathConstants<float>::pi,
                         1.18f * juce::MathConstants<float>::pi,
                         juce::Colour (0xFFFFF8EA).withAlpha (0.34f), 1.5f);
        drawShoulderArc (capInset, 0.20f * juce::MathConstants<float>::pi,
                         0.52f * juce::MathConstants<float>::pi,
                         juce::Colour (0xFF7A684A).withAlpha (0.26f), 1.2f);
        g.setColour (juce::Colour (0xff8d7a58).withAlpha (0.34f));
        g.drawEllipse (capInset, 0.7f);
        // Faint moulding seam: the hairline a two-part plastic knob leaves.
        g.setColour (juce::Colour (0xff8d7a58).withAlpha (0.16f));
        g.drawEllipse (face.reduced (0.7f), 0.6f);
        g.setColour (juce::Colour (0xff8d7a58).withAlpha (0.18f));
        g.drawEllipse (face.reduced (0.7f), 0.6f);

        // Printed scale marks on the panel around the cap.
        for (int tick = 0; tick <= 10; ++tick)
        {
            const float t = static_cast<float> (tick) / 10.0f;
            const float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
            const bool major = tick % 2 == 0;
            const auto tickOuter = pointOnRing (angle, radius + 3.0f);
            const auto tickInner = pointOnRing (angle, radius + (major ? -0.8f : 1.0f));
            g.setColour (juce::Colour (0xffe8d9b8).withAlpha (major ? 0.62f : 0.34f));
            g.drawLine (tickOuter.x, tickOuter.y, tickInner.x, tickInner.y, major ? 1.0f : 0.65f);
        }

        const float valueAngle = rotaryStartAngle
                               + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        // The value arc is printed ink on the skirt, not an emissive ring.
        juce::Path arc;
        arc.addCentredArc (cx, cy, radius + 1.2f, radius + 1.2f,
                           0.0f, rotaryStartAngle, valueAngle, true);
        g.setColour (juce::Colour (0xfff0d9a8).withAlpha (0.50f));
        g.strokePath (arc, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Pointer: a single black line printed into the cap, the way an
        // analogue dial is actually read.
        const float pointerLength = radius * 0.80f;
        const auto pointer = pointOnRing (valueAngle, pointerLength);
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.drawLine (centre.x, centre.y + 0.6f, pointer.x, pointer.y + 0.6f, 3.4f);
        g.setColour (juce::Colour (0xff241d16));
        g.drawLine (centre.x, centre.y, pointer.x, pointer.y, 2.2f);

        const auto caption = juce::Rectangle<int> (x + 1, y + height - 17, width - 2, 16);
        const bool isAdjusting = slider.getProperties().getWithDefault ("eon.dragging", false);
        const auto text = isAdjusting ? slider.getTextFromValue (slider.getValue()).toUpperCase()
                                      : slider.getName();
        g.setFont (isAdjusting ? readoutFont (12.3f) : panelFont (12.0f, true, 0.93f));
        g.setColour (isAdjusting ? juce::Colour (0xfff6d78c) : juce::Colour (0xffece0c8).withAlpha (0.94f));
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
    void knob(juce::Slider&, const juce::String&, EonAnalogLookAndFeel::DialGrade = EonAnalogLookAndFeel::standard);
    void applyPreset (int index);
    void savePreset();
    void loadPreset();
    EonMiniEEFProcessor& processor;
    juce::ComboBox osc1Wave, osc2Wave, osc3Wave, osc4Wave, voiceMode, filterMode, driveCurveMode, lfoShapeMode;
    juce::ComboBox oversamplingMode;
    juce::ComboBox presetMenu;
    juce::TextButton initButton { "INIT" }, saveButton { "SAVE" }, loadButton { "LOAD" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::array<juce::Slider, 4> oscLevel, oscCoarse, oscFine, oscPhase, oscPan, oscPulseWidth;
    juce::Slider noiseMix, amDepth, unisonVoices, unisonDetune, unisonSpread, unisonPhase, unisonDrift, voiceVariance, attack, decay, sustain, release, envCurve, filterAttack, filterDecay, filterSustain, filterRelease, filterEnvAmount, cutoff, resonance, filterDrive, gain, drive, ampSaturation;
    juce::Slider lfoRate, lfoDepth, lfoPitch, velocityAmount;
    juce::Slider fxWet, delayTime, delayFeedback, delayStereo, chorusDepth, chorusRate, chorusMix, reverbMix, reverbModulation;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osc1Attachment, osc2Attachment, osc3Attachment, osc4Attachment, modeAttachment, filterModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> driveCurveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoShapeAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 4> oscLevelAttachment, oscCoarseAttachment, oscFineAttachment, oscPhaseAttachment, oscPanAttachment, oscPulseWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseMixAttachment, amDepthAttachment, unisonVoicesAttachment, unisonDetuneAttachment, unisonSpreadAttachment, unisonPhaseAttachment, unisonDriftAttachment, voiceVarianceAttachment, attackAttachment, decayAttachment, sustainAttachment, releaseAttachment, envCurveAttachment, cutoffAttachment, resonanceAttachment, filterDriveAttachment, gainAttachment, driveAttachment, ampSatAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterAttackAttachment, filterDecayAttachment, filterSustainAttachment, filterReleaseAttachment, filterEnvAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment, lfoDepthAttachment, lfoPitchAttachment, velocityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fxWetAttachment, delayTimeAttachment, delayFeedbackAttachment, delayStereoAttachment, chorusDepthAttachment, chorusRateAttachment, chorusMixAttachment, reverbMixAttachment, reverbModulationAttachment;
    float meterLeft = 0.0f, meterRight = 0.0f;
    // Cached wood cabinet: the grain needs hundreds of thin strokes to read
    // as wood, so it is rasterised on resize instead of every paint().
    juce::Image woodCache;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EonMiniEEFEditor)
};
