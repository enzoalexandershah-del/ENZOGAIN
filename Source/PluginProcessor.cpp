#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout EnzoGainAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // GAIN - Linear gain from 0.0 to 1.5 (displayed as 0% to 150%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "GAIN", 1 },
        "Gain",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f, 1.0f),
        1.0f,
        "%"
    ));

    // LFO_STRENGTH - How much the LFO modulates the gain (0% to 100%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LFO_STRENGTH", 1 },
        "LFO Strength",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f, 1.0f),
        0.0f,
        "%"
    ));

    // LFO_FREQ - LFO rate in Hz (0.1 to 20 Hz)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "LFO_FREQ", 1 },
        "LFO Frequency",
        juce::NormalisableRange<float>(0.1f, 20.0f, 0.01f, 0.4f),
        1.0f,
        "Hz"
    ));

    // LFO_ENABLED - On/Off toggle for LFO section
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "LFO_ENABLED", 1 },
        "LFO Enabled",
        false
    ));

    // PAN - Stereo panning from -100 (full left) to +100 (full right)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "PAN", 1 },
        "Pan",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f, 1.0f),
        0.0f,
        "%"
    ));

    // SAT_MODE - Saturation curve selector (Off, Tape, Tube, Digital, Fold)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "SAT_MODE", 1 },
        "Saturation Mode",
        juce::StringArray { "Off", "Tape", "Tube", "Digital", "Fold" },
        0
    ));

    // SAT_ENABLED - On/Off toggle for saturation section
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "SAT_ENABLED", 1 },
        "Saturation Enabled",
        false
    ));

    // SAT_DRIVE - How hard to drive into the saturation (0-100%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "SAT_DRIVE", 1 },
        "Saturation Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f, 1.0f),
        0.0f,
        "%"
    ));

    return layout;
}

EnzoGainAudioProcessor::EnzoGainAudioProcessor()
    : AudioProcessor(BusesProperties()
                        .withInput("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

EnzoGainAudioProcessor::~EnzoGainAudioProcessor()
{
}

void EnzoGainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;

    // Initialize smoothed gain to avoid zipper noise on parameter changes
    smoothedGain.reset(sampleRate, 0.02);  // 20ms smoothing
    smoothedGain.setCurrentAndTargetValue(
        parameters.getRawParameterValue("GAIN")->load()
    );

    smoothedPan.reset(sampleRate, 0.02);
    smoothedPan.setCurrentAndTargetValue(
        parameters.getRawParameterValue("PAN")->load() / 100.0f
    );

    // Saturation drive smoothing  (50 ms ramp — eliminates zipper/aliasing clicks)
    smoothedDrive.reset(sampleRate, 0.05);
    float initDrive = parameters.getRawParameterValue("SAT_DRIVE")->load() / 100.0f;
    smoothedDrive.setCurrentAndTargetValue(1.0f + initDrive * 9.0f);

    // Sat enable crossfade  (20 ms ramp — eliminates click on toggle)
    smoothedSatMix.reset(sampleRate, 0.02);
    bool initEnabled = parameters.getRawParameterValue("SAT_ENABLED")->load() >= 0.5f;
    int  initMode    = static_cast<int>(parameters.getRawParameterValue("SAT_MODE")->load());
    smoothedSatMix.setCurrentAndTargetValue((initEnabled && initMode > 0) ? 1.0f : 0.0f);

    // Peak-envelope follower for auto-gain compensation
    //   5 ms attack  — fast enough to catch transients
    // 150 ms release — slow enough to avoid pumping
    satInputEnvelope   = 0.0f;
    satEnvAttackCoeff  = 1.0f - static_cast<float>(std::exp(-1.0 / (sampleRate * 0.005)));
    satEnvReleaseCoeff = 1.0f - static_cast<float>(std::exp(-1.0 / (sampleRate * 0.150)));
}

void EnzoGainAudioProcessor::releaseResources()
{
}

void EnzoGainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // ── Read parameters (atomic reads, real-time safe) ───────────────
    float gainLinear  = parameters.getRawParameterValue("GAIN")->load();
    float lfoStrength = parameters.getRawParameterValue("LFO_STRENGTH")->load() / 100.0f;
    float lfoFreq     = parameters.getRawParameterValue("LFO_FREQ")->load();
    bool  lfoEnabled  = parameters.getRawParameterValue("LFO_ENABLED")->load() >= 0.5f;
    float panValue    = parameters.getRawParameterValue("PAN")->load() / 100.0f;
    int   satMode     = static_cast<int>(parameters.getRawParameterValue("SAT_MODE")->load());
    bool  satEnabled  = parameters.getRawParameterValue("SAT_ENABLED")->load() >= 0.5f;
    float satDrive    = parameters.getRawParameterValue("SAT_DRIVE")->load() / 100.0f;
    float driveTarget = 1.0f + satDrive * 9.0f;       // 1× … 10×

    // ── Update smoothed targets ──────────────────────────────────────
    smoothedGain.setTargetValue(gainLinear);
    smoothedPan.setTargetValue(panValue);
    smoothedDrive.setTargetValue(driveTarget);
    smoothedSatMix.setTargetValue((satEnabled && satMode > 0) ? 1.0f : 0.0f);

    // LFO phase increment per sample
    double phaseIncrement = lfoFreq / currentSampleRate;

    int numChannels = buffer.getNumChannels();
    int numSamples  = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // ── Per-sample smoothed values ───────────────────────────────
        float baseGain  = smoothedGain.getNextValue();
        float driveMult = smoothedDrive.getNextValue();
        float satMix    = smoothedSatMix.getNextValue();  // 0 = dry, 1 = sat

        // ── LFO modulation ───────────────────────────────────────────
        float finalGain = baseGain;
        if (lfoEnabled)
        {
            float lfoValue = static_cast<float>(
                std::sin(2.0 * juce::MathConstants<double>::pi * lfoPhase));
            float lfoMod = 1.0f - lfoStrength
                         + lfoStrength * (lfoValue * 0.5f + 0.5f);
            finalGain = baseGain * lfoMod;
        }
        lfoPhase += phaseIncrement;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        // ── Equal-power panning ──────────────────────────────────────
        float pan   = smoothedPan.getNextValue();
        float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        float leftGain  = std::cos(angle);
        float rightGain = std::sin(angle);

        // ── Read samples ─────────────────────────────────────────────
        if (numChannels >= 2)
        {
            float sampleL = buffer.getWritePointer(0)[sample];
            float sampleR = buffer.getWritePointer(1)[sample];

            // ── Saturation with auto-gain compensation ───────────────
            if (satMix > 0.0001f)
            {
                // 1. Track input peak envelope  (fast attack / slow release)
                float inputPeak = std::max(std::abs(sampleL), std::abs(sampleR));
                if (inputPeak > satInputEnvelope)
                    satInputEnvelope += satEnvAttackCoeff  * (inputPeak - satInputEnvelope);
                else
                    satInputEnvelope += satEnvReleaseCoeff * (inputPeak - satInputEnvelope);

                // 2. Auto-gain: what would the waveshaper output at the
                //    current envelope level?  Compensate to keep peaks steady.
                float satComp = 1.0f;
                if (satInputEnvelope > 0.002f)
                {
                    // For Fold mode, use bounded peak estimate (avoids
                    // zero-crossing compensation spikes)
                    float envDriven;
                    if (satMode == 4)
                        envDriven = std::min(satInputEnvelope * driveMult, 1.0f);
                    else
                        envDriven = std::abs(
                            applySaturation(satInputEnvelope * driveMult, satMode));
                    if (envDriven > 0.0001f)
                        satComp = juce::jlimit(0.1f, 4.0f,
                                               satInputEnvelope / envDriven);
                }

                // 3. Compute wet (saturated + compensated) signal
                float wetL = applySaturation(sampleL * driveMult, satMode) * satComp;
                float wetR = applySaturation(sampleR * driveMult, satMode) * satComp;

                // 4. Crossfade dry ↔ wet  (smoothed satMix avoids click)
                sampleL = sampleL * (1.0f - satMix) + wetL * satMix;
                sampleR = sampleR * (1.0f - satMix) + wetR * satMix;
            }

            // ── Gain + panning ───────────────────────────────────────
            buffer.getWritePointer(0)[sample] = sampleL * finalGain * leftGain;
            buffer.getWritePointer(1)[sample] = sampleR * finalGain * rightGain;

            for (int channel = 2; channel < numChannels; ++channel)
                buffer.getWritePointer(channel)[sample] *= finalGain;
        }
        else
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                float s = buffer.getWritePointer(channel)[sample];

                if (satMix > 0.0001f)
                {
                    float inputPeak = std::abs(s);
                    if (inputPeak > satInputEnvelope)
                        satInputEnvelope += satEnvAttackCoeff  * (inputPeak - satInputEnvelope);
                    else
                        satInputEnvelope += satEnvReleaseCoeff * (inputPeak - satInputEnvelope);

                    float satComp = 1.0f;
                    if (satInputEnvelope > 0.002f)
                    {
                        float envDriven;
                        if (satMode == 4)
                            envDriven = std::min(satInputEnvelope * driveMult, 1.0f);
                        else
                            envDriven = std::abs(
                                applySaturation(satInputEnvelope * driveMult, satMode));
                        if (envDriven > 0.0001f)
                            satComp = juce::jlimit(0.1f, 4.0f,
                                                   satInputEnvelope / envDriven);
                    }

                    float wet = applySaturation(s * driveMult, satMode) * satComp;
                    s = s * (1.0f - satMix) + wet * satMix;
                }

                buffer.getWritePointer(channel)[sample] = s * finalGain;
            }
        }
    }
}

juce::AudioProcessorEditor* EnzoGainAudioProcessor::createEditor()
{
    return new EnzoGainAudioProcessorEditor(*this);
}

void EnzoGainAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EnzoGainAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// Factory function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EnzoGainAudioProcessor();
}
