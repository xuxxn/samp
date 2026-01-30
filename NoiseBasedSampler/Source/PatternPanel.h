/*
PatternPanel.h - ИСПРАВЛЕН: ИСПОЛЬЗУЕТ deletePatternRemoveSamples
✅ Правильный вызов метода удаления
✅ Передача totalSamples в properties panel для визуализации
✅ Детальное логирование
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "OptimizedPatternAnalyzer.h"
#include "PatternSearchVisualization.h"
#include "TreeMapVisualization.h"
#include "PatternPropertiesPanel.h"

class PatternPanel : public juce::Component,
    public juce::Timer
{
public:
    PatternPanel(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        startTimerHz(30);
        
        // Enable mouse events for child components
        setWantsKeyboardFocus(true);
        setInterceptsMouseClicks(false, true);

        loadStoredPatterns();

        // Visualization component
        addAndMakeVisible(searchVisualization);

        // TreeMap component
        addAndMakeVisible(treeMapViz);

        treeMapViz.onPatternSelected = [this](const TreeMapNode& node)
            {
                // ✅ ИСПРАВЛЕНО: Передаём totalSamples для визуализации
                int totalSamples = processor.hasFeatureData() ?
                    processor.getFeatureData().getNumSamples() : 0;

                propertiesPanel.setPattern(node, totalSamples);

                DBG("Selected pattern #" + juce::String(node.patternId));
            };

        treeMapViz.onPatternDeselected = [this]()
            {
                propertiesPanel.clearSelection();
                DBG("Deselected pattern");
            };

        // Properties panel
        addAndMakeVisible(propertiesPanel);

        // ✅ ИСПРАВЛЕНО: Delete callback использует правильный метод
        propertiesPanel.onDeletePattern = [this](int patternId)
            {
                deletePatternById(patternId);
            };

        // Export & Analyze button
        addAndMakeVisible(exportAndAnalyzeButton);
        exportAndAnalyzeButton.setButtonText("Export Indices & Analyze Patterns");
        exportAndAnalyzeButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff10b981));
        exportAndAnalyzeButton.onClick = [this] { exportAndAnalyzePatterns(); };

        // Cancel button
        addAndMakeVisible(cancelButton);
        cancelButton.setButtonText("❌ Cancel Analysis");
        cancelButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xffef4444));
        cancelButton.onClick = [this] { cancelAnalysis(); };
        cancelButton.setVisible(false);

        // Time estimation label
        addAndMakeVisible(timeEstimationLabel);
        timeEstimationLabel.setFont(juce::Font(11.0f, juce::Font::bold));
        timeEstimationLabel.setColour(juce::Label::textColourId, juce::Colour(0xff10b981));
        timeEstimationLabel.setVisible(false);

        // Sort mode selector
        addAndMakeVisible(sortModeLabel);
        sortModeLabel.setText("Sort by:", juce::dontSendNotification);
        sortModeLabel.setFont(juce::Font(12.0f, juce::Font::bold));

        addAndMakeVisible(sortModeCombo);
        sortModeCombo.addItem("Occurrences (Most First)", 1);
        sortModeCombo.addItem("Pattern ID", 2);
        sortModeCombo.addItem("Length (Longest First)", 3);
        sortModeCombo.addItem("Avg Value (Highest First)", 4);
        sortModeCombo.setSelectedId(1);
        sortModeCombo.onChange = [this]
            {
                int selected = sortModeCombo.getSelectedId();
                TreeMapVisualization::SortMode mode;

                switch (selected)
                {
                case 1: mode = TreeMapVisualization::SortMode::ByOccurrences; break;
                case 2: mode = TreeMapVisualization::SortMode::ById; break;
                case 3: mode = TreeMapVisualization::SortMode::ByLength; break;
                case 4: mode = TreeMapVisualization::SortMode::ByAvgValue; break;
                default: mode = TreeMapVisualization::SortMode::ByOccurrences; break;
                }

                treeMapViz.setSortMode(mode);
            };

        // Settings
        addAndMakeVisible(indexTypeLabel);
        indexTypeLabel.setText("Analyze Index:", juce::dontSendNotification);
        indexTypeLabel.setFont(juce::Font(12.0f, juce::Font::bold));

        addAndMakeVisible(indexTypeCombo);
        indexTypeCombo.addItem("Amplitude", 1);
        indexTypeCombo.addItem("Frequency", 2);
        indexTypeCombo.addItem("Phase", 3);
        indexTypeCombo.addItem("Volume", 4);
        indexTypeCombo.addItem("Pan", 5);
        indexTypeCombo.setSelectedId(1);
        indexTypeCombo.onChange = [this] { updateAllRanges(); };

        // Min Occurrences
        addAndMakeVisible(minOccurrencesLabel);
        minOccurrencesLabel.setText("Min Occurrences:", juce::dontSendNotification);
        minOccurrencesLabel.setFont(juce::Font(11.0f));

        addAndMakeVisible(minOccurrencesSlider);
        minOccurrencesSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        minOccurrencesSlider.setRange(2, 1000, 1);
        minOccurrencesSlider.setValue(15);
        minOccurrencesSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);

        // Tolerance
        addAndMakeVisible(toleranceLabel);
        toleranceLabel.setText("Tolerance:", juce::dontSendNotification);
        toleranceLabel.setFont(juce::Font(11.0f));

        addAndMakeVisible(toleranceSlider);
        toleranceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        toleranceSlider.setRange(0.001, 0.1, 0.001);
        toleranceSlider.setValue(0.01);
        toleranceSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

        // Min Pattern Length
        addAndMakeVisible(minPatternLengthLabel);
        minPatternLengthLabel.setText("Min Pattern Length:", juce::dontSendNotification);
        minPatternLengthLabel.setFont(juce::Font(11.0f));

        addAndMakeVisible(minPatternLengthSlider);
        minPatternLengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        minPatternLengthSlider.setRange(2, 100, 1);
        minPatternLengthSlider.setValue(2);
        minPatternLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        minPatternLengthSlider.onValueChange = [this]
            {
                if (maxPatternLengthSlider.getValue() < minPatternLengthSlider.getValue())
                    maxPatternLengthSlider.setValue(minPatternLengthSlider.getValue());

                updateTimeEstimation();
            };

        // Max Pattern Length
        addAndMakeVisible(maxPatternLengthLabel);
        maxPatternLengthLabel.setText("Max Pattern Length:", juce::dontSendNotification);
        maxPatternLengthLabel.setFont(juce::Font(11.0f));

        addAndMakeVisible(maxPatternLengthSlider);
        maxPatternLengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        maxPatternLengthSlider.setRange(2, 100, 1);
        maxPatternLengthSlider.setValue(10);
        maxPatternLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        maxPatternLengthSlider.onValueChange = [this]
            {
                if (maxPatternLengthSlider.getValue() < minPatternLengthSlider.getValue())
                    minPatternLengthSlider.setValue(maxPatternLengthSlider.getValue());

                updateTimeEstimation();
            };

        updateAllRanges();
        updateTimeEstimation();
    }

    ~PatternPanel() override
    {
        if (isAnalyzing)
        {
            shouldCancelAnalysis = true;
            juce::Thread::sleep(100);
        }
    }

    void visibilityChanged() override
    {
        if (isVisible())
        {
            loadStoredPatterns();
            updateAllRanges();
            updateTimeEstimation();
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0a0a0a));

        // Title
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText("🔍 Pattern Detection", 20, 20, 300, 30,
            juce::Justification::centredLeft);

        // Cache hint
        if (!isAnalyzing && processor.hasFeatureData())
        {
            g.setColour(juce::Colours::grey);
            g.setFont(10.0f);

            if (processor.needsReexport())
            {
                g.drawText("⚠️ Features modified - export needed",
                    350, 25, 300, 20, juce::Justification::centredLeft);
            }
            else if (processor.hasStoredPatterns())
            {
                g.drawText("✓ Using cached export data",
                    350, 25, 300, 20, juce::Justification::centredLeft);
            }
        }

        // Warning for large datasets
        if (!isAnalyzing && estimatedAnalysisTime > 30.0)
        {
            g.setColour(juce::Colour(0xfff59e0b));
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("⚠️ Large dataset - analysis may take " +
                formatTime(estimatedAnalysisTime),
                getLocalBounds().withY(450).withHeight(20),
                juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        area.removeFromTop(60);

        // Controls area
        auto controlArea = area.removeFromTop(140);

        // Row 1
        auto row1 = controlArea.removeFromTop(40);
        exportAndAnalyzeButton.setBounds(row1.removeFromLeft(280).withHeight(35));
        row1.removeFromLeft(10);

        if (isAnalyzing)
        {
            cancelButton.setBounds(row1.removeFromLeft(140).withHeight(35));
            row1.removeFromLeft(10);
        }

        sortModeLabel.setBounds(row1.removeFromLeft(60));
        sortModeCombo.setBounds(row1.removeFromLeft(200).withHeight(30));

        row1.removeFromLeft(10);
        indexTypeLabel.setBounds(row1.removeFromLeft(110));
        indexTypeCombo.setBounds(row1.removeFromLeft(120).withHeight(30));

        controlArea.removeFromTop(10);

        // Row 2
        auto row2 = controlArea.removeFromTop(35);
        minOccurrencesLabel.setBounds(row2.removeFromLeft(125));
        minOccurrencesSlider.setBounds(row2.removeFromLeft(150).withHeight(30));
        row2.removeFromLeft(20);
        toleranceLabel.setBounds(row2.removeFromLeft(80));
        toleranceSlider.setBounds(row2.removeFromLeft(150).withHeight(30));

        controlArea.removeFromTop(5);

        // Row 3
        auto row3 = controlArea.removeFromTop(35);
        minPatternLengthLabel.setBounds(row3.removeFromLeft(140));
        minPatternLengthSlider.setBounds(row3.removeFromLeft(150).withHeight(30));
        row3.removeFromLeft(20);
        maxPatternLengthLabel.setBounds(row3.removeFromLeft(140));
        maxPatternLengthSlider.setBounds(row3.removeFromLeft(150).withHeight(30));

        area.removeFromTop(10);

        // Time estimation
        if (timeEstimationLabel.isVisible())
        {
            timeEstimationLabel.setBounds(area.removeFromTop(25).reduced(5));
            area.removeFromTop(5);
        }

        // Visualization (during analysis)
        if (isAnalyzing && searchVisualization.isVisible())
        {
            searchVisualization.setBounds(area.removeFromTop(200).reduced(5));
            area.removeFromTop(10);
        }

        // Main content
        auto propertiesArea = area.removeFromRight(300);
        propertiesPanel.setBounds(propertiesArea.reduced(5));

        area.removeFromRight(10);
        treeMapViz.setBounds(area.reduced(5));
    }

    void timerCallback() override
    {
        bool shouldShowViz = isAnalyzing;
        bool shouldShowCancel = isAnalyzing;
        bool shouldShowTime = isAnalyzing && estimatedAnalysisTime > 0.0;

        if (searchVisualization.isVisible() != shouldShowViz ||
            cancelButton.isVisible() != shouldShowCancel ||
            timeEstimationLabel.isVisible() != shouldShowTime)
        {
            searchVisualization.setVisible(shouldShowViz);
            cancelButton.setVisible(shouldShowCancel);
            timeEstimationLabel.setVisible(shouldShowTime);
            resized();
        }

        repaint();
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;
    OptimizedPatternAnalyzer analyzer;

    // UI Components
    PatternSearchVisualization searchVisualization;
    TreeMapVisualization treeMapViz;
    PatternPropertiesPanel propertiesPanel;

    juce::TextButton exportAndAnalyzeButton;
    juce::TextButton cancelButton;

    juce::Label sortModeLabel;
    juce::ComboBox sortModeCombo;

    juce::Label indexTypeLabel;
    juce::ComboBox indexTypeCombo;

    juce::Label minOccurrencesLabel;
    juce::Slider minOccurrencesSlider;

    juce::Label toleranceLabel;
    juce::Slider toleranceSlider;

    juce::Label minPatternLengthLabel;
    juce::Slider minPatternLengthSlider;

    juce::Label maxPatternLengthLabel;
    juce::Slider maxPatternLengthSlider;

    juce::Label timeEstimationLabel;

    // State
    bool isAnalyzing = false;
    std::atomic<bool> shouldCancelAnalysis{ false };
    double estimatedAnalysisTime = 0.0;
    int currentAnalyzedIndex = 0;

    juce::String formatTime(double seconds) const
    {
        if (seconds < 1.0)
            return "< 1 second";

        if (seconds < 60.0)
            return juce::String(static_cast<int>(seconds)) + " seconds";

        int minutes = static_cast<int>(seconds / 60.0);
        int secs = static_cast<int>(seconds) % 60;

        if (minutes < 60)
            return juce::String(minutes) + "m " + juce::String(secs) + "s";

        int hours = minutes / 60;
        minutes = minutes % 60;

        return juce::String(hours) + "h " + juce::String(minutes) + "m";
    }

    void updateTimeEstimation()
    {
        if (!processor.hasFeatureData() || isAnalyzing)
            return;

        std::vector<float> indexData = extractIndexData();
        if (indexData.empty())
        {
            estimatedAnalysisTime = 0.0;
            return;
        }

        OptimizedPatternAnalyzer::AnalysisSettings settings;
        settings.minPatternLength = static_cast<int>(minPatternLengthSlider.getValue());
        settings.maxPatternLength = static_cast<int>(maxPatternLengthSlider.getValue());
        settings.minOccurrences = static_cast<int>(minOccurrencesSlider.getValue());
        settings.tolerance = calculateRelativeTolerance(
            static_cast<float>(toleranceSlider.getValue()));
        settings.enableDownsampling = true;
        settings.maxDataSize = 5000000;

        analyzer.setSettings(settings);
        estimatedAnalysisTime = analyzer.estimateAnalysisTime(indexData);
    }

    void loadStoredPatterns()
    {
        if (processor.hasStoredPatterns())
        {
            auto patterns = processor.getStoredPatterns();
            treeMapViz.setPatterns(patterns);
            DBG("✅ Loaded " + juce::String(patterns.size()) + " patterns from processor");
        }
    }

    void updateAllRanges()
    {
        if (!processor.hasFeatureData())
            return;

        std::vector<float> indexData = extractIndexData();
        if (indexData.empty())
            return;

        int indexLength = static_cast<int>(indexData.size());

        int calculatedMaxPatternLength = (indexLength - 2) / 2;
        calculatedMaxPatternLength = juce::jmax(2, calculatedMaxPatternLength);
        calculatedMaxPatternLength = juce::jmin(1000, calculatedMaxPatternLength);

        maxPatternLengthSlider.setRange(2, calculatedMaxPatternLength, 1);

        int defaultMaxPatternLength = juce::jmin(10, calculatedMaxPatternLength);
        if (maxPatternLengthSlider.getValue() > calculatedMaxPatternLength)
        {
            maxPatternLengthSlider.setValue(defaultMaxPatternLength, juce::dontSendNotification);
        }

        minPatternLengthSlider.setRange(2, calculatedMaxPatternLength, 1);
        if (minPatternLengthSlider.getValue() > calculatedMaxPatternLength)
        {
            minPatternLengthSlider.setValue(2, juce::dontSendNotification);
        }

        int maxOccurrences = indexLength;
        minOccurrencesSlider.setRange(2, maxOccurrences, 1);

        int defaultMinOccurrences = juce::jmin(15, juce::jmax(2, maxOccurrences / 10));
        if (minOccurrencesSlider.getValue() > maxOccurrences)
        {
            minOccurrencesSlider.setValue(defaultMinOccurrences, juce::dontSendNotification);
        }

        updateTimeEstimation();
    }

    void cancelAnalysis()
    {
        shouldCancelAnalysis = true;
        DBG("🛑 User requested analysis cancellation");

        cancelButton.setButtonText("⏳ Cancelling...");
        cancelButton.setEnabled(false);
    }

    void exportAndAnalyzePatterns()
    {
        if (!processor.hasFeatureData())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "⚠️ No Data",
                "Please load a sample first.",
                "OK"
            );
            return;
        }

        if (processor.needsReexport())
        {
            juce::String fileName = "Indices_" +
                juce::String(juce::Time::getCurrentTime().toMilliseconds());

            juce::File baseFile = juce::File::getSpecialLocation(
                juce::File::userDesktopDirectory).getChildFile(fileName);

            processor.exportIndicesAsync(baseFile,
                [this](bool success, juce::String message)
                {
                    if (success)
                    {
                        DBG("✅ Export complete, starting pattern analysis");
                        analyzePatterns();
                    }
                    else
                    {
                        DBG("❌ Export failed: " + message);
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::WarningIcon,
                            "Export Failed",
                            message,
                            "OK"
                        );
                    }
                });
        }
        else
        {
            DBG("✓ Using cached data, skipping export");
            analyzePatterns();
        }
    }

    void analyzePatterns()
    {
        isAnalyzing = true;
        shouldCancelAnalysis = false;

        currentAnalyzedIndex = indexTypeCombo.getSelectedId() - 1;

        treeMapViz.clearPatterns();
        propertiesPanel.clearSelection();

        searchVisualization.reset();
        searchVisualization.setVisible(true);
        cancelButton.setButtonText("❌ Cancel Analysis");
        cancelButton.setEnabled(true);
        cancelButton.setVisible(true);
        timeEstimationLabel.setVisible(true);
        resized();

        OptimizedPatternAnalyzer::AnalysisSettings settings;
        settings.minPatternLength = static_cast<int>(minPatternLengthSlider.getValue());
        settings.maxPatternLength = static_cast<int>(maxPatternLengthSlider.getValue());
        settings.minOccurrences = static_cast<int>(minOccurrencesSlider.getValue());

        float rawTolerance = static_cast<float>(toleranceSlider.getValue());
        settings.tolerance = calculateRelativeTolerance(rawTolerance);
        settings.enableProgressCallback = true;
        settings.enableDownsampling = true;
        settings.maxDataSize = 5000000;
        settings.maxMemoryMB = 512;

        analyzer.setSettings(settings);

        std::vector<float> indexData = extractIndexData();

        if (indexData.empty())
        {
            isAnalyzing = false;
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "⚠️ No Data",
                "Could not extract index data.",
                "OK"
            );
            return;
        }

        DBG("Starting pattern analysis:");
        DBG("  Data points: " + juce::String(indexData.size()));
        DBG("  Min length: " + juce::String(settings.minPatternLength));
        DBG("  Max length: " + juce::String(settings.maxPatternLength));
        DBG("  Estimated time: " + formatTime(estimatedAnalysisTime));

        juce::Thread::launch([this, indexData, settings]() mutable
            {
                auto patterns = analyzer.analyzeIndex(indexData,
                    [this](const PatternSearchProgress& progress) -> bool
                    {
                        juce::MessageManager::callAsync([this, progress]()
                            {
                                searchVisualization.updateProgress(progress);

                                if (progress.newPatternFound)
                                {
                                    treeMapViz.addPattern(progress.lastFoundPattern);
                                    treeMapViz.setProgress(progress.overallProgress);
                                }

                                if (progress.remainingTimeSeconds > 0.0)
                                {
                                    juce::String timeText = "⏱️ Remaining: " +
                                        formatTime(progress.remainingTimeSeconds) +
                                        " (Elapsed: " +
                                        formatTime(progress.elapsedTimeSeconds) + ")";

                                    timeEstimationLabel.setText(timeText,
                                        juce::dontSendNotification);
                                }
                            });

                        return !shouldCancelAnalysis.load();
                    });

                if (shouldCancelAnalysis)
                {
                    juce::MessageManager::callAsync([this]()
                        {
                            isAnalyzing = false;
                            searchVisualization.setVisible(false);
                            cancelButton.setVisible(false);
                            timeEstimationLabel.setVisible(false);
                            resized();
                            repaint();

                            juce::AlertWindow::showMessageBoxAsync(
                                juce::AlertWindow::InfoIcon,
                                "❌ Analysis Cancelled",
                                "Pattern analysis was cancelled by user.",
                                "OK"
                            );
                        });
                    return;
                }

                juce::MessageManager::callAsync([this, patterns]() mutable
                    {
                        isAnalyzing = false;

                        processor.storeFoundPatterns(patterns);
                        treeMapViz.setPatterns(patterns);

                        searchVisualization.setVisible(false);
                        cancelButton.setVisible(false);
                        timeEstimationLabel.setVisible(false);
                        resized();
                        repaint();

                        juce::String message = "Pattern analysis complete!\n\n";
                        message += "Found " + juce::String(patterns.size()) + " patterns\n";

                        if (!patterns.empty())
                        {
                            message += "\nTop 3 most frequent patterns:\n";
                            for (int i = 0; i < juce::jmin(3, static_cast<int>(patterns.size())); ++i)
                            {
                                message += "• Pattern #" + juce::String(patterns[i].patternId) +
                                    ": " + juce::String(patterns[i].occurrenceCount) +
                                    " occurrences\n";
                            }
                        }

                        juce::AlertWindow::showMessageBoxAsync(
                            juce::AlertWindow::InfoIcon,
                            "✅ Analysis Complete",
                            message,
                            "OK"
                        );
                    });
            });
    }

    float calculateRelativeTolerance(float rawTolerance)
    {
        std::vector<float> indexData = extractIndexData();

        if (indexData.empty())
            return rawTolerance;

        float minVal = *std::min_element(indexData.begin(), indexData.end());
        float maxVal = *std::max_element(indexData.begin(), indexData.end());
        float range = maxVal - minVal;

        if (range < 0.0001f)
            return 0.001f;

        return range * rawTolerance;
    }

    std::vector<float> extractIndexData()
    {
        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        if (numSamples == 0)
            return {};

        std::vector<float> data;
        data.reserve(numSamples);

        int selectedIndex = indexTypeCombo.getSelectedId();

        for (int i = 0; i < numSamples; ++i)
        {
            switch (selectedIndex)
            {
            case 1: data.push_back(features[i].amplitude); break;
            case 2: data.push_back(features[i].frequency); break;
            case 3: data.push_back(features[i].phase); break;
            case 4: data.push_back(features[i].volume); break;
            case 5: data.push_back(features[i].pan); break;
            default: data.push_back(features[i].amplitude); break;
            }
        }

        return data;
    }

    // ==========================================================================
    // ✅ ИСПРАВЛЕНО: ИСПОЛЬЗУЕТ deletePatternRemoveSamples
    // ==========================================================================

    void deletePatternById(int patternId)
    {
        DBG("===========================================");
        DBG("🗑️ DELETE PATTERN REQUEST");
        DBG("===========================================");
        DBG("Pattern ID: " + juce::String(patternId));
        DBG("Index type: " + juce::String(currentAnalyzedIndex));

        // ✅ КРИТИЧНО: Используем метод который РЕАЛЬНО удаляет сэмплы
        bool success = processor.deletePatternRemoveSamples(
            patternId,
            currentAnalyzedIndex
        );

        if (!success)
        {
            DBG("❌ Delete failed!");
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Delete Failed",
                "Could not delete pattern #" + juce::String(patternId),
                "OK"
            );
            return;
        }

        DBG("✅ Pattern deleted successfully");

        // Обновляем UI
        auto updatedPatterns = processor.getStoredPatterns();
        treeMapViz.setPatterns(updatedPatterns);
        propertiesPanel.clearSelection();

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "✅ Pattern Deleted",
            "Pattern #" + juce::String(patternId) + " has been deleted.\n\n"
            "All occurrences have been removed from the timeline.\n\n"
            "The sample is now shorter.",
            "OK"
        );

        DBG("===========================================");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternPanel)
};