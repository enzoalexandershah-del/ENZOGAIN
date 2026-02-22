#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class EnzoGainAudioProcessor : public juce::AudioProcessor
{
public:
    EnzoGainAudioProcessor();
    ~EnzoGainAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EnzoGain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Public access to parameters for editor
    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Saturation processing
    static inline float applySaturation(float x, int mode) noexcept
    {
        switch (mode)
        {
            case 1: // Tape — soft symmetric tanh saturation
                return std::tanh(x);

            case 2: // Tube — asymmetric exponential (adds even harmonics)
            {
                if (x >= 0.0f)
                    return 1.0f - std::exp(-x);
                else
                    return -(1.0f - std::exp(x * 0.8f)) / 0.8f;
            }

            case 3: // Digital — hard clip at ±1
                return juce::jlimit(-1.0f, 1.0f, x);

            case 4: // Fold — triangle wavefolder (bounded to ±1)
            {
                // Classic triangle fold: always stays in [-1, 1]
                float phase = std::fmod(x + 1.0f, 4.0f);
                if (phase < 0.0f) phase += 4.0f;
                return (phase < 2.0f) ? (phase - 1.0f) : (3.0f - phase);
            }

            default:
                return x;
        }
    }

    // Smoothed gain to avoid zipper noise
    juce::SmoothedValue<float> smoothedGain;
    juce::SmoothedValue<float> smoothedPan;

    // Smoothed saturation parameters  (avoids clicks / zipper noise)
    juce::SmoothedValue<float> smoothedDrive;   // drive multiplier (1‥10)
    juce::SmoothedValue<float> smoothedSatMix;  // 0 = dry, 1 = wet  (crossfades on enable/disable)

    // Peak-envelope follower for auto-gain compensation
    float satInputEnvelope  = 0.0f;
    float satEnvAttackCoeff = 0.0f;
    float satEnvReleaseCoeff = 0.0f;

    // LFO state
    double lfoPhase = 0.0;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnzoGainAudioProcessor)
};
