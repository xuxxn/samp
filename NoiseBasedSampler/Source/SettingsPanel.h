/*
SettingsPanel.h - SIMPLIFIED
✅ No "Load Algorithms" button (auto-scan enabled)
✅ Shows current count (updated automatically)
✅ Cleaner interface
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AlgorithmFileManager.h"

class SettingsPanel : public juce::Component, public juce::Timer
{
public:
    SettingsPanel(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        // Algorithm Path Section
        addAndMakeVisible(algorithmPathLabel);
        algorithmPathLabel.setText("Algorithm Storage Location:",
            juce::dontSendNotification);
        algorithmPathLabel.setFont(juce::Font(14.0f, juce::Font::bold));
        algorithmPathLabel.setColour(juce::Label::textColourId,
            juce::Colours::white);

        addAndMakeVisible(currentPathDisplay);
        currentPathDisplay.setMultiLine(true);
        currentPathDisplay.setReadOnly(true);
        currentPathDisplay.setColour(juce::TextEditor::backgroundColourId,
            juce::Colour(0xff2d2d2d));
        currentPathDisplay.setColour(juce::TextEditor::outlineColourId,
            juce::Colour(0xff4a4a4a));
        currentPathDisplay.setColour(juce::TextEditor::textColourId,
            juce::Colours::white);

        addAndMakeVisible(browseButton);
        browseButton.setButtonText("Browse...");
        browseButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff3b82f6));
        browseButton.onClick = [this] { browseForFolder(); };

        addAndMakeVisible(resetPathButton);
        resetPathButton.setButtonText("Reset to Default");
        resetPathButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff6b7280));
        resetPathButton.onClick = [this] { resetToDefaultPath(); };

        addAndMakeVisible(openFolderButton);
        openFolderButton.setButtonText("Open Folder");
        openFolderButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff10b981));
        openFolderButton.onClick = [this] { openCurrentFolder(); };

        // Info section
        addAndMakeVisible(infoLabel);
        infoLabel.setText(
            "ℹ️ Algorithms are automatically saved to this location.\n"
            "Change this path if you want to store algorithms elsewhere\n"
            "(e.g., a shared network drive or cloud folder).\n\n"
            "✅ Auto-refresh enabled - new files appear automatically!",
            juce::dontSendNotification
        );
        infoLabel.setFont(juce::Font(12.0f));
        infoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        infoLabel.setJustificationType(juce::Justification::topLeft);

        // ✅ Update display every second
        startTimer(1000);

        updatePathDisplay();
    }

    ~SettingsPanel() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));

        // Title
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText("⚙️ SETTINGS", 20, 20, getWidth() - 40, 30,
            juce::Justification::centredLeft);

        // Divider
        g.setColour(juce::Colour(0xff4a4a4a));
        g.drawLine(20, 55, getWidth() - 20, 55, 2.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        area.removeFromTop(60); // Skip title

        // Algorithm Path Section
        algorithmPathLabel.setBounds(area.removeFromTop(25));
        area.removeFromTop(10);

        currentPathDisplay.setBounds(area.removeFromTop(100));
        area.removeFromTop(15);

        auto buttonArea = area.removeFromTop(40);
        browseButton.setBounds(buttonArea.removeFromLeft(120));
        buttonArea.removeFromLeft(10);
        resetPathButton.setBounds(buttonArea.removeFromLeft(150));
        buttonArea.removeFromLeft(10);
        openFolderButton.setBounds(buttonArea.removeFromLeft(120));

        area.removeFromTop(20);
        infoLabel.setBounds(area.removeFromTop(120));
    }

    void timerCallback() override
    {
        updatePathDisplay();
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;

    juce::Label algorithmPathLabel;
    juce::TextEditor currentPathDisplay;
    juce::TextButton browseButton;
    juce::TextButton resetPathButton;
    juce::TextButton openFolderButton;
    juce::Label infoLabel;

    void updatePathDisplay()
    {
        auto& fileManager = processor.getAlgorithmFileManager();
        juce::File currentPath = fileManager.getAlgorithmsFolder();

        juce::String displayText = currentPath.getFullPathName();

        if (fileManager.isUsingCustomPath())
        {
            displayText += "\n\n(Custom location)";
        }
        else
        {
            displayText += "\n\n(Default location)";
        }

        // ✅ Always show current count (updated automatically)
        int count = fileManager.getNumAlgorithms();
        displayText += "\n" + juce::String(count) +
            " algorithm" + (count != 1 ? "s" : "") + " available";

        currentPathDisplay.setText(displayText);
    }

    void browseForFolder()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select folder for algorithm storage...",
            processor.getAlgorithmFileManager().getAlgorithmsFolder(),
            "*");

        auto flags = juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectDirectories;

        chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();

                if (folder.isDirectory())
                {
                    processor.getAlgorithmFileManager().setCustomAlgorithmsPath(folder);
                    updatePathDisplay();

                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::InfoIcon,
                        "✅ Path Changed",
                        "Algorithm storage location updated!\n\n"
                        "New algorithms will be saved to:\n" +
                        folder.getFullPathName() +
                        "\n\nAuto-scan will refresh the list.",
                        "OK"
                    );
                }
            });
    }

    void resetToDefaultPath()
    {
        auto defaultPath = AlgorithmFileManager::getDefaultAlgorithmsFolder();

        processor.getAlgorithmFileManager().setCustomAlgorithmsPath(defaultPath);
        updatePathDisplay();

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "✅ Path Reset",
            "Algorithm storage location reset to default:\n" +
            defaultPath.getFullPathName() +
            "\n\nSettings saved.",
            "OK"
        );
    }

    void openCurrentFolder()
    {
        auto folder = processor.getAlgorithmFileManager().getAlgorithmsFolder();
        folder.revealToUser();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};