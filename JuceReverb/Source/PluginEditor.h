#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// =============================================================================
// ReverbLookAndFeel
//
// Custom dark look-and-feel for a professional reverb UI:
//   • Dark navy background
//   • Teal/cyan arc knobs with a white indicator dot
// =============================================================================
class ReverbLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReverbLookAndFeel()
    {
        // Slider track / thumb colours used by the text-box
        setColour(juce::Slider::textBoxTextColourId,       juce::Colour(0xFFCCCCCC));
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF1A1A2E));
        setColour(juce::Slider::textBoxOutlineColourId,    juce::Colour(0xFF2A2A4A));
        setColour(juce::Label::textColourId,               juce::Colour(0xFFAAAAAA));
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& /*slider*/) override
    {
        const float radius  = static_cast<float>(juce::jmin(width, height)) * 0.5f - 6.0f;
        const float centreX = static_cast<float>(x) + static_cast<float>(width)  * 0.5f;
        const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;

        // --- Background circle ---
        g.setColour(juce::Colour(0xFF14142B));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        // --- Outer ring ---
        g.setColour(juce::Colour(0xFF2E2E5E));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        // --- Value arc (track) ---
        const float trackRadius = radius - 4.0f;
        const float lineW       = 4.0f;

        juce::Path arcTrack;
        arcTrack.addCentredArc(centreX, centreY,
                               trackRadius, trackRadius,
                               0.0f,
                               rotaryStartAngle, rotaryEndAngle,
                               true);
        g.setColour(juce::Colour(0xFF2A2A4A));
        g.strokePath(arcTrack, juce::PathStrokeType(lineW,
                     juce::PathStrokeType::curved,
                     juce::PathStrokeType::rounded));

        // --- Filled arc (value) ---
        const float toAngle = rotaryStartAngle
                              + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        juce::Path arcFill;
        arcFill.addCentredArc(centreX, centreY,
                              trackRadius, trackRadius,
                              0.0f,
                              rotaryStartAngle, toAngle,
                              true);

        // Gradient from teal to cyan
        juce::ColourGradient grad(juce::Colour(0xFF00B4C8), centreX - radius, centreY,
                                  juce::Colour(0xFF00E5FF), centreX + radius, centreY,
                                  false);
        g.setGradientFill(grad);
        g.strokePath(arcFill, juce::PathStrokeType(lineW,
                     juce::PathStrokeType::curved,
                     juce::PathStrokeType::rounded));

        // --- Indicator dot ---
        const float dotRadius = 4.0f;
        const float dotDist   = trackRadius - lineW * 0.5f;
        const float dotX      = centreX + dotDist * std::cos(toAngle - juce::MathConstants<float>::halfPi);
        const float dotY      = centreY + dotDist * std::sin(toAngle - juce::MathConstants<float>::halfPi);

        g.setColour(juce::Colours::white);
        g.fillEllipse(dotX - dotRadius, dotY - dotRadius,
                      dotRadius * 2.0f, dotRadius * 2.0f);
    }
};

// =============================================================================
// KnobWithLabel
//
// A small component that bundles a rotary slider + name label + value label.
// =============================================================================
class KnobWithLabel : public juce::Component
{
public:
    juce::Slider     slider;
    juce::Label      nameLabel;
    juce::Label      valueLabel;

    KnobWithLabel()
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);

        nameLabel.setJustificationType(juce::Justification::centred);
        nameLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xFFAAAAAA));
        addAndMakeVisible(nameLabel);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::FontOptions(11.0f));
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF00E5FF));
        addAndMakeVisible(valueLabel);

        slider.onValueChange = [this] { updateValueLabel(); };
    }

    void setLabelName(const juce::String& name) { nameLabel.setText(name, juce::dontSendNotification); }
    void setSuffix(const juce::String& suf) { suffix = suf; updateValueLabel(); }

    void resized() override
    {
        auto area = getLocalBounds();
        nameLabel.setBounds(area.removeFromTop(18));
        valueLabel.setBounds(area.removeFromBottom(18));
        slider.setBounds(area.reduced(4));
    }

    void updateValueLabel()
    {
        juce::String text;
        if (suffix == "%")
            text = juce::String(static_cast<int>(slider.getValue() * 100)) + " %";
        else if (suffix == "ms")
            text = juce::String(slider.getValue(), 1) + " ms";
        else
            text = juce::String(slider.getValue(), 2) + (suffix.isNotEmpty() ? " " + suffix : "");
        valueLabel.setText(text, juce::dontSendNotification);
    }

private:
    juce::String suffix;
};

// =============================================================================
// ReverbAudioProcessorEditor
// =============================================================================
class ReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReverbAudioProcessorEditor(ReverbAudioProcessor& p);
    ~ReverbAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ReverbAudioProcessor& audioProcessor;
    ReverbLookAndFeel     laf;

    // Five parameter knobs
    KnobWithLabel dryWetKnob;
    KnobWithLabel decayKnob;
    KnobWithLabel preDelayKnob;
    KnobWithLabel sizeKnob;
    KnobWithLabel widthKnob;

    // APVTS attachments — keep sliders in sync with the processor
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> dryWetAttach;
    std::unique_ptr<SliderAttachment> decayAttach;
    std::unique_ptr<SliderAttachment> preDelayAttach;
    std::unique_ptr<SliderAttachment> sizeAttach;
    std::unique_ptr<SliderAttachment> widthAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbAudioProcessorEditor)
};
