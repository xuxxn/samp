/*
PatternPropertiesPanel.h - ENHANCED PATTERN POSITION VISUALIZATION
✅ Полный timeline с визуализацией всего индекса
✅ Выделенные регионы для каждого вхождения паттерна
✅ Интерактивные hover tooltips с точной информацией
✅ Масштабирование для больших семплов
✅ Красивая визуализация в стиле DAW
*/

#pragma once
#include <JuceHeader.h>
#include "TreeMapVisualization.h"

class PatternPropertiesPanel : public juce::Component
{
public:
    PatternPropertiesPanel()
    {
        addAndMakeVisible(titleLabel);
        titleLabel.setText("Pattern Properties", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(14.0f, juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        addAndMakeVisible(noSelectionLabel);
        noSelectionLabel.setText("No pattern selected\n\nClick a pattern in the treemap\nto view its properties",
            juce::dontSendNotification);
        noSelectionLabel.setFont(juce::Font(12.0f));
        noSelectionLabel.setJustificationType(juce::Justification::centred);
        noSelectionLabel.setColour(juce::Label::textColourId, juce::Colours::grey);

        // Delete button
        addAndMakeVisible(deleteButton);
        deleteButton.setButtonText("🗑️ Delete Pattern");
        deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffef4444));
        deleteButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        deleteButton.onClick = [this]
            {
                if (!currentNode || !onDeletePattern)
                    return;

                juce::AlertWindow::showOkCancelBox(
                    juce::MessageBoxIconType::QuestionIcon,
                    "Delete Pattern",
                    "Delete pattern #" + juce::String(currentNode->patternId) +
                    "?\n\nThis will remove all " + juce::String(currentNode->occurrences) +
                    " occurrences from the audio.\n\nThis action cannot be undone.",
                    "Delete",
                    "Cancel",
                    this,
                    juce::ModalCallbackFunction::create([this](int result)
                        {
                            if (result == 1 && currentNode && onDeletePattern)
                            {
                                onDeletePattern(currentNode->patternId);
                            }
                        })
                );
            };

        clearSelection();
    }

    void setPattern(const TreeMapNode& node, int totalSamples)
    {
        currentNode = std::make_unique<TreeMapNode>(node);
        this->totalSamples = totalSamples;
        noSelectionLabel.setVisible(false);
        deleteButton.setVisible(true);
        deleteButton.setEnabled(true);

        // Пересчитываем позиции для визуализации
        rebuildPositionData();

        repaint();
        resized();
    }

    void clearSelection()
    {
        currentNode.reset();
        totalSamples = 0;
        positionData.clear();
        noSelectionLabel.setVisible(true);
        deleteButton.setVisible(false);
        hoveredPositionIndex = -1;
        repaint();
    }

    std::function<void(int patternId)> onDeletePattern;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0f0f0f));

        // Border
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRect(getLocalBounds(), 1);

        if (!currentNode)
            return;

        auto area = contentArea;

        // ==========================================================================
        // WAVEFORM PREVIEW
        // ==========================================================================

        auto waveformArea = area.removeFromTop(100);

        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRect(waveformArea);

        if (!currentNode->waveform.empty())
        {
            juce::Path waveformPath;

            float minVal = *std::min_element(currentNode->waveform.begin(),
                currentNode->waveform.end());
            float maxVal = *std::max_element(currentNode->waveform.begin(),
                currentNode->waveform.end());
            float range = maxVal - minVal;
            if (range < 0.0001f) range = 1.0f;

            float padding = 10.0f;
            auto drawArea = waveformArea.toFloat().reduced(padding);

            for (size_t i = 0; i < currentNode->waveform.size(); ++i)
            {
                float x = drawArea.getX() +
                    (i / static_cast<float>(currentNode->waveform.size() - 1)) * drawArea.getWidth();
                float normalized = (currentNode->waveform[i] - minVal) / range;
                float y = drawArea.getBottom() - (normalized * drawArea.getHeight());

                if (i == 0)
                    waveformPath.startNewSubPath(x, y);
                else
                    waveformPath.lineTo(x, y);
            }

            g.setColour(juce::Colour(0xff10b981));
            g.strokePath(waveformPath, juce::PathStrokeType(2.0f));
        }

        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawRect(waveformArea, 1);

        area.removeFromTop(10);

        // ==========================================================================
        // PROPERTIES
        // ==========================================================================

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(12.0f, juce::Font::bold));

        int lineHeight = 22;

        drawProperty(g, area.removeFromTop(lineHeight),
            "Pattern #", juce::String(currentNode->patternId));

        drawProperty(g, area.removeFromTop(lineHeight),
            "Length:", juce::String(currentNode->length) + " samples");

        drawProperty(g, area.removeFromTop(lineHeight),
            "Occurrences:", juce::String(currentNode->occurrences));

        drawProperty(g, area.removeFromTop(lineHeight),
            "Avg Value:", juce::String(currentNode->avgValue, 4));

        area.removeFromTop(15);

        // ==========================================================================
        // ✅ ENHANCED: FULL INDEX TIMELINE WITH PATTERN POSITIONS
        // ==========================================================================

        if (totalSamples > 0 && !positionData.empty())
        {
            // Header
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText("Pattern Occurrences in Full Index Timeline",
                area.removeFromTop(25), juce::Justification::centredLeft);

            area.removeFromTop(5);

            // Main timeline area (larger for better visibility)
            timelineArea = area.removeFromTop(120);

            // Draw full timeline with pattern highlights
            drawFullTimeline(g, timelineArea);

            // Stats below timeline
            area.removeFromTop(5);
            auto statsArea = area.removeFromTop(40);

            g.setColour(juce::Colours::grey);
            g.setFont(juce::Font(10.0f));

            // Calculate coverage percentage
            int totalPatternSamples = currentNode->length * currentNode->occurrences;
            float coveragePercent = (totalPatternSamples * 100.0f) / totalSamples;

            juce::String stats = juce::String::formatted(
                "Coverage: %.2f%% (%d/%d samples) | Occurrences: %d",
                coveragePercent,
                totalPatternSamples,
                totalSamples,
                currentNode->occurrences
            );

            g.drawText(stats, statsArea, juce::Justification::centred);

            // Hover info (if hovering over a pattern)
            if (hoveredPositionIndex >= 0 && hoveredPositionIndex < positionData.size())
            {
                area.removeFromTop(5);
                auto hoverArea = area.removeFromTop(60);

                const auto& pos = positionData[hoveredPositionIndex];

                // Background for hover info
                g.setColour(juce::Colour(0xff10b981).withAlpha(0.1f));
                g.fillRoundedRectangle(hoverArea.toFloat(), 4.0f);

                g.setColour(juce::Colour(0xff10b981));
                g.drawRoundedRectangle(hoverArea.toFloat(), 4.0f, 2.0f);

                hoverArea.reduce(10, 5);

                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(11.0f, juce::Font::bold));
                g.drawText("Occurrence #" + juce::String(hoveredPositionIndex + 1),
                    hoverArea.removeFromTop(20), juce::Justification::centredLeft);

                g.setFont(juce::Font(10.0f));
                g.setColour(juce::Colours::lightgrey);

                juce::String info = juce::String::formatted(
                    "Position: %d - %d samples\nDuration: %.3f seconds",
                    pos.startSample,
                    pos.endSample,
                    (pos.endSample - pos.startSample + 1) / 44100.0f  // Assuming 44.1kHz
                );

                g.drawText(info, hoverArea, juce::Justification::centredLeft);
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);

        // Title
        titleLabel.setBounds(area.removeFromTop(30));
        area.removeFromTop(5);

        // Reserve space for delete button
        auto buttonArea = area.removeFromBottom(40);
        area.removeFromBottom(5);

        // Always set button bounds
        deleteButton.setBounds(buttonArea.reduced(2));

        // No selection label
        noSelectionLabel.setBounds(getLocalBounds().reduced(20));

        // Store remaining area for content
        contentArea = area;
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        if (!currentNode || positionData.empty())
            return;

        // Check if mouse is over timeline
        if (!timelineArea.contains(event.position.toInt()))
        {
            if (hoveredPositionIndex != -1)
            {
                hoveredPositionIndex = -1;
                repaint();
            }
            return;
        }

        // Find which position is being hovered
        int newHoveredIndex = -1;

        for (size_t i = 0; i < positionData.size(); ++i)
        {
            if (positionData[i].bounds.contains(event.position))
            {
                newHoveredIndex = static_cast<int>(i);
                break;
            }
        }

        if (newHoveredIndex != hoveredPositionIndex)
        {
            hoveredPositionIndex = newHoveredIndex;
            repaint();
        }
    }

private:
    std::unique_ptr<TreeMapNode> currentNode;
    juce::Label titleLabel;
    juce::Label noSelectionLabel;
    juce::TextButton deleteButton;
    juce::Rectangle<int> contentArea;
    juce::Rectangle<int> timelineArea;

    int totalSamples = 0;
    int hoveredPositionIndex = -1;

    // Position data for each occurrence
    struct PositionInfo
    {
        int startSample;
        int endSample;
        juce::Rectangle<float> bounds;
    };
    std::vector<PositionInfo> positionData;

    void drawProperty(juce::Graphics& g, juce::Rectangle<int> area,
        const juce::String& label, const juce::String& value)
    {
        g.setColour(juce::Colours::lightgrey);
        g.setFont(juce::Font(11.0f));
        g.drawText(label, area.removeFromLeft(100), juce::Justification::centredLeft);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(value, area, juce::Justification::centredLeft);
    }

    // ==========================================================================
    // ✅ ENHANCED: FULL TIMELINE VISUALIZATION
    // ==========================================================================

    void rebuildPositionData()
    {
        positionData.clear();

        if (!currentNode || totalSamples == 0)
            return;

        for (size_t i = 0; i < currentNode->occurrencePositions.size(); ++i)
        {
            PositionInfo info;
            info.startSample = currentNode->occurrencePositions[i];
            info.endSample = info.startSample + currentNode->length - 1;
            positionData.push_back(info);
        }
    }

    void drawFullTimeline(juce::Graphics& g, juce::Rectangle<int> area)
    {
        if (totalSamples == 0 || !currentNode)
            return;

        auto drawArea = area.reduced(5);

        // ==========================================================================
        // BACKGROUND - Full index representation
        // ==========================================================================

        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(drawArea.toFloat(), 4.0f);

        // Draw subtle grid lines for time markers
        g.setColour(juce::Colour(0xff2a2a2a));
        int numGridLines = 10;
        for (int i = 0; i <= numGridLines; ++i)
        {
            float x = drawArea.getX() + (i / (float)numGridLines) * drawArea.getWidth();
            g.drawLine(x, drawArea.getY(), x, drawArea.getBottom(), 1.0f);
        }

        // ==========================================================================
        // PATTERN OCCURRENCES - Highlighted regions
        // ==========================================================================

        for (size_t i = 0; i < positionData.size(); ++i)
        {
            const auto& pos = positionData[i];

            // Calculate normalized coordinates
            float startNorm = pos.startSample / (float)totalSamples;
            float endNorm = pos.endSample / (float)totalSamples;

            // Convert to pixel coordinates
            float x1 = drawArea.getX() + startNorm * drawArea.getWidth();
            float x2 = drawArea.getX() + endNorm * drawArea.getWidth();
            float width = juce::jmax(2.0f, x2 - x1);  // Minimum 2 pixels

            auto regionBounds = juce::Rectangle<float>(
                x1,
                drawArea.getY(),
                width,
                drawArea.getHeight()
            );

            // Store bounds for hover detection
            positionData[i].bounds = regionBounds;

            // Color: green for normal, yellow for hover
            bool isHovered = (static_cast<int>(i) == hoveredPositionIndex);

            juce::Colour regionColor = isHovered
                ? juce::Colour(0xfff59e0b)  // Yellow
                : juce::Colour(0xff10b981); // Green

            // Draw region with gradient
            juce::ColourGradient gradient(
                regionColor.withAlpha(0.8f),
                regionBounds.getX(), regionBounds.getCentreY(),
                regionColor.withAlpha(0.4f),
                regionBounds.getRight(), regionBounds.getCentreY(),
                false
            );

            g.setGradientFill(gradient);
            g.fillRect(regionBounds);

            // Border
            g.setColour(regionColor.brighter(0.2f));
            g.drawRect(regionBounds, isHovered ? 2.0f : 1.0f);

            // Draw occurrence number if space allows
            if (width > 15.0f)
            {
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::jmin(10.0f, width * 0.15f), juce::Font::bold));
                g.drawText(juce::String(i + 1),
                    regionBounds.reduced(1),
                    juce::Justification::centred);
            }
        }

        // ==========================================================================
        // TIME MARKERS
        // ==========================================================================

        g.setColour(juce::Colours::grey.withAlpha(0.7f));
        g.setFont(juce::Font(9.0f));

        // Start marker
        g.drawText("0",
            juce::Rectangle<int>(area.getX(), area.getBottom() + 2, 40, 12),
            juce::Justification::centredLeft);

        // End marker
        g.drawText(juce::String(totalSamples) + " samples",
            juce::Rectangle<int>(area.getRight() - 80, area.getBottom() + 2, 80, 12),
            juce::Justification::centredRight);

        // Middle markers with time info (assuming 44.1kHz)
        for (int i = 1; i < numGridLines; ++i)
        {
            float normPos = i / (float)numGridLines;
            int samplePos = static_cast<int>(normPos * totalSamples);
            float timePos = samplePos / 44100.0f;  // Convert to seconds

            float x = drawArea.getX() + normPos * drawArea.getWidth();

            g.drawText(juce::String(timePos, 2) + "s",
                juce::Rectangle<float>(x - 25, area.getBottom() + 2, 50, 12).toNearestInt(),
                juce::Justification::centred);
        }

        // ==========================================================================
        // BORDER
        // ==========================================================================

        g.setColour(juce::Colour(0xff4a4a4a));
        g.drawRoundedRectangle(drawArea.toFloat(), 4.0f, 2.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternPropertiesPanel)
};