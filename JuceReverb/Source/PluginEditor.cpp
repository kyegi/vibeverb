#include "PluginEditor.h"

// =============================================================================
// Constructor
// =============================================================================
ReverbAudioProcessorEditor::ReverbAudioProcessorEditor(ReverbAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&laf);

    // -------------------------------------------------------------------------
    // Configure knobs
    // -------------------------------------------------------------------------
    auto setup = [&](KnobWithLabel& knob,
                     const juce::String& name,
                     const juce::String& suffix,
                     const juce::String& paramID,
                     std::unique_ptr<SliderAttachment>& attach)
    {
        knob.setLabelName(name);
        knob.setSuffix(suffix);
        addAndMakeVisible(knob);

        attach = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, knob.slider);
        knob.updateValueLabel(); // show initial value
    };

    setup(dryWetKnob,   "DRY / WET",    "%",  ParamID::DRY_WET,   dryWetAttach);
    setup(decayKnob,    "DECAY",         "",   ParamID::DECAY,     decayAttach);
    setup(preDelayKnob, "PRE-DELAY",     "ms", ParamID::PRE_DELAY, preDelayAttach);
    setup(sizeKnob,     "SIZE",          "",   ParamID::SIZE,      sizeAttach);
    setup(widthKnob,    "STEREO WIDTH",  "",   ParamID::WIDTH,     widthAttach);

    // -------------------------------------------------------------------------
    // Window size
    // -------------------------------------------------------------------------
    setResizable(false, false);
    setSize(660, 290);
}

ReverbAudioProcessorEditor::~ReverbAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

// =============================================================================
// paint
// =============================================================================
void ReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    // --- Background gradient ---
    juce::ColourGradient bg(juce::Colour(0xFF0D0D1A), 0.0f,                      0.0f,
                            juce::Colour(0xFF1A1A38), static_cast<float>(getWidth()),
                                                      static_cast<float>(getHeight()),
                            false);
    g.setGradientFill(bg);
    g.fillAll();

    // --- Subtle horizontal separator below the title ---
    g.setColour(juce::Colour(0xFF2A2A5A));
    g.drawLine(20.0f, 52.0f, static_cast<float>(getWidth()) - 20.0f, 52.0f, 1.0f);

    // --- Plugin title ---
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("JUCE REVERB", getLocalBounds().removeFromTop(52),
               juce::Justification::centred);

    // --- Manufacturer tag ---
    g.setColour(juce::Colour(0xFF555588));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("VST3  |  MyStudio",
               getLocalBounds().removeFromTop(52).translated(0, 30),
               juce::Justification::centred);
}

// =============================================================================
// resized
// =============================================================================
void ReverbAudioProcessorEditor::resized()
{
    // Layout: 5 knobs in a single row, evenly spaced.
    //
    //  +--title--+
    //  |         |
    //  | [1][2][3][4][5] |
    //  |         |
    //  +---------+

    constexpr int titleH  = 55;
    constexpr int padding = 16;
    constexpr int knobW   = (660 - padding * 2) / 5;  // ~125 px each

    auto knobArea = getLocalBounds()
                        .removeFromBottom(getHeight() - titleH)
                        .reduced(padding);

    KnobWithLabel* knobs[] = { &dryWetKnob, &decayKnob, &preDelayKnob,
                                &sizeKnob,   &widthKnob };

    for (auto* knob : knobs)
    {
        knob->setBounds(knobArea.removeFromLeft(knobW));
        knobArea.removeFromLeft(0); // no extra gap; knobW already divides evenly
    }
}
