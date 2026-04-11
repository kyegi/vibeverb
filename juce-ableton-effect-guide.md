# Building an Ableton Audio Effect with JUCE

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Setting Up JUCE](#setting-up-juce)
4. [Creating the Plugin Project](#creating-the-plugin-project)
5. [Project Structure](#project-structure)
6. [The AudioProcessor](#the-audioprocessor)
7. [Plugin Parameters](#plugin-parameters)
8. [Processing Audio](#processing-audio)
9. [The Plugin Editor (UI)](#the-plugin-editor-ui)
10. [State Management](#state-management)
11. [Building the Plugin](#building-the-plugin)
12. [Loading in Ableton Live](#loading-in-ableton-live)
13. [Complete Example: Gain + Highpass Filter](#complete-example-gain--highpass-filter)

---

## Overview

JUCE (Jules' Utility Class Extensions) is a C++ framework for building cross-platform audio applications and plugins. It supports the following plugin formats, all compatible with Ableton Live:

| Format | Platform | Ableton Support |
|--------|----------|-----------------|
| VST3   | Win / Mac | Yes |
| AU     | Mac only  | Yes (Mac only) |
| AAX    | Win / Mac | No (Pro Tools only) |
| Standalone | Win / Mac | N/A (not a plugin) |

For Ableton Live on **Windows**, target **VST3**.
For Ableton Live on **macOS**, target **VST3** and/or **AU**.

---

## Prerequisites

- **C++ knowledge** — intermediate level (classes, inheritance, templates)
- **JUCE** — download from [juce.com](https://juce.com) (free for open-source, paid for commercial)
- **IDE**:
  - Windows: Visual Studio 2019 or 2022
  - macOS: Xcode 13+
- **CMake** (optional, modern alternative to Projucer) — version 3.22+
- **Ableton Live** — any version that supports VST3/AU

---

## Setting Up JUCE

### Option A: Projucer (GUI-based, beginner-friendly)

1. Download JUCE from the official site and extract it.
2. Open the `Projucer` application inside the JUCE folder.
3. Sign in or use the free/GPL license.
4. Set your global paths: **Projucer > Global Paths**
   - Path to JUCE: `/path/to/JUCE`
   - VST3 SDK: bundled with JUCE (no separate download needed)

### Option B: CMake (recommended for CI/teams)

```bash
git clone https://github.com/juce-framework/JUCE.git
```

Add to your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyEffect VERSION 1.0.0)

add_subdirectory(JUCE)

juce_add_plugin(MyEffect
    COMPANY_NAME "YourCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Eff1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "My Effect"
)

target_sources(MyEffect PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(MyEffect PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MyEffect PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)
```

Build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## Creating the Plugin Project

### Using Projucer

1. **New Project > Audio Plug-In**
2. Fill in:
   - **Project Name**: `MyGainEffect`
   - **Plugin Name**: `My Gain Effect`
   - **Plugin Manufacturer**: `YourCompany`
   - **Plugin Manufacturer Code**: 4 characters, e.g. `Mnfr`
   - **Plugin Code**: 4 unique characters, e.g. `Mge1`
3. Under **Plugin Formats**, check: `VST3`, `AU` (mac), `Standalone`
4. Set **Plugin is a Synth**: `OFF`
5. Set **Plugin MIDI Input**: `OFF`
6. Click **Create Project** and choose your IDE

Projucer generates two key files:
- `PluginProcessor.h / .cpp` — audio logic
- `PluginEditor.h / .cpp` — UI

---

## Project Structure

```
MyGainEffect/
├── Source/
│   ├── PluginProcessor.h
│   ├── PluginProcessor.cpp
│   ├── PluginEditor.h
│   └── PluginEditor.cpp
├── JuceLibraryCode/
│   ├── JuceHeader.h
│   └── ...
└── MyGainEffect.jucer   (or CMakeLists.txt)
```

---

## The AudioProcessor

The `AudioProcessor` is the core of your plugin. It handles all audio processing.

### PluginProcessor.h

```cpp
#pragma once
#include <JuceHeader.h>

class MyGainEffectAudioProcessor : public juce::AudioProcessor
{
public:
    MyGainEffectAudioProcessor();
    ~MyGainEffectAudioProcessor() override;

    // --- Lifecycle ---
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // --- Plugin info ---
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // --- Programs (presets) ---
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // --- State ---
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- Editor ---
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // --- Parameters ---
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyGainEffectAudioProcessor)
};
```

---

## Plugin Parameters

Use `AudioProcessorValueTreeState` (APVTS) to manage parameters. It handles:
- Automation in Ableton
- Thread-safe parameter access
- State save/restore

### Defining Parameters

```cpp
// In PluginProcessor.cpp

juce::AudioProcessorValueTreeState::ParameterLayout
MyGainEffectAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Gain parameter: -60dB to +12dB, default 0dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "gain", 1 },   // parameterID, version hint
        "Gain",                             // parameter name (shown in Ableton)
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f),
        0.0f                               // default value
    ));

    // Bypass toggle
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 },
        "Bypass",
        false
    ));

    return { params.begin(), params.end() };
}

// Constructor — initialize APVTS
MyGainEffectAudioProcessor::MyGainEffectAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}
```

### Reading Parameters in processBlock

```cpp
// Safe atomic read — use getRawParameterValue for real-time audio thread
auto* gainParam  = apvts.getRawParameterValue("gain");
auto* bypassParam = apvts.getRawParameterValue("bypass");

float gainDb  = gainParam->load();   // thread-safe atomic load
bool  bypass  = bypassParam->load() > 0.5f;
```

---

## Processing Audio

### prepareToPlay

Called once before playback begins. Initialize DSP objects here.

```cpp
void MyGainEffectAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32>(getTotalNumOutputChannels());

    // Initialize any DSP here, e.g.:
    // myFilter.prepare(spec);
}
```

### processBlock

Called for every audio buffer during playback. **This runs on the real-time audio thread — no memory allocation, no locks, no UI calls.**

```cpp
void MyGainEffectAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals; // prevents CPU spikes from denormal floats

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Read parameters (atomic, safe on audio thread)
    float gainDb = apvts.getRawParameterValue("gain")->load();
    bool  bypass = apvts.getRawParameterValue("bypass")->load() > 0.5f;

    if (bypass) return;

    // Convert dB to linear gain
    float gainLinear = juce::Decibels::decibelsToGain(gainDb);

    // Apply gain to all channels
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            channelData[sample] *= gainLinear;
    }
}
```

---

## The Plugin Editor (UI)

The editor is the visual interface shown when you open the plugin in Ableton.

### PluginEditor.h

```cpp
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MyGainEffectAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MyGainEffectAudioProcessorEditor(MyGainEffectAudioProcessor&);
    ~MyGainEffectAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MyGainEffectAudioProcessor& audioProcessor;

    // UI Components
    juce::Slider gainSlider;
    juce::Label  gainLabel;
    juce::ToggleButton bypassButton;

    // Attachments sync the UI component to an APVTS parameter automatically
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyGainEffectAudioProcessorEditor)
};
```

### PluginEditor.cpp

```cpp
#include "PluginEditor.h"

MyGainEffectAudioProcessorEditor::MyGainEffectAudioProcessorEditor(
    MyGainEffectAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Gain slider setup
    gainSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    gainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(gainSlider);

    gainLabel.setText("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gainLabel);

    bypassButton.setButtonText("Bypass");
    addAndMakeVisible(bypassButton);

    // Wire sliders/buttons to APVTS parameters
    gainAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "gain", gainSlider);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "bypass", bypassButton);

    setSize(300, 200);
}

MyGainEffectAudioProcessorEditor::~MyGainEffectAudioProcessorEditor() {}

void MyGainEffectAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawFittedText("My Gain Effect", getLocalBounds().removeFromTop(30),
                     juce::Justification::centred, 1);
}

void MyGainEffectAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    area.removeFromTop(30); // title space

    gainLabel.setBounds(area.removeFromTop(20));
    gainSlider.setBounds(area.removeFromBottom(120).withSizeKeepingCentre(120, 120));
    bypassButton.setBounds(area.removeFromBottom(30).withSizeKeepingCentre(100, 25));
}
```

---

## State Management

Ableton saves and restores plugin state in sessions. Implement `getStateInformation` and `setStateInformation`.

```cpp
void MyGainEffectAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Serialize the APVTS parameter tree to XML, then to binary
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MyGainEffectAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Deserialize binary back to XML, then restore APVTS state
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}
```

---

## Building the Plugin

### With Projucer

1. Open your `.jucer` file in Projucer
2. Click **Save and Open in IDE**
3. In your IDE, select the **Release** build configuration
4. Build the project
5. The plugin is automatically copied to your system plugin folder:
   - **Windows VST3**: `C:\Program Files\Common Files\VST3\`
   - **macOS VST3**: `~/Library/Audio/Plug-Ins/VST3/`
   - **macOS AU**: `~/Library/Audio/Plug-Ins/Components/`

### With CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Output locations (example):
# build/MyEffect_artefacts/Release/VST3/MyEffect.vst3
# build/MyEffect_artefacts/Release/AU/MyEffect.component
```

Copy the output to your system plugin folder manually, or add an install target:

```cmake
# In CMakeLists.txt
juce_enable_copy_plugin_step(MyEffect)
```

This copies the built plugin automatically after each build.

---

## Loading in Ableton Live

### First-time Setup

1. Open **Ableton Live > Preferences > Plugins**
2. Enable **"Use VST3 Plug-In System Folders"** (Windows) or **"Use Audio Units"** (macOS)
3. Set a custom VST3 folder if needed, or use the system default
4. Click **"Rescan"**

### Using the Plugin

1. In the **Browser**, navigate to **Plug-ins > VST3** (or AU on Mac)
2. Find **My Gain Effect** under your manufacturer name
3. Drag it onto an audio track or into a rack
4. Automate any parameter by right-clicking the knob > **"Show Automation"**

---

## Complete Example: Gain + Highpass Filter

This example combines a gain knob and a highpass filter using `juce::dsp`.

### PluginProcessor.h

```cpp
#pragma once
#include <JuceHeader.h>

class GainFilterProcessor : public juce::AudioProcessor
{
public:
    GainFilterProcessor();
    ~GainFilterProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GainFilter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;

    juce::dsp::ProcessorDuplicator<Filter, FilterCoefs> highpassFilter;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GainFilterProcessor)
};
```

### PluginProcessor.cpp

```cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout GainFilterProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain", 1}, "Gain",
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"hpFreq", 1}, "HP Frequency",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f), // skew for log feel
        80.0f));

    return { params.begin(), params.end() };
}

GainFilterProcessor::GainFilterProcessor()
    : AudioProcessor(BusesProperties()
        .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createLayout())
{}

void GainFilterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = 2;

    highpassFilter.prepare(spec);

    // Set initial coefficients
    float hpFreq = apvts.getRawParameterValue("hpFreq")->load();
    *highpassFilter.state = *FilterCoefs::makeFirstOrderHighPass(sampleRate, hpFreq);
}

void GainFilterProcessor::processBlock(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    float gainDb = apvts.getRawParameterValue("gain")->load();
    float hpFreq = apvts.getRawParameterValue("hpFreq")->load();

    // Update filter coefficients (safe to do per-block for smooth automation)
    *highpassFilter.state = *FilterCoefs::makeFirstOrderHighPass(
        currentSampleRate, static_cast<double>(hpFreq));

    // Apply highpass filter via DSP context
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    highpassFilter.process(ctx);

    // Apply gain
    float gainLinear = juce::Decibels::decibelsToGain(gainDb);
    buffer.applyGain(gainLinear);
}

void GainFilterProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void GainFilterProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* GainFilterProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this); // auto-generated UI
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GainFilterProcessor();
}
```

> **Tip**: `juce::GenericAudioProcessorEditor` auto-generates a basic UI from your APVTS parameters. It is great for prototyping before building a custom UI.

---

## Key Rules for the Audio Thread

These rules apply inside `processBlock`:

| Rule | Reason |
|------|--------|
| No `new` / `delete` / `malloc` | Memory allocation can block |
| No mutexes / locks | Can cause priority inversion |
| No file I/O | Disk access can block |
| No UI calls | Not thread-safe |
| No `std::cout` | Can block on some platforms |
| Use `std::atomic` for shared state | Safe cross-thread parameter reads |
| Use `juce::AbstractFifo` for queues | Lock-free communication |

---

## Further Reading

- **JUCE DSP Module** — `juce::dsp` contains ready-made filters, convolution, oscillators, compressors, and more
- **JUCE Tutorials** — official step-by-step tutorials at juce.com/learn/tutorials
- **AudioProcessorGraph** — for building multi-node effect chains inside a single plugin
- **MIDI Effects** — set `acceptsMidi(true)` and process `MidiBuffer` for MIDI FX
- **LookAndFeel** — subclass `juce::LookAndFeel_V4` to fully customize the UI style
- **CLAP format** — newer open plugin format; JUCE 7+ has community support via `clap-juce-extensions`
