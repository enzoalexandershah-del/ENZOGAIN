#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
// Constructor - CRITICAL: Initialize in correct order
//==============================================================================

EnzoGainAudioProcessorEditor::EnzoGainAudioProcessorEditor(EnzoGainAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    // STEP 1: CREATE RELAYS (before WebView!)
    gainRelay = std::make_unique<juce::WebSliderRelay>("GAIN");
    panRelay = std::make_unique<juce::WebSliderRelay>("PAN");
    lfoStrengthRelay = std::make_unique<juce::WebSliderRelay>("LFO_STRENGTH");
    lfoFreqRelay = std::make_unique<juce::WebSliderRelay>("LFO_FREQ");
    lfoEnabledRelay = std::make_unique<juce::WebToggleButtonRelay>("LFO_ENABLED");
    satModeRelay = std::make_unique<juce::WebSliderRelay>("SAT_MODE");
    satEnabledRelay = std::make_unique<juce::WebToggleButtonRelay>("SAT_ENABLED");
    satDriveRelay = std::make_unique<juce::WebSliderRelay>("SAT_DRIVE");

    // STEP 2: CREATE WEBVIEW (with relay options)
    webView = std::make_unique<juce::WebBrowserComponent>(
        juce::WebBrowserComponent::Options{}
            .withNativeIntegrationEnabled()
            .withResourceProvider([this](const auto& url) {
                return getResource(url);
            })
            .withKeepPageLoadedWhenBrowserIsHidden()
            .withOptionsFrom(*gainRelay)
            .withOptionsFrom(*panRelay)
            .withOptionsFrom(*lfoStrengthRelay)
            .withOptionsFrom(*lfoFreqRelay)
            .withOptionsFrom(*lfoEnabledRelay)
            .withOptionsFrom(*satModeRelay)
            .withOptionsFrom(*satEnabledRelay)
            .withOptionsFrom(*satDriveRelay)
    );

    // STEP 3: CREATE PARAMETER ATTACHMENTS (after WebView!)
    gainAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("GAIN"),
        *gainRelay,
        nullptr
    );

    panAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("PAN"),
        *panRelay,
        nullptr
    );

    lfoStrengthAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("LFO_STRENGTH"),
        *lfoStrengthRelay,
        nullptr
    );

    lfoFreqAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("LFO_FREQ"),
        *lfoFreqRelay,
        nullptr
    );

    lfoEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processorRef.parameters.getParameter("LFO_ENABLED"),
        *lfoEnabledRelay,
        nullptr
    );

    satModeAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("SAT_MODE"),
        *satModeRelay,
        nullptr
    );

    satEnabledAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment>(
        *processorRef.parameters.getParameter("SAT_ENABLED"),
        *satEnabledRelay,
        nullptr
    );

    satDriveAttachment = std::make_unique<juce::WebSliderParameterAttachment>(
        *processorRef.parameters.getParameter("SAT_DRIVE"),
        *satDriveRelay,
        nullptr
    );

    // Listen for LFO_ENABLED changes to resize the window
    processorRef.parameters.addParameterListener("LFO_ENABLED", this);

    // Navigate to root (loads index.html via resource provider)
    webView->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    addAndMakeVisible(*webView);

    // Set initial size based on LFO_ENABLED state
    bool lfoOn = processorRef.parameters.getRawParameterValue("LFO_ENABLED")->load() >= 0.5f;
    setSize(kWidth, lfoOn ? kExpandedHeight : kCollapsedHeight);
    setResizable(false, false);
}

//==============================================================================
// Destructor
//==============================================================================

EnzoGainAudioProcessorEditor::~EnzoGainAudioProcessorEditor()
{
    processorRef.parameters.removeParameterListener("LFO_ENABLED", this);
    // Members automatically destroyed in reverse order:
    // 1. Attachments (stop calling evaluateJavascript)
    // 2. webView (safe, attachments are gone)
    // 3. Relays (safe, nothing using them)
}

//==============================================================================
// AudioProcessorEditor Overrides
//==============================================================================

void EnzoGainAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void EnzoGainAudioProcessorEditor::resized()
{
    webView->setBounds(getLocalBounds());
}

void EnzoGainAudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "LFO_ENABLED")
    {
        bool enabled = newValue >= 0.5f;
        // Must resize on the message thread — use SafePointer to avoid dangling reference
        juce::Component::SafePointer<EnzoGainAudioProcessorEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis, enabled]() {
            if (safeThis != nullptr)
                safeThis->setSize(kWidth, enabled ? kExpandedHeight : kCollapsedHeight);
        });
    }
}

//==============================================================================
// Resource Provider
//==============================================================================

std::optional<juce::WebBrowserComponent::Resource> EnzoGainAudioProcessorEditor::getResource(
    const juce::String& url
)
{
    auto makeVector = [](const char* data, int size) {
        return std::vector<std::byte>(
            reinterpret_cast<const std::byte*>(data),
            reinterpret_cast<const std::byte*>(data) + size
        );
    };

    if (url == "/" || url == "/index.html") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_html, BinaryData::index_htmlSize),
            juce::String("text/html")
        };
    }

    if (url == "/js/juce/index.js") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::index_js, BinaryData::index_jsSize),
            juce::String("text/javascript")
        };
    }

    if (url == "/js/juce/check_native_interop.js") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::check_native_interop_js, BinaryData::check_native_interop_jsSize),
            juce::String("text/javascript")
        };
    }

    if (url == "/logo.png") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::logo_png, BinaryData::logo_pngSize),
            juce::String("image/png")
        };
    }

    if (url == "/spicy.png") {
        return juce::WebBrowserComponent::Resource {
            makeVector(BinaryData::spicy_png, BinaryData::spicy_pngSize),
            juce::String("image/png")
        };
    }

    // Return empty resource for any unknown URL (e.g. /favicon.ico)
    // Prevents "missing file" errors in FL Studio and other hosts
    return juce::WebBrowserComponent::Resource {
        std::vector<std::byte>{},
        juce::String("text/plain")
    };
}
