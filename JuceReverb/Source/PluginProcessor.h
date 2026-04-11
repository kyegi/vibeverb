#pragma once
#include <JuceHeader.h>

// =============================================================================
// CombFilter
//
// Feedback comb filter with a one-pole lowpass in the feedback path (damping).
// Buffer is pre-allocated to a maximum size; the effective delay length can be
// changed at runtime without any heap allocation.
// =============================================================================
class CombFilter
{
public:
    // Allocate buffer for up to maxSamples delay.
    void prepare(int maxSamples)
    {
        buffer.assign(static_cast<size_t>(maxSamples), 0.0f);
        maxSize      = maxSamples;
        currentSize  = maxSamples;
        writePos     = 0;
        filterState  = 0.0f;
    }

    // Change effective delay length without allocating.
    void setEffectiveSize(int samples) noexcept
    {
        currentSize = juce::jlimit(1, maxSize, samples);
    }

    void setFeedback(float f) noexcept { feedback = f; }
    void setDamp    (float d) noexcept { damp     = d; }

    float process(float input) noexcept
    {
        // Read output sample from currentSize samples ago
        int readPos = writePos - currentSize;
        if (readPos < 0) readPos += maxSize;

        const float output = buffer[static_cast<size_t>(readPos)];

        // One-pole lowpass in feedback path (frequency-dependent decay)
        filterState = output * (1.0f - damp) + filterState * damp;

        buffer[static_cast<size_t>(writePos)] = input + filterState * feedback;

        if (++writePos >= maxSize) writePos = 0;
        return output;
    }

    void clear() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        filterState = 0.0f;
        writePos    = 0;
    }

private:
    std::vector<float> buffer;
    int   maxSize     = 0;
    int   currentSize = 0;
    int   writePos    = 0;
    float feedback    = 0.5f;
    float filterState = 0.0f;  // one-pole lowpass state
    float damp        = 0.3f;
};

// =============================================================================
// AllpassFilter
//
// Schroeder allpass filter for diffusing the reverb tail.
// =============================================================================
class AllpassFilter
{
public:
    void prepare(int maxSamples)
    {
        buffer.assign(static_cast<size_t>(maxSamples), 0.0f);
        maxSize     = maxSamples;
        currentSize = maxSamples;
        writePos    = 0;
    }

    void setEffectiveSize(int samples) noexcept
    {
        currentSize = juce::jlimit(1, maxSize, samples);
    }

    void setFeedback(float f) noexcept { feedback = f; }

    float process(float input) noexcept
    {
        int readPos = writePos - currentSize;
        if (readPos < 0) readPos += maxSize;

        const float bufOut = buffer[static_cast<size_t>(readPos)];
        const float output = bufOut - input;

        buffer[static_cast<size_t>(writePos)] = input + bufOut * feedback;

        if (++writePos >= maxSize) writePos = 0;
        return output;
    }

    void clear() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

private:
    std::vector<float> buffer;
    int   maxSize     = 0;
    int   currentSize = 0;
    int   writePos    = 0;
    float feedback    = 0.5f;
};

// =============================================================================
// PreDelayLine
//
// Simple circular delay buffer. Max delay is fixed at prepare(); the actual
// delay in milliseconds can be changed at any time without allocation.
// =============================================================================
class PreDelayLine
{
public:
    static constexpr float MAX_DELAY_MS = 200.0f;

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        const int maxSamples = static_cast<int>(sampleRate * MAX_DELAY_MS / 1000.0) + 2;
        buffer.assign(static_cast<size_t>(maxSamples), 0.0f);
        maxSize      = maxSamples;
        writePos     = 0;
        delaySamples = 0;
    }

    void setDelayMs(float ms) noexcept
    {
        delaySamples = juce::jlimit(0, maxSize - 1,
                                    static_cast<int>(sr * static_cast<double>(ms) / 1000.0));
    }

    float process(float input) noexcept
    {
        if (maxSize == 0) return input;

        buffer[static_cast<size_t>(writePos)] = input;

        int readPos = writePos - delaySamples;
        if (readPos < 0) readPos += maxSize;

        const float output = buffer[static_cast<size_t>(readPos)];
        if (++writePos >= maxSize) writePos = 0;
        return output;
    }

    void clear() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

private:
    std::vector<float> buffer;
    double sr            = 44100.0;
    int    maxSize       = 0;
    int    writePos      = 0;
    int    delaySamples  = 0;
};

// =============================================================================
// ReverbEngine
//
// Freeverb-inspired stereo reverb:
//   • 8 parallel feedback comb filters per channel (slightly detuned L vs R)
//   • 4 series allpass filters per channel for diffusion
//   • Pre-delay line per channel
//   • Mid-side width processing on the wet signal
//
// All buffers are pre-allocated; size, decay, pre-delay, and width can be
// updated safely from the audio thread.
// =============================================================================
class ReverbEngine
{
public:
    static constexpr int NUM_COMBS     = 8;
    static constexpr int NUM_ALLPASSES = 4;

    // Delay lengths in samples at 44.1 kHz, derived from Freeverb.
    // L and R are offset so the two channels decorrelate naturally.
    static constexpr int BASE_COMB_L[NUM_COMBS]      = { 1116, 1188, 1277, 1356,
                                                           1422, 1491, 1557, 1617 };
    static constexpr int BASE_COMB_R[NUM_COMBS]      = { 1139, 1211, 1300, 1379,
                                                           1445, 1514, 1580, 1640 };
    static constexpr int BASE_ALLPASS_L[NUM_ALLPASSES] = { 556, 441, 341, 225 };
    static constexpr int BASE_ALLPASS_R[NUM_ALLPASSES] = { 579, 464, 364, 248 };

    // Call once per prepareToPlay. Allocates all buffers.
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        const float srScale = static_cast<float>(sampleRate / 44100.0);

        // Allocate at 2× the base length so size up to 2.0 fits without realloc.
        for (int i = 0; i < NUM_COMBS; ++i)
        {
            combL[i].prepare(static_cast<int>(BASE_COMB_L[i] * srScale * 2.0f) + 4);
            combR[i].prepare(static_cast<int>(BASE_COMB_R[i] * srScale * 2.0f) + 4);
        }
        for (int i = 0; i < NUM_ALLPASSES; ++i)
        {
            allpassL[i].prepare(static_cast<int>(BASE_ALLPASS_L[i] * srScale * 2.0f) + 4);
            allpassR[i].prepare(static_cast<int>(BASE_ALLPASS_R[i] * srScale * 2.0f) + 4);
            allpassL[i].setFeedback(0.5f);
            allpassR[i].setFeedback(0.5f);
        }

        preDelayL.prepare(sampleRate);
        preDelayR.prepare(sampleRate);

        applySize(1.0f);
    }

    // size: 0.1 – 2.0. Scales comb/allpass delay lengths.
    // Safe to call from the audio thread (no allocation).
    void applySize(float size) noexcept
    {
        const float srScale = static_cast<float>(sr / 44100.0);
        const float s       = juce::jlimit(0.1f, 2.0f, size);

        for (int i = 0; i < NUM_COMBS; ++i)
        {
            combL[i].setEffectiveSize(static_cast<int>(BASE_COMB_L[i] * srScale * s));
            combR[i].setEffectiveSize(static_cast<int>(BASE_COMB_R[i] * srScale * s));
        }
        for (int i = 0; i < NUM_ALLPASSES; ++i)
        {
            allpassL[i].setEffectiveSize(static_cast<int>(BASE_ALLPASS_L[i] * srScale * s));
            allpassR[i].setEffectiveSize(static_cast<int>(BASE_ALLPASS_R[i] * srScale * s));
        }
    }

    // decay: 0.0 – 1.0. Maps to comb feedback 0.70 – 0.98 (longer = more decay).
    void setDecay(float decay) noexcept
    {
        const float fb = 0.70f + juce::jlimit(0.0f, 1.0f, decay) * 0.28f;
        for (int i = 0; i < NUM_COMBS; ++i)
        {
            combL[i].setFeedback(fb);
            combR[i].setFeedback(fb);
        }
    }

    // damp: 0.0 – 1.0. Internal value, fixed for natural HF roll-off.
    void setDamp(float d) noexcept
    {
        for (int i = 0; i < NUM_COMBS; ++i)
        {
            combL[i].setDamp(d);
            combR[i].setDamp(d);
        }
    }

    void setPreDelayMs(float ms) noexcept
    {
        preDelayL.setDelayMs(ms);
        preDelayR.setDelayMs(ms);
    }

    // Process one stereo sample.
    // width : 0 (mono wet) – 2 (hyper-wide)
    // wet   : 0.0 – 1.0
    // dry   : 0.0 – 1.0  (typically 1.0 - wet)
    void processSample(float  inL, float  inR,
                       float& outL, float& outR,
                       float  width, float wet, float dry) noexcept
    {
        // --- Pre-delay ---
        const float pdL = preDelayL.process(inL);
        const float pdR = preDelayR.process(inR);

        // --- Mix to mono for reverb input (classic approach) ---
        const float input = (pdL + pdR) * 0.5f;

        // --- Parallel comb filters ---
        float reverbL = 0.0f, reverbR = 0.0f;
        for (int i = 0; i < NUM_COMBS; ++i)
        {
            reverbL += combL[i].process(input);
            reverbR += combR[i].process(input);
        }
        reverbL *= combScale;
        reverbR *= combScale;

        // --- Series allpass filters (diffusion) ---
        for (int i = 0; i < NUM_ALLPASSES; ++i)
        {
            reverbL = allpassL[i].process(reverbL);
            reverbR = allpassR[i].process(reverbR);
        }

        // --- Stereo width via mid-side ---
        //   width=0  → mono wet (only mid)
        //   width=1  → normal stereo
        //   width=2  → hyper-wide (exaggerated side)
        const float mid  = (reverbL + reverbR) * 0.5f;
        const float side = (reverbL - reverbR) * 0.5f;
        reverbL = mid + width * side;
        reverbR = mid - width * side;

        // --- Dry / wet mix ---
        outL = inL * dry + reverbL * wet;
        outR = inR * dry + reverbR * wet;
    }

    void clear() noexcept
    {
        for (int i = 0; i < NUM_COMBS;     ++i) { combL[i].clear();    combR[i].clear(); }
        for (int i = 0; i < NUM_ALLPASSES; ++i) { allpassL[i].clear(); allpassR[i].clear(); }
        preDelayL.clear();
        preDelayR.clear();
    }

private:
    CombFilter    combL[NUM_COMBS],     combR[NUM_COMBS];
    AllpassFilter allpassL[NUM_ALLPASSES], allpassR[NUM_ALLPASSES];
    PreDelayLine  preDelayL, preDelayR;
    double        sr        = 44100.0;

    // Normalise summed comb output
    static constexpr float combScale = 1.0f / static_cast<float>(NUM_COMBS);
};

// =============================================================================
// Parameter IDs — single definition, shared by processor and editor
// =============================================================================
namespace ParamID
{
    inline const juce::String DRY_WET   = "DRY_WET";
    inline const juce::String DECAY     = "DECAY";
    inline const juce::String PRE_DELAY = "PRE_DELAY";
    inline const juce::String SIZE      = "SIZE";
    inline const juce::String WIDTH     = "WIDTH";
}

// =============================================================================
// AudioProcessor
// =============================================================================
class ReverbAudioProcessor : public juce::AudioProcessor
{
public:
    ReverbAudioProcessor();
    ~ReverbAudioProcessor() override = default;

    // --- Lifecycle ---
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;  // unhide the double-precision overload

    // --- Editor ---
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // --- Info ---
    const juce::String getName()          const override { return JucePlugin_Name; }
    bool  acceptsMidi()                   const override { return false; }
    bool  producesMidi()                  const override { return false; }
    bool  isMidiEffect()                  const override { return false; }
    double getTailLengthSeconds()         const override { return 6.0; }

    // --- Programs ---
    int  getNumPrograms()                        override { return 1; }
    int  getCurrentProgram()                     override { return 0; }
    void setCurrentProgram(int)                  override {}
    const juce::String getProgramName(int)       override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // --- State ---
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Exposed so the editor can attach to parameters
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    ReverbEngine reverb;

    // Track size so we only call applySize when it actually changes
    float lastSize = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbAudioProcessor)
};
