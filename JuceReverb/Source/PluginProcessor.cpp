#include "PluginProcessor.h"
#include "PluginEditor.h"

// =============================================================================
// Parameter layout
// =============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Dry / Wet  — 0 % (fully dry) … 100 % (fully wet), default 30 %
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::DRY_WET, 1 },
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.30f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float v, int) {
                return juce::String(static_cast<int>(v * 100)) + " %";
            })
    ));

    // Decay  — 0 (very short) … 1 (very long), default 0.5
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::DECAY, 1 },
        "Decay",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.50f
    ));

    // Pre-delay  — 0 … 200 ms, default 20 ms
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::PRE_DELAY, 1 },
        "Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f),
        20.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")
    ));

    // Size  — 0.1 (tiny room) … 2.0 (huge space), default 1.0
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::SIZE, 1 },
        "Size",
        juce::NormalisableRange<float>(0.1f, 2.0f, 0.01f),
        1.0f
    ));

    // Stereo Width  — 0 (mono wet) … 2 (hyper-wide), default 1.0
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::WIDTH, 1 },
        "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f),
        1.0f
    ));

    return { params.begin(), params.end() };
}

// =============================================================================
// Constructor / Destructor
// =============================================================================
ReverbAudioProcessor::ReverbAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

// =============================================================================
// Lifecycle
// =============================================================================
void ReverbAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    reverb.prepare(sampleRate);

    // Apply current parameter values immediately
    reverb.setDecay(*apvts.getRawParameterValue(ParamID::DECAY));
    reverb.setPreDelayMs(*apvts.getRawParameterValue(ParamID::PRE_DELAY));
    reverb.setDamp(0.35f);  // fixed internal damping — natural HF roll-off

    lastSize = *apvts.getRawParameterValue(ParamID::SIZE);
    reverb.applySize(lastSize);
}

void ReverbAudioProcessor::releaseResources()
{
    reverb.clear();
}

bool ReverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Accept stereo or mono on both in and out, but in/out must match.
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() &&
        out != juce::AudioChannelSet::stereo())
        return false;

    // Allow mono-in → stereo-out (upmix)
    if (in  != juce::AudioChannelSet::mono() &&
        in  != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

// =============================================================================
// processBlock
// =============================================================================
void ReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Silence unused output channels
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    // --- Read parameters (atomic loads, safe on audio thread) ---
    const float wet      = apvts.getRawParameterValue(ParamID::DRY_WET)->load();
    const float dry      = 1.0f - wet;
    const float decay    = apvts.getRawParameterValue(ParamID::DECAY)->load();
    const float preDelay = apvts.getRawParameterValue(ParamID::PRE_DELAY)->load();
    const float size     = apvts.getRawParameterValue(ParamID::SIZE)->load();
    const float width    = apvts.getRawParameterValue(ParamID::WIDTH)->load();

    // --- Apply parameters ---
    reverb.setDecay(decay);
    reverb.setPreDelayMs(preDelay);

    // Size only triggers the (allocation-free) length update when it changes
    if (std::abs(size - lastSize) > 0.005f)
    {
        lastSize = size;
        reverb.applySize(size);
    }

    // --- Get channel pointers (handle mono-in → stereo-out) ---
    float* channelL = buffer.getWritePointer(0);
    float* channelR = (buffer.getNumChannels() > 1)
                          ? buffer.getWritePointer(1)
                          : buffer.getWritePointer(0);

    const bool monoInput  = (totalIn  == 1);
    const bool monoOutput = (totalOut == 1);

    // --- Sample loop ---
    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = channelL[i];
        const float inR = monoInput ? inL : channelR[i];

        float outL, outR;
        reverb.processSample(inL, inR, outL, outR, width, wet, dry);

        channelL[i] = outL;
        if (!monoOutput)
            channelR[i] = outR;
    }
}

// =============================================================================
// State save / restore (for Ableton session files)
// =============================================================================
void ReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// =============================================================================
// Editor
// =============================================================================
juce::AudioProcessorEditor* ReverbAudioProcessor::createEditor()
{
    return new ReverbAudioProcessorEditor(*this);
}

// =============================================================================
// JUCE plug-in entry point
// =============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReverbAudioProcessor();
}
