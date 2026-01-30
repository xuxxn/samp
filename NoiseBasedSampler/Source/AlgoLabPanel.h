/*
AlgoLabPanel.h - PRODUCTION VERSION
✅ Virtual list (only visible items rendered)
✅ Auto-refresh (no manual scan button)
✅ Lazy loading (load full algorithm only on click)
✅ No crashes on tab close
✅ Safe lifecycle management
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AlgorithmDNA.h"
#include "DifferenceAnalyzer.h"
#include "AlgorithmEngine.h"
#include "AlgorithmFileManager.h"

// ==========================================================================
// AUDIO DROP ZONE (unchanged)
// ==========================================================================

class AudioDropZone : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    AudioDropZone(const juce::String& labelText) : label(labelText) {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        if (isDraggingOver)
            g.setColour(juce::Colour(0xff3b82f6).withAlpha(0.2f));
        else if (hasAudio)
            g.setColour(juce::Colour(0xff10b981).withAlpha(0.1f));
        else
            g.setColour(juce::Colour(0xff374151));

        g.fillRoundedRectangle(bounds, 6.0f);

        if (isDraggingOver)
            g.setColour(juce::Colour(0xff3b82f6));
        else if (hasAudio)
            g.setColour(juce::Colour(0xff10b981));
        else
            g.setColour(juce::Colour(0xff4b5563));

        g.drawRoundedRectangle(bounds.reduced(1), 6.0f, 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(label, bounds.reduced(10).removeFromTop(20),
            juce::Justification::centredLeft);

        if (hasAudio)
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText(fileName, bounds.reduced(10).removeFromTop(50),
                juce::Justification::centred);

            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.setFont(juce::Font(10.0f));
            juce::String info = juce::String(numSamples) + " samples, " +
                juce::String(sampleRate, 0) + " Hz";
            g.drawText(info, bounds.reduced(10).removeFromBottom(20),
                juce::Justification::centred);
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.setFont(juce::Font(12.0f));
            g.drawText("Drop audio file here", bounds.reduced(10),
                juce::Justification::centred);
        }
    }

    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (auto& file : files)
        {
            if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".mp3") ||
                file.endsWithIgnoreCase(".aif") || file.endsWithIgnoreCase(".aiff"))
                return true;
        }
        return false;
    }

    void fileDragEnter(const juce::StringArray&, int, int) override
    {
        isDraggingOver = true;
        repaint();
    }

    void fileDragExit(const juce::StringArray&) override
    {
        isDraggingOver = false;
        repaint();
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        isDraggingOver = false;
        if (files.size() > 0)
            loadAudioFile(juce::File(files[0]));
    }

    void loadAudioFile(const juce::File& file)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(file));

        if (reader != nullptr)
        {
            audioBuffer.setSize(static_cast<int>(reader->numChannels),
                static_cast<int>(reader->lengthInSamples));

            reader->read(&audioBuffer, 0,
                static_cast<int>(reader->lengthInSamples), 0, true, true);

            fileName = file.getFileName();
            numSamples = audioBuffer.getNumSamples();
            sampleRate = reader->sampleRate;
            hasAudio = true;

            if (onAudioLoaded)
                onAudioLoaded(audioBuffer, sampleRate);

            repaint();
        }
    }

    const juce::AudioBuffer<float>& getAudioBuffer() const { return audioBuffer; }
    bool hasAudioLoaded() const { return hasAudio; }
    double getSampleRate() const { return sampleRate; }

    void clear()
    {
        audioBuffer.setSize(0, 0);
        fileName = "";
        hasAudio = false;
        numSamples = 0;
        repaint();
    }

    std::function<void(const juce::AudioBuffer<float>&, double)> onAudioLoaded;

private:
    juce::String label;
    juce::AudioBuffer<float> audioBuffer;
    juce::String fileName;
    int numSamples = 0;
    double sampleRate = 44100.0;
    bool hasAudio = false;
    bool isDraggingOver = false;
};

// ==========================================================================
// ✅ VIRTUAL ALGORITHM LIST (renders only visible items)
// ==========================================================================

class VirtualAlgorithmList : public juce::Component,
    public juce::ListBoxModel
{
public:
    VirtualAlgorithmList(AlgorithmFileManager& manager)
        : fileManager(manager)
    {
        addAndMakeVisible(listBox);
        listBox.setModel(this);
        listBox.setRowHeight(90);
        listBox.setColour(juce::ListBox::backgroundColourId,
            juce::Colour(0xff1a1a1a));
    }

    ~VirtualAlgorithmList() override
    {
        listBox.setModel(nullptr);
    }

    void refresh()
    {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        listBox.updateContent();
        repaint();
    }

    void resized() override
    {
        listBox.setBounds(getLocalBounds());
    }

    // ==========================================================================
    // ListBoxModel implementation
    // ==========================================================================

    int getNumRows() override
    {
        return fileManager.getNumAlgorithms();
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g,
        int width, int height, bool rowIsSelected) override
    {
        // ✅ Fetch metadata on-demand (lightweight)
        const auto* meta = fileManager.getMetadata(rowNumber);

        if (!meta)
            return;

        auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();

        // Background
        g.setColour(juce::Colour(0xff1f2937));
        g.fillRoundedRectangle(bounds.reduced(5), 6.0f);

        if (rowIsSelected)
            g.setColour(juce::Colour(0xff3b82f6));
        else
            g.setColour(juce::Colour(0xff374151));

        g.drawRoundedRectangle(bounds.reduced(6), 6.0f, 2.0f);

        auto contentArea = bounds.reduced(15);

        // Type badge
        g.setColour(juce::Colour(0xff3b82f6).withAlpha(0.3f));
        auto typeBadge = contentArea.removeFromTop(20);
        g.fillRoundedRectangle(typeBadge.removeFromLeft(80), 3.0f);

        g.setColour(juce::Colour(0xff3b82f6));
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(meta->algorithmType.toUpperCase(),
            typeBadge.removeFromLeft(80).reduced(2),
            juce::Justification::centred);

        contentArea.removeFromTop(5);

        // Name
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(meta->name, contentArea.removeFromTop(18),
            juce::Justification::centredLeft);

        // Author
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(juce::Font(10.0f));
        g.drawText("by " + meta->author, contentArea.removeFromTop(15),
            juce::Justification::centredLeft);

        // Date
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(juce::Font(9.0f));
        g.drawText(meta->creationDate.formatted("%d %b %Y"),
            contentArea.removeFromTop(12),
            juce::Justification::centredLeft);
    }

    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected,
        juce::Component* existingComponent) override
    {
        // ✅ Use a full row component (text + small delete button).
        // NOTE: If we return a Component here, ListBox will NOT call paintListBoxItem().
        auto* rowComp = dynamic_cast<RowComponent*>(existingComponent);

        if (rowComp == nullptr)
            rowComp = new RowComponent(fileManager);

        rowComp->setRow(rowNumber, isRowSelected);
        rowComp->onDelete = [this, rowNumber]()
            {
                if (onAlgorithmDelete)
                    onAlgorithmDelete(rowNumber);
            };

        return rowComp;
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (onAlgorithmClicked)
            onAlgorithmClicked(row);
    }

    std::function<void(int)> onAlgorithmClicked;
    std::function<void(int)> onAlgorithmDelete;

private:
    AlgorithmFileManager& fileManager;
    juce::ListBox listBox{ "Algorithms", this };

    class DeleteButton : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour(isMouseOver ? juce::Colour(0xffef4444).brighter(0.08f)
                                    : juce::Colour(0xffef4444));
            g.fillRoundedRectangle(b, 4.0f);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(14.0f, juce::Font::bold));
            g.drawText("X", getLocalBounds(), juce::Justification::centred);
        }

        void mouseEnter(const juce::MouseEvent&) override { isMouseOver = true; repaint(); }
        void mouseExit(const juce::MouseEvent&) override { isMouseOver = false; repaint(); }

        void mouseDown(const juce::MouseEvent&) override
        {
            if (onDelete)
                onDelete();
        }

        std::function<void()> onDelete;

    private:
        bool isMouseOver = false;
    };

    class RowComponent : public juce::Component
    {
    public:
        RowComponent(AlgorithmFileManager& manager) : fileManager(manager)
        {
            addAndMakeVisible(deleteButton);
            deleteButton.onDelete = [this]()
                {
                    if (onDelete)
                        onDelete();
                };

            // Don't let the button click select/apply algorithm
            deleteButton.setInterceptsMouseClicks(true, false);
        }

        void setRow(int row, bool selected)
        {
            rowNumber = row;
            isSelected = selected;
            repaint();
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(10);
            auto topRow = area.removeFromTop(28);
            deleteButton.setBounds(topRow.removeFromRight(26).withHeight(26));
        }

        void paint(juce::Graphics& g) override
        {
            const auto* meta = fileManager.getMetadata(rowNumber);
            if (!meta)
                return;

            auto bounds = getLocalBounds().toFloat();

            // Background
            g.setColour(juce::Colour(0xff1f2937));
            g.fillRoundedRectangle(bounds.reduced(5), 6.0f);

            g.setColour(isSelected ? juce::Colour(0xff3b82f6) : juce::Colour(0xff374151));
            g.drawRoundedRectangle(bounds.reduced(6), 6.0f, 2.0f);

            auto contentArea = bounds.reduced(15);

            // Header row (reserve space for delete button)
            auto header = contentArea.removeFromTop(20);
            header.removeFromRight(30);

            // Type badge
            g.setColour(juce::Colour(0xff3b82f6).withAlpha(0.3f));
            auto typeBadge = header.removeFromLeft(80);
            g.fillRoundedRectangle(typeBadge, 3.0f);

            g.setColour(juce::Colour(0xff3b82f6));
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText(meta->algorithmType.toUpperCase(),
                typeBadge.reduced(2),
                juce::Justification::centred);

            contentArea.removeFromTop(5);

            // Name
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText(meta->name, contentArea.removeFromTop(18),
                juce::Justification::centredLeft);

            // Author
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.setFont(juce::Font(10.0f));
            g.drawText("by " + meta->author, contentArea.removeFromTop(15),
                juce::Justification::centredLeft);

            // Date
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.setFont(juce::Font(9.0f));
            g.drawText(meta->creationDate.formatted("%d %b %Y"),
                contentArea.removeFromTop(12),
                juce::Justification::centredLeft);
        }

        std::function<void()> onDelete;

    private:
        AlgorithmFileManager& fileManager;
        DeleteButton deleteButton;
        int rowNumber = -1;
        bool isSelected = false;
    };
};

// ==========================================================================
// ✅ MAIN ALGO LAB PANEL - PRODUCTION READY
// ==========================================================================

class AlgoLabPanel : public juce::Component, public juce::Timer
{
public:
    AlgoLabPanel(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        addAndMakeVisible(differenceLabButton);
        differenceLabButton.setButtonText("DIFFERENCE LAB");
        differenceLabButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff3b82f6));

        sound1Zone = std::make_unique<AudioDropZone>("DROP #1 SOUND (Original)");
        addAndMakeVisible(sound1Zone.get());

        sound2Zone = std::make_unique<AudioDropZone>("DROP #2 SOUND (Processed)");
        addAndMakeVisible(sound2Zone.get());

        addAndMakeVisible(calculateButton);
        calculateButton.setButtonText("CALCULATE DIFFERENCE");
        calculateButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff10b981));
        calculateButton.onClick = [this] { calculateDifference(); };

        addAndMakeVisible(saveAlgorithmButton);
        saveAlgorithmButton.setButtonText("SAVE ALGORITHM");
        saveAlgorithmButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff8b5cf6));
        saveAlgorithmButton.onClick = [this] { saveAlgorithm(); };
        saveAlgorithmButton.setEnabled(false);

        addAndMakeVisible(openFolderButton);
        openFolderButton.setButtonText("Open Folder");
        openFolderButton.onClick = [this] { openAlgorithmsFolder(); };

        // ✅ Virtual list instead of creating all buttons
        algorithmList = std::make_unique<VirtualAlgorithmList>(fileManager);
        addAndMakeVisible(algorithmList.get());

        algorithmList->onAlgorithmClicked = [this](int idx)
            {
                applyAlgorithm(idx);
            };

        algorithmList->onAlgorithmDelete = [this](int index)
            {
                deleteAlgorithm(index);
            };

        // ✅ Auto-refresh when metadata changes
        fileManager.onMetadataChanged =
            [safeThis = Component::SafePointer<AlgoLabPanel>(this)]()
            {
                juce::MessageManager::callAsync([safeThis]()
                    {
                        if (safeThis && safeThis->algorithmList)
                        {
                            safeThis->algorithmList->refresh();
                            safeThis->repaint();
                        }
                    });
            };

        startTimerHz(30);

        DBG("🚀 AlgoLab initialized (auto-scan enabled)");
    }

    ~AlgoLabPanel() override
    {
        stopTimer();
        fileManager.onMetadataChanged = nullptr;

        algorithmList.reset();
        sound1Zone.reset();
        sound2Zone.reset();

        DBG("🛑 AlgoLabPanel destroyed safely");
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("DIFFERENCE LAB", 10, 10, 400, 25,
            juce::Justification::centredLeft);

        juce::String headerText = "SAVED ALGORITHMS (" +
            juce::String(fileManager.getNumAlgorithms()) + ")";

        g.drawText(headerText, getWidth() - 450, 10, 440, 25,
            juce::Justification::centredLeft);

        auto calcArea = calculatingArea;
        g.setColour(juce::Colour(0xff111827));
        g.fillRoundedRectangle(calcArea.toFloat(), 6.0f);

        if (analysisComplete && currentAlgorithm.isValid())
        {
            auto contentArea = calcArea.reduced(15);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("ANALYSIS COMPLETE", contentArea.removeFromTop(20),
                juce::Justification::centredLeft);

            auto stats = currentAlgorithm.calculateStatistics();
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.setFont(juce::Font(11.0f));

            contentArea.removeFromTop(10);

            juce::String info;
            info << "• Frames: " << currentAlgorithm.transformData.numFrames << "\n";
            info << "• Bins: " << currentAlgorithm.transformData.numBins << "\n";
            info << "• Avg boost: " << juce::String(stats.averageMagnitudeBoost, 2) << "x";

            g.drawMultiLineText(info, contentArea.getX(),
                contentArea.getY() + 15, contentArea.getWidth());
        }
        else if (isCalculating)
        {
            g.setColour(juce::Colour(0xff3b82f6));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText("Analyzing...", calcArea, juce::Justification::centred);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        area.removeFromTop(35);

        auto leftPanel = area.removeFromLeft(getWidth() * 0.55f);
        area.removeFromLeft(10);
        auto rightPanel = area;

        differenceLabButton.setBounds(leftPanel.removeFromTop(35).withWidth(150));
        leftPanel.removeFromTop(10);

        auto dropZoneArea = leftPanel.removeFromTop(140);
        sound1Zone->setBounds(dropZoneArea.removeFromLeft(200).reduced(3));
        dropZoneArea.removeFromLeft(5);
        sound2Zone->setBounds(dropZoneArea.removeFromLeft(200).reduced(3));

        leftPanel.removeFromTop(10);
        calculateButton.setBounds(leftPanel.removeFromTop(40).withHeight(35));
        leftPanel.removeFromTop(10);
        saveAlgorithmButton.setBounds(leftPanel.removeFromTop(40).withHeight(35));
        leftPanel.removeFromTop(15);

        calculatingArea = leftPanel;

        auto headerRow = rightPanel.removeFromTop(35);
        openFolderButton.setBounds(headerRow.removeFromRight(120));

        rightPanel.removeFromTop(10);

        if (algorithmList)
            algorithmList->setBounds(rightPanel);
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;
    AlgorithmFileManager& fileManager = processor.getAlgorithmFileManager();

    juce::TextButton differenceLabButton;
    std::unique_ptr<AudioDropZone> sound1Zone, sound2Zone;
    juce::TextButton calculateButton, saveAlgorithmButton;
    juce::TextButton openFolderButton;
    std::unique_ptr<VirtualAlgorithmList> algorithmList;

    juce::Rectangle<int> calculatingArea;

    DifferenceAnalyzer differenceAnalyzer;
    AlgorithmEngine algorithmEngine;
    AlgorithmDNA currentAlgorithm;

    bool isCalculating = false;
    bool analysisComplete = false;

    void calculateDifference()
    {
        if (!sound1Zone->hasAudioLoaded() || !sound2Zone->hasAudioLoaded())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Missing Audio",
                "Please load both original and processed audio files.",
                "OK");
            return;
        }

        isCalculating = true;
        analysisComplete = false;
        repaint();

        juce::Thread::launch([this]()
            {
                currentAlgorithm = differenceAnalyzer.analyze(
                    sound1Zone->getAudioBuffer(),
                    sound2Zone->getAudioBuffer(),
                    sound1Zone->getSampleRate());

                currentAlgorithm.metadata.name = "Difference_" +
                    juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
                currentAlgorithm.metadata.author = "User";
                currentAlgorithm.metadata.algorithmType = "difference";

                juce::MessageManager::callAsync([this]()
                    {
                        isCalculating = false;
                        analysisComplete = true;
                        saveAlgorithmButton.setEnabled(currentAlgorithm.isValid());
                        repaint();
                    });
            });
    }

    void saveAlgorithm()
    {
        if (!currentAlgorithm.isValid())
            return;

        stopTimer();

        AlgorithmDNA algorithmCopy = currentAlgorithm;
        auto safeThis = Component::SafePointer<AlgoLabPanel>(this);

        auto* nameWindow = new juce::AlertWindow("Save Algorithm",
            "Enter algorithm name:", juce::AlertWindow::QuestionIcon);

        nameWindow->addTextEditor("name", algorithmCopy.metadata.name, "Name:");
        nameWindow->addTextEditor("description", "", "Description (optional):");
        nameWindow->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        nameWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        nameWindow->enterModalState(true,
            juce::ModalCallbackFunction::create(
                [safeThis, nameWindow, algorithmCopy](int result) mutable
                {
                    bool shouldSave = (result == 1);
                    juce::String name, description;

                    if (shouldSave)
                    {
                        name = nameWindow->getTextEditorContents("name");
                        description = nameWindow->getTextEditorContents("description");
                    }

                    delete nameWindow;

                    if (safeThis)
                        safeThis->startTimerHz(30);

                    if (!shouldSave || !safeThis)
                        return;

                    algorithmCopy.metadata.name = name;
                    algorithmCopy.metadata.description = description;

                    if (safeThis->fileManager.saveAlgorithm(algorithmCopy))
                    {
                        juce::MessageManager::callAsync([safeThis]()
                            {
                                if (safeThis)
                                {
                                    juce::AlertWindow::showMessageBoxAsync(
                                        juce::AlertWindow::InfoIcon,
                                        "Success",
                                        "Algorithm saved!\nAuto-refresh will update the list.",
                                        "OK");
                                }
                            });
                    }
                }), true);
    }

    void applyAlgorithm(int index)
    {
        // ✅ Lazy loading - load full algorithm only now
        auto algo = fileManager.loadFullAlgorithm(index);

        if (!algo || !processor.hasSampleLoaded())
            return;

        juce::AudioBuffer<float> input;
        input.makeCopyOf(processor.getOriginalSample());

        juce::AudioBuffer<float> output;
        algorithmEngine.applyAlgorithm(input, output, *algo);

        processor.loadSampleFromBuffer(output);

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Applied",
            "Algorithm '" + algo->metadata.name + "' applied successfully!",
            "OK");
    }

    void deleteAlgorithm(int index)
    {
        const auto* meta = fileManager.getMetadata(index);
        if (!meta)
            return;

        const auto name = meta->name;

        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon,
            "Delete Algorithm",
            "Are you sure you want to delete '" + name + "'?",
            "Delete", "Cancel", nullptr,
            juce::ModalCallbackFunction::create([this, index, name](int result)
                {
                    if (result == 1)
                    {
                        const bool ok = fileManager.deleteAlgorithmAtIndex(index);

                        if (!ok)
                        {
                            juce::AlertWindow::showMessageBoxAsync(
                                juce::AlertWindow::WarningIcon,
                                "Delete failed",
                                "Couldn't delete '" + name + "'.\n"
                                "Check file permissions or whether the file still exists.",
                                "OK");
                        }
                    }
                }));
    }

    void openAlgorithmsFolder()
    {
        auto folder = fileManager.getAlgorithmsFolder();
        folder.revealToUser();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgoLabPanel)
};