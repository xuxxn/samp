/*
UIStateManager.h - UPDATED WITH ALGOLAB STATE
Централизованное хранилище UI-состояния для всего плагина.
✅ Добавлена поддержка AlgoLabPanel
*/

#pragma once
#include <JuceHeader.h>

// ==========================================================================
// UI STATE MANAGER
// ==========================================================================

class UIStateManager
{
public:
    UIStateManager() = default;

    // ==========================================================================
    // UI STATE PROPERTIES
    // ==========================================================================

    // Tabs
    int selectedTab = 0;

    // MainPanel state
    int selectedChartType = 0;
    int selectedEditTool = 0;
    float horizontalZoom = 1.0f;
    float verticalZoom = 1.0f;
    float panOffsetX = 0.0f;
    float panOffsetY = 0.0f;
    bool showClipboard = true;
    int clipboardSlot = 0;

    // Region selection state
    bool hasRegionSelection = false;
    int regionStartSample = 0;
    int regionEndSample = 0;
    bool isRegionFocused = false;

    // PatternPanel state
    int selectedPatternId = -1;
    int patternSortMode = 0;
    int patternIndexType = 0;
    float patternMinOccurrences = 15.0f;
    float patternTolerance = 0.01f;
    int patternMinLength = 2;
    int patternMaxLength = 10;

    // SpectralIndexPanel state
    float spectralHorizontalZoom = 1.0f;
    float spectralVerticalZoom = 1.0f;
    float spectralPanX = 0.0f;
    float spectralPanY = 0.0f;
    int spectralEditTool = 0;
    float spectralToolSize = 5.0f;
    float spectralToolIntensity = 1.0f;

    // ✅ NEW: AlgoLabPanel state
    int algoLabTool = 0;                    // DifferenceLab, SecondTool, ThirdTool
    int selectedAlgorithmSlot = -1;         // Какой алгоритм выбран
    bool algoLabAnalysisComplete = false;   // Завершен ли анализ
    bool algoLabHasCurrentAlgorithm = false;// Есть ли текущий алгоритм

    // ADSRPanel state
    bool adsrEnabled = true;

    // ==========================================================================
    // SERIALIZATION TO/FROM XML
    // ==========================================================================

    juce::XmlElement* toXml() const
    {
        auto* xml = new juce::XmlElement("UIState");

        // Tabs
        xml->setAttribute("selectedTab", selectedTab);

        // MainPanel
        xml->setAttribute("chartType", selectedChartType);
        xml->setAttribute("editTool", selectedEditTool);
        xml->setAttribute("horizontalZoom", horizontalZoom);
        xml->setAttribute("verticalZoom", verticalZoom);
        xml->setAttribute("panOffsetX", panOffsetX);
        xml->setAttribute("panOffsetY", panOffsetY);
        xml->setAttribute("showClipboard", showClipboard);
        xml->setAttribute("clipboardSlot", clipboardSlot);

        // Region selection
        xml->setAttribute("hasRegion", hasRegionSelection);
        xml->setAttribute("regionStart", regionStartSample);
        xml->setAttribute("regionEnd", regionEndSample);
        xml->setAttribute("regionFocused", isRegionFocused);

        // PatternPanel
        xml->setAttribute("patternId", selectedPatternId);
        xml->setAttribute("patternSort", patternSortMode);
        xml->setAttribute("patternIndex", patternIndexType);
        xml->setAttribute("patternMinOcc", patternMinOccurrences);
        xml->setAttribute("patternTol", patternTolerance);
        xml->setAttribute("patternMinLen", patternMinLength);
        xml->setAttribute("patternMaxLen", patternMaxLength);

        // SpectralIndexPanel
        xml->setAttribute("spectralZoomH", spectralHorizontalZoom);
        xml->setAttribute("spectralZoomV", spectralVerticalZoom);
        xml->setAttribute("spectralPanX", spectralPanX);
        xml->setAttribute("spectralPanY", spectralPanY);
        xml->setAttribute("spectralTool", spectralEditTool);
        xml->setAttribute("spectralSize", spectralToolSize);
        xml->setAttribute("spectralIntensity", spectralToolIntensity);

        // ✅ AlgoLabPanel
        xml->setAttribute("algoTool", algoLabTool);
        xml->setAttribute("algoSlot", selectedAlgorithmSlot);
        xml->setAttribute("algoAnalysisComplete", algoLabAnalysisComplete);
        xml->setAttribute("algoHasCurrentAlg", algoLabHasCurrentAlgorithm);

        // ADSRPanel
        xml->setAttribute("adsrEnabled", adsrEnabled);

        return xml;
    }

    void fromXml(const juce::XmlElement* xml)
    {
        if (xml == nullptr || !xml->hasTagName("UIState"))
            return;

        // Tabs
        selectedTab = xml->getIntAttribute("selectedTab", 0);

        // MainPanel
        selectedChartType = xml->getIntAttribute("chartType", 0);
        selectedEditTool = xml->getIntAttribute("editTool", 0);
        horizontalZoom = static_cast<float>(xml->getDoubleAttribute("horizontalZoom", 1.0));
        verticalZoom = static_cast<float>(xml->getDoubleAttribute("verticalZoom", 1.0));
        panOffsetX = static_cast<float>(xml->getDoubleAttribute("panOffsetX", 0.0));
        panOffsetY = static_cast<float>(xml->getDoubleAttribute("panOffsetY", 0.0));
        showClipboard = xml->getBoolAttribute("showClipboard", true);
        clipboardSlot = xml->getIntAttribute("clipboardSlot", 0);

        // Region selection
        hasRegionSelection = xml->getBoolAttribute("hasRegion", false);
        regionStartSample = xml->getIntAttribute("regionStart", 0);
        regionEndSample = xml->getIntAttribute("regionEnd", 0);
        isRegionFocused = xml->getBoolAttribute("regionFocused", false);

        // PatternPanel
        selectedPatternId = xml->getIntAttribute("patternId", -1);
        patternSortMode = xml->getIntAttribute("patternSort", 0);
        patternIndexType = xml->getIntAttribute("patternIndex", 0);
        patternMinOccurrences = static_cast<float>(xml->getDoubleAttribute("patternMinOcc", 15.0));
        patternTolerance = static_cast<float>(xml->getDoubleAttribute("patternTol", 0.01));
        patternMinLength = xml->getIntAttribute("patternMinLen", 2);
        patternMaxLength = xml->getIntAttribute("patternMaxLen", 10);

        // SpectralIndexPanel
        spectralHorizontalZoom = static_cast<float>(xml->getDoubleAttribute("spectralZoomH", 1.0));
        spectralVerticalZoom = static_cast<float>(xml->getDoubleAttribute("spectralZoomV", 1.0));
        spectralPanX = static_cast<float>(xml->getDoubleAttribute("spectralPanX", 0.0));
        spectralPanY = static_cast<float>(xml->getDoubleAttribute("spectralPanY", 0.0));
        spectralEditTool = xml->getIntAttribute("spectralTool", 0);
        spectralToolSize = static_cast<float>(xml->getDoubleAttribute("spectralSize", 5.0));
        spectralToolIntensity = static_cast<float>(xml->getDoubleAttribute("spectralIntensity", 1.0));

        // ✅ AlgoLabPanel
        algoLabTool = xml->getIntAttribute("algoTool", 0);
        selectedAlgorithmSlot = xml->getIntAttribute("algoSlot", -1);
        algoLabAnalysisComplete = xml->getBoolAttribute("algoAnalysisComplete", false);
        algoLabHasCurrentAlgorithm = xml->getBoolAttribute("algoHasCurrentAlg", false);

        // ADSRPanel
        adsrEnabled = xml->getBoolAttribute("adsrEnabled", true);

        DBG("✅ UI State loaded from XML");
    }

    // ==========================================================================
    // CONVENIENCE METHODS
    // ==========================================================================

    void reset()
    {
        selectedTab = 0;
        selectedChartType = 0;
        selectedEditTool = 0;
        horizontalZoom = 1.0f;
        verticalZoom = 1.0f;
        panOffsetX = 0.0f;
        panOffsetY = 0.0f;
        showClipboard = true;
        clipboardSlot = 0;

        hasRegionSelection = false;
        regionStartSample = 0;
        regionEndSample = 0;
        isRegionFocused = false;

        selectedPatternId = -1;
        patternSortMode = 0;
        patternIndexType = 0;
        patternMinOccurrences = 15.0f;
        patternTolerance = 0.01f;
        patternMinLength = 2;
        patternMaxLength = 10;

        spectralHorizontalZoom = 1.0f;
        spectralVerticalZoom = 1.0f;
        spectralPanX = 0.0f;
        spectralPanY = 0.0f;
        spectralEditTool = 0;
        spectralToolSize = 5.0f;
        spectralToolIntensity = 1.0f;

        algoLabTool = 0;
        selectedAlgorithmSlot = -1;
        algoLabAnalysisComplete = false;
        algoLabHasCurrentAlgorithm = false;

        adsrEnabled = true;

        DBG("🔄 UI State reset to defaults");
    }

    void printState() const
    {
        DBG("===========================================");
        DBG("CURRENT UI STATE:");
        DBG("===========================================");
        DBG("Selected Tab: " + juce::String(selectedTab));
        DBG("Chart Type: " + juce::String(selectedChartType));
        DBG("Horizontal Zoom: " + juce::String(horizontalZoom, 2));
        DBG("Vertical Zoom: " + juce::String(verticalZoom, 2));
        DBG("Has Region: " + juce::String(hasRegionSelection ? "YES" : "NO"));
        DBG("Pattern Selected: " + juce::String(selectedPatternId));
        DBG("AlgoLab Tool: " + juce::String(algoLabTool));
        DBG("AlgoLab Analysis: " + juce::String(algoLabAnalysisComplete ? "COMPLETE" : "INCOMPLETE"));
        DBG("ADSR Enabled: " + juce::String(adsrEnabled ? "YES" : "NO"));
        DBG("===========================================");
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIStateManager)
};