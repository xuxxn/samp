/*
AlgorithmSection.h - ALGORITHM PANEL (PLACEHOLDER)
✅ Placeholder for future algorithm functionality
✅ Displays "coming soon" message
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class AlgorithmSection : public juce::Component
{
public:
    AlgorithmSection(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
    }

    ~AlgorithmSection() override
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(bounds, 6.0f);

        // Border
        g.setColour(juce::Colour(0xff4b5563));
        g.drawRoundedRectangle(bounds.reduced(1), 6.0f, 1.5f);

        // Title
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("ALGORITHM", bounds.removeFromTop(30), juce::Justification::centred);

        // Coming soon message
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.setFont(juce::Font(12.0f));
        g.drawText("Coming soon...", bounds, juce::Justification::centred);
    }

    void resized() override
    {
        // Layout managed in paint
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmSection)
};

/*
AlgorithmSection.h - RIGHT ALGORITHM SECTION
âœ… 15 algorithm slots (3Ã—5 grid)
âœ… Algorithm selection
âœ… Apply button

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AlgorithmFileManager.h"

// ========== ALGORITHM SLOT ==========
class AlgorithmSlot : public juce::Component
{
public:
    AlgorithmSlot(int slotIndex, AlgorithmFileManager& manager)
        : slotNumber(slotIndex), fileManager(manager)
    {
        addAndMakeVisible(algorithmSelector);
        algorithmSelector.setTextWhenNothingSelected("Select algorithm...");
        algorithmSelector.onChange = [this] {
            int selectedId = algorithmSelector.getSelectedId();
            hasAlgorithm = (selectedId > 0);
            selectedAlgorithmIndex = selectedId - 1;
            repaint();
            };

        updateAlgorithmList();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        juce::Colour bgColour = hasAlgorithm
            ? juce::Colour(0xff10b981).withAlpha(0.15f)
            : juce::Colour(0xff374151).withAlpha(0.5f);

        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(hasAlgorithm ? juce::Colour(0xff10b981) : juce::Colour(0xff4b5563));
        g.drawRoundedRectangle(bounds.reduced(1), 6.0f, 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(juce::String(slotNumber + 1), bounds.reduced(5).removeFromTop(15),
            juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(5);
        area.removeFromTop(15);
        algorithmSelector.setBounds(area);
    }

    void updateAlgorithmList()
    {
        algorithmSelector.clear();

        int numAlgos = fileManager.getNumAlgorithms();
        for (int i = 0; i < numAlgos; ++i)
        {
            auto* meta = fileManager.getMetadata(i);
            if (meta)
                algorithmSelector.addItem(meta->name, i + 1);
        }
    }

    int getSelectedAlgorithmIndex() const { return selectedAlgorithmIndex; }
    bool hasAlgorithmSelected() const { return hasAlgorithm; }

    void clear()
    {
        algorithmSelector.setSelectedId(0);
        hasAlgorithm = false;
        selectedAlgorithmIndex = -1;
        repaint();
    }

private:
    int slotNumber;
    int selectedAlgorithmIndex = -1;
    bool hasAlgorithm = false;
    AlgorithmFileManager& fileManager;
    juce::ComboBox algorithmSelector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmSlot)
};

// ========== ALGORITHM SECTION ==========
class AlgorithmSection : public juce::Component
{
public:
    static constexpr int kNumAlgorithmSlots = 15;
    static constexpr int kAlgoSlotColumns = 3;

    AlgorithmSection(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        algorithmFileManager = std::make_unique<AlgorithmFileManager>();

        for (int i = 0; i < kNumAlgorithmSlots; ++i)
        {
            algorithmSlots[i] = std::make_unique<AlgorithmSlot>(i, *algorithmFileManager);
            addAndMakeVisible(algorithmSlots[i].get());
        }

        addAndMakeVisible(algorithmPanelLabel);
        algorithmPanelLabel.setText("ALGORITHM SLOTS:", juce::dontSendNotification);
        algorithmPanelLabel.setFont(juce::Font(11.0f, juce::Font::bold));
        algorithmPanelLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));

        addAndMakeVisible(applyAlgorithmsButton);
        applyAlgorithmsButton.setButtonText("APPLY ALGORITHMS TO SAMPLE");
        applyAlgorithmsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8b5cf6));
        applyAlgorithmsButton.onClick = [this] { applySelectedAlgorithms(); };

        algorithmFileManager->onMetadataChanged = [this]()
            {
                juce::MessageManager::callAsync([this]()
                    {
                        for (auto& slot : algorithmSlots)
                            slot->updateAlgorithmList();
                    });
            };
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff111827).withAlpha(0.90f));
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(juce::Colour(0xff374151).withAlpha(0.9f));
        g.drawRoundedRectangle(r.reduced(1.0f), 10.0f, 2.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10).withTrimmedTop(5);

        algorithmPanelLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(5);

        applyAlgorithmsButton.setBounds(area.removeFromBottom(35));
        area.removeFromBottom(5);

        const int cols = kAlgoSlotColumns;
        const int rows = (kNumAlgorithmSlots + cols - 1) / cols;

        const int gapX = 10;
        const int gapY = 10;
        const int slotH = 78;

        const int totalGapX = gapX * (cols - 1);
        const int colW = (area.getWidth() - totalGapX) / cols;

        for (int i = 0; i < kNumAlgorithmSlots; ++i)
        {
            const int col = i % cols;
            const int row = i / cols;

            const int x = area.getX() + col * (colW + gapX);
            const int y = area.getY() + row * (slotH + gapY);

            algorithmSlots[i]->setBounds(x, y, colW, slotH);
        }
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;

    std::unique_ptr<AlgorithmFileManager> algorithmFileManager;
    std::array<std::unique_ptr<AlgorithmSlot>, kNumAlgorithmSlots> algorithmSlots;

    juce::TextButton applyAlgorithmsButton;
    juce::Label algorithmPanelLabel;

    void applySelectedAlgorithms()
    {
        if (!processor.hasSampleLoaded())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "âš ï¸ No Sample",
                "Please load a sample first.",
                "OK");
            return;
        }

        int appliedCount = 0;

        for (int i = 0; i < kNumAlgorithmSlots; ++i)
        {
            if (!algorithmSlots[i]->hasAlgorithmSelected())
                continue;

            int algoIndex = algorithmSlots[i]->getSelectedAlgorithmIndex();
            auto algo = algorithmFileManager->loadFullAlgorithm(algoIndex);

            if (!algo || !algo->isValid())
                continue;

            DBG("ðŸŽ¨ Applying algorithm from slot " + juce::String(i + 1) + ": " + algo->metadata.name);

            processor.applyAlgorithmToSample(*algo);
            appliedCount++;
        }

        if (appliedCount > 0)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "âœ… Applied",
                juce::String(appliedCount) + " algorithm(s) applied successfully!",
                "OK");

            repaint();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "âš ï¸ No Algorithms Selected",
                "Please select at least one algorithm to apply.",
                "OK");
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmSection)
};
*/