#include "PluginProcessor.h"

JuceReverbAudioProcessor::JuceReverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pDryWet     = apvts.getRawParameterValue ("dryWet");
    pDecay      = apvts.getRawParameterValue ("decay");
    pSize       = apvts.getRawParameterValue ("size");
    pWidth      = apvts.getRawParameterValue ("width");
    pPreDelayMs = apvts.getRawParameterValue ("preDelayMs");
}

juce::AudioProcessorValueTreeState::ParameterLayout
JuceReverbAudioProcessor::createParameterLayout()
{
    using Float = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<Float> (juce::ParameterID { "dryWet",     1 }, "Dry/Wet",   Range { 0.0f, 1.0f, 0.0001f }, 0.3f));
    params.push_back (std::make_unique<Float> (juce::ParameterID { "decay",      1 }, "Decay",     Range { 0.0f, 1.0f, 0.0001f }, 0.5f));
    params.push_back (std::make_unique<Float> (juce::ParameterID { "size",       1 }, "Size",      Range { 0.0f, 1.0f, 0.0001f }, 0.5f));
    params.push_back (std::make_unique<Float> (juce::ParameterID { "width",      1 }, "Width",     Range { 0.0f, 1.0f, 0.0001f }, 1.0f));
    params.push_back (std::make_unique<Float> (juce::ParameterID { "preDelayMs", 1 }, "Pre-Delay", Range { 0.0f, 200.0f, 0.01f, 0.5f }, 20.0f));

    return { params.begin(), params.end() };
}

bool JuceReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void JuceReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32> (juce::jmax (getTotalNumInputChannels(), 2));

    reverb.prepare (spec);
    reverb.reset();

    preDelay.prepare (spec);
    preDelay.setMaximumDelayInSamples (static_cast<int> (0.21 * sampleRate) + 1);
    preDelay.reset();
}

void JuceReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const float wet    = pDryWet->load();
    const float decay  = pDecay->load();
    const float size   = pSize->load();
    const float width  = pWidth->load();
    const float preMs  = pPreDelayMs->load();

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf (buffer);

    juce::dsp::Reverb::Parameters rp;
    rp.roomSize   = size;
    rp.damping    = 1.0f - decay;
    rp.wetLevel   = 1.0f;
    rp.dryLevel   = 0.0f;
    rp.width      = width;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    preDelay.setDelay (juce::jlimit (0.0f,
                                     static_cast<float> (preDelay.getMaximumDelayInSamples() - 1),
                                     preMs * 0.001f * static_cast<float> (currentSampleRate)));

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    preDelay.process (ctx);
    reverb.process (ctx);

    buffer.applyGain (wet);
    for (int ch = 0; ch < juce::jmin (totalOut, dry.getNumChannels()); ++ch)
        buffer.addFrom (ch, 0, dry, ch, 0, numSamples, 1.0f - wet);
}

void JuceReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void JuceReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuceReverbAudioProcessor();
}
