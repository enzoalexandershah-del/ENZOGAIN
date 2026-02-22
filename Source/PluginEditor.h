#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

/**
 * WebView-based Plugin Editor for EnzoGain
 *
 * CRITICAL: Member order prevents release build crashes.
 *
 * Destruction order (reverse of declaration):
 * 1. Attachment destroyed FIRST (stops using relay and WebView)
 * 2. WebView destroyed SECOND (safe, attachment is gone)
 * 3. Relay destroyed LAST (safe, nothing using it)
 */

class EnzoGainAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit EnzoGainAudioProcessorEditor(EnzoGainAudioProcessor& p);
    ~EnzoGainAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    std::optional<juce::WebBrowserComponent::Resource> getResource(
        const juce::String& url
    );

    EnzoGainAudioProcessor& processorRef;

    static constexpr int kWidth           = 340;
    static constexpr int kCollapsedHeight = 350;
    static constexpr int kExpandedHeight  = 545;

    // ========================================================================
    // CRITICAL MEMBER DECLARATION ORDER: Relays -> WebView -> Attachments
    // ========================================================================

    // 1. RELAYS FIRST (created first, destroyed last)
    std::unique_ptr<juce::WebSliderRelay> gainRelay;
    std::unique_ptr<juce::WebSliderRelay> panRelay;
    std::unique_ptr<juce::WebSliderRelay> lfoStrengthRelay;
    std::unique_ptr<juce::WebSliderRelay> lfoFreqRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> lfoEnabledRelay;
    std::unique_ptr<juce::WebSliderRelay> satModeRelay;
    std::unique_ptr<juce::WebToggleButtonRelay> satEnabledRelay;
    std::unique_ptr<juce::WebSliderRelay> satDriveRelay;

    // 2. WEBVIEW SECOND (created after relays, destroyed before relays)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS LAST (created last, destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> gainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> panAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lfoStrengthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> lfoFreqAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> lfoEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> satModeAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> satEnabledAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> satDriveAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnzoGainAudioProcessorEditor)
};
