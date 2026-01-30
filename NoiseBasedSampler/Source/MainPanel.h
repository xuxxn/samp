/*
MainPanel.h - FINAL VERSION WITH DRAG-AND-DROP
✅ Waveform display visible (175px height)
✅ Drag-and-drop export button working
✅ CMD terminal (280px height)
✅ Optimal layout for 700x600 window
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CMDTerminalToolsSection.h"
#include "WaveformDisplaySection.h"
#include "AlgorithmSection.h"
#include "SimpleIndexExporter.h"

// ========== EXPORT BUTTON ==========
class ExportButton : public juce::Component
{
public:
    ExportButton(const juce::String& label, std::function<juce::File()> exportCallback)
        : buttonText(label), exportFunction(exportCallback) {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        juce::Colour bgColour = shouldBeEnabled
            ? (isMouseOver ? juce::Colour(0xff9b6dff) : juce::Colour(0xff8b5cf6))
            : juce::Colour(0xff4b5563);

        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds, 4.0f);

        if (shouldBeEnabled && isMouseOver)
        {
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.drawRoundedRectangle(bounds.reduced(1), 4.0f, 2.0f);
        }

        g.setColour(shouldBeEnabled ? juce::Colours::white : juce::Colours::white.withAlpha(0.4f));

        // Export icon (down arrow)
        auto iconBounds = bounds.reduced(8).removeFromLeft(20);
        juce::Path exportIcon;
        float centerX = iconBounds.getCentreX();
        float centerY = iconBounds.getCentreY();
        exportIcon.startNewSubPath(centerX, centerY - 5);
        exportIcon.lineTo(centerX, centerY + 5);
        exportIcon.lineTo(centerX - 3, centerY + 2);
        exportIcon.startNewSubPath(centerX, centerY + 5);
        exportIcon.lineTo(centerX + 3, centerY + 2);
        g.strokePath(exportIcon, juce::PathStrokeType(2.0f));

        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(buttonText, bounds.reduced(5), juce::Justification::centred);
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        isMouseOver = true;
        if (shouldBeEnabled)
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        isMouseOver = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isLeftButtonDown() && shouldBeEnabled)
            isDragging = false;
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!shouldBeEnabled) return;
        if (!isDragging && event.getDistanceFromDragStart() > 5)
        {
            isDragging = true;
            auto exportedFile = exportFunction();
            if (exportedFile.existsAsFile())
            {
                juce::StringArray files;
                files.add(exportedFile.getFullPathName());

                // Perform external drag - parent is DragAndDropContainer
                juce::DragAndDropContainer::performExternalDragDropOfFiles(files, true, this);
            }
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!shouldBeEnabled) return;
        if (!isDragging)
        {
            // Click without drag - reveal file
            auto exportedFile = exportFunction();
            if (exportedFile.existsAsFile())
                exportedFile.revealToUser();
        }
        isDragging = false;
    }

    void setEnabled(bool enabled) { shouldBeEnabled = enabled; repaint(); }
    bool isEnabled() const { return shouldBeEnabled; }

private:
    std::function<juce::File()> exportFunction;
    juce::String buttonText;
    bool isMouseOver = false;
    bool isDragging = false;
    bool shouldBeEnabled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportButton)
};

// ========== BOTTOM BUTTON ==========
class BottomButton : public juce::Component
{
public:
    BottomButton(const juce::String& text) : buttonText(text) {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        juce::Colour bgColour = isMouseOver
            ? juce::Colour(0xff4b5563)
            : juce::Colour(0xff374151);

        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(buttonText, bounds, juce::Justification::centred);
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        isMouseOver = true;
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        isMouseOver = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown() && onClick)
            onClick();
    }

    std::function<void()> onClick;

private:
    juce::String buttonText;
    bool isMouseOver = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BottomButton)
};

// ========== MAIN PANEL ==========
class MainPanel : public juce::Component,
    public juce::Timer,
    public juce::DragAndDropContainer  // For export button drag-and-drop
{
public:
    MainPanel(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        startTimerHz(30);

        // Export button with drag-and-drop
        exportSampleButton = std::make_unique<ExportButton>("export sample",
            [this]() { return exportSampleToTemp(); });
        addAndMakeVisible(exportSampleButton.get());

        // Create sections
        toolsSection = std::make_unique<CMDTerminalToolsSection>(processor);
        addAndMakeVisible(toolsSection.get());

        waveformSection = std::make_unique<WaveformDisplaySection>(processor);
        addAndMakeVisible(waveformSection.get());

        // Link tools section to waveform section
        toolsSection->setWaveformSection(waveformSection.get());

        algorithmSection = std::make_unique<AlgorithmSection>(processor);
        addAndMakeVisible(algorithmSection.get());

        // Bottom buttons
        projectButton = std::make_unique<BottomButton>("project");
        projectButton->onClick = [this] {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Project",
                "Project view coming soon...",
                "OK");
            };
        addAndMakeVisible(projectButton.get());

        settingsButton = std::make_unique<BottomButton>("Settings");
        settingsButton->onClick = [this] {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Settings",
                "Settings panel coming soon...",
                "OK");
            };
        addAndMakeVisible(settingsButton.get());

        patchButton = std::make_unique<BottomButton>("patch");
        patchButton->onClick = [this] {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Patch",
                "Patch view coming soon...",
                "OK");
            };
        addAndMakeVisible(patchButton.get());
    }

    void visibilityChanged() override
    {
        if (isVisible() && toolsSection)
        {
            // Give keyboard focus to CMD terminal automatically
            toolsSection->grabKeyboardFocus();
        }
    }

    void parentHierarchyChanged() override
    {
        if (isVisible() && toolsSection)
        {
            toolsSection->grabKeyboardFocus();
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2d2d2d));
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Top bar - export button on left
        auto topBar = area.removeFromTop(40);
        topBar.removeFromLeft(10);
        topBar.removeFromRight(10);

        exportSampleButton->setBounds(topBar.removeFromLeft(140));

        // Bottom bar - three action buttons
        auto bottomBar = area.removeFromBottom(40);
        bottomBar.removeFromLeft(10);
        bottomBar.removeFromRight(10);

        int buttonWidth = (bottomBar.getWidth() - 20) / 3;
        projectButton->setBounds(bottomBar.removeFromLeft(buttonWidth));
        bottomBar.removeFromLeft(10);
        settingsButton->setBounds(bottomBar.removeFromLeft(buttonWidth));
        bottomBar.removeFromLeft(10);
        patchButton->setBounds(bottomBar.removeFromLeft(buttonWidth));

        // Main content area
        auto contentArea = area.reduced(10);

        // Vertical layout: Waveform (top) + Tools/Algorithm (bottom)
        int toolsHeight = 280;  // CMD terminal height
        int waveformHeight = contentArea.getHeight() - toolsHeight - 10;

        // Waveform display section (now visible!)
        waveformSection->setBounds(contentArea.removeFromTop(waveformHeight));
        contentArea.removeFromTop(10);  // Gap

        // Bottom row: CMD Terminal (left) + Algorithm section (right)
        int toolsWidth = 340;
        int algoWidth = contentArea.getWidth() - toolsWidth - 10;

        auto bottomRow = contentArea;
        toolsSection->setBounds(bottomRow.removeFromLeft(toolsWidth));
        bottomRow.removeFromLeft(10);  // Gap
        algorithmSection->setBounds(bottomRow.removeFromLeft(algoWidth));
    }

    void timerCallback() override
    {
        // Update export button state based on sample loaded
        bool hasData = processor.hasSampleLoaded();
        if (hasData != exportSampleButton->isEnabled())
        {
            exportSampleButton->setEnabled(hasData);
        }
    }

    // Public access for CMDTerminalToolsSection to control edit tools
    CMDTerminalToolsSection* getToolsSection() { return toolsSection.get(); }
    WaveformDisplaySection* getWaveformSection() { return waveformSection.get(); }

private:
    NoiseBasedSamplerAudioProcessor& processor;

    std::unique_ptr<ExportButton> exportSampleButton;
    std::unique_ptr<CMDTerminalToolsSection> toolsSection;
    std::unique_ptr<WaveformDisplaySection> waveformSection;
    std::unique_ptr<AlgorithmSection> algorithmSection;
    std::unique_ptr<BottomButton> projectButton;
    std::unique_ptr<BottomButton> settingsButton;
    std::unique_ptr<BottomButton> patchButton;

    juce::File exportSampleToTemp()
    {
        juce::String fileName = "Sample_" +
            juce::String(juce::Time::getCurrentTime().toMilliseconds()) + ".wav";
        juce::File exportFile = juce::File::getSpecialLocation(
            juce::File::userDesktopDirectory).getChildFile(fileName);

        processor.exportModifiedSample(exportFile);

        if (exportFile.existsAsFile())
        {
            return exportFile;
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "⚠️ Export Failed",
                "Could not export sample.",
                "OK");
            return juce::File();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};