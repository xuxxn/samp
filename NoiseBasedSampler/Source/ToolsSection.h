/*
ToolsSection.h - LEFT TOOLS SECTION
✅ Feature tool buttons (5×3 grid)
✅ Category switching (GENERAL/INDEX/LFO/ADSR)
✅ START/LENGTH circular controls
✅ ADSRPanel integration
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ADSRPanel.h"

// ========== FEATURE TOOL BUTTON ==========
class FeatureToolButton : public juce::TextButton
{
public:
    std::function<void()> onRightClick;
    std::function<void(const juce::MouseWheelDetails&)> onMouseWheel;

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            rightButtonPressed = true;
        }
        else
        {
            juce::TextButton::mouseDown(e);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (rightButtonPressed && e.mods.isRightButtonDown() && onRightClick)
        {
            onRightClick();
            rightButtonPressed = false;
        }
        else
        {
            rightButtonPressed = false;
            juce::TextButton::mouseUp(e);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!e.mods.isRightButtonDown())
        {
            rightButtonPressed = false;
        }
        juce::TextButton::mouseDrag(e);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (onMouseWheel)
        {
            onMouseWheel(wheel);
        }
        else
        {
            juce::TextButton::mouseWheelMove(e, wheel);
        }
    }

private:
    bool rightButtonPressed = false;
};

// ========== START/LENGTH CONTROL ==========
class StartLengthControl : public juce::Component,
    private juce::Timer
{
public:
    enum class Type { Start, Length };

    StartLengthControl(Type controlType, NoiseBasedSamplerAudioProcessor& proc)
        : type(controlType), processor(proc)
    {
        startTimerHz(30);

        if (type == Type::Start)
        {
            currentValue = 100.0f;
            originalValue = 100.0f;
        }
        else
        {
            currentValue = 100.0f;
            originalValue = 100.0f;
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        juce::Colour bgColour = isDragging
            ? juce::Colour(0xff7c3aed)
            : (isMouseOver ? juce::Colour(0xff9333ea) : juce::Colour(0xff6b21a8));

        g.setColour(bgColour);
        g.fillEllipse(bounds.reduced(8));

        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawEllipse(bounds.reduced(8), 2.0f);

        float startAngle = -2.5f;
        float endAngle = startAngle + (currentValue / 100.0f) * 5.0f;

        juce::Path arcPath;
        arcPath.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
            bounds.getWidth() / 2.5f, bounds.getHeight() / 2.5f,
            0.0f, startAngle, endAngle, true);

        g.setColour(juce::Colour(0xff10b981));
        g.strokePath(arcPath, juce::PathStrokeType(4.0f));

        if (!isDragging || (blinkPhase < 0.5f))
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(16.0f, juce::Font::bold));
            g.drawText(juce::String((int)currentValue),
                bounds, juce::Justification::centred);
        }

        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        juce::String label = (type == Type::Start) ? "START" : "LENGTH";
        g.drawText(label, bounds.removeFromBottom(15),
            juce::Justification::centred);
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        isMouseOver = true;
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        isMouseOver = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            isDragging = true;
            dragStartY = e.position.y;
            originalValue = currentValue;
            processor.beginSampleRangePreview();
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!isDragging) return;

        float deltaY = dragStartY - e.position.y;
        float sensitivity = 0.5f;

        float newValue = juce::jlimit(0.0f, 100.0f,
            originalValue + (deltaY * sensitivity));

        if (std::abs(newValue - currentValue) > 0.1f)
        {
            currentValue = newValue;

            if (type == Type::Start)
            {
                float startPercent = 1.0f - (currentValue / 100.0f);
                processor.previewSampleStart(startPercent);
            }
            else
                processor.previewSampleLength(currentValue / 100.0f);

            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (isDragging)
        {
            isDragging = false;

            if (type == Type::Start)
            {
                float startPercent = 1.0f - (currentValue / 100.0f);
                processor.applySampleStart(startPercent);
            }
            else
                processor.applySampleLength(currentValue / 100.0f);

            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent&,
        const juce::MouseWheelDetails& wheel) override
    {
        float delta = wheel.deltaY * 2.0f;
        currentValue = juce::jlimit(0.0f, 100.0f, currentValue + delta);

        if (type == Type::Start)
        {
            float startPercent = 1.0f - (currentValue / 100.0f);
            processor.applySampleStart(startPercent);
        }
        else
            processor.applySampleLength(currentValue / 100.0f);

        repaint();
    }

    void updateFromProcessor()
    {
        if (type == Type::Start)
        {
            float startPercent = processor.getSampleStartPercent();
            currentValue = (1.0f - startPercent) * 100.0f;
        }
        else
        {
            float lengthPercent = processor.getSampleLengthPercent();
            currentValue = lengthPercent * 100.0f;
        }

        originalValue = currentValue;
        repaint();
    }

    void timerCallback() override
    {
        if (isDragging)
        {
            blinkPhase += 0.15f;
            if (blinkPhase >= 1.0f)
                blinkPhase = 0.0f;
            repaint();
        }
    }

private:
    Type type;
    NoiseBasedSamplerAudioProcessor& processor;

    float currentValue = 100.0f;
    float originalValue = 100.0f;
    bool isDragging = false;
    bool isMouseOver = false;
    float dragStartY = 0.0f;
    float blinkPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StartLengthControl)
};

// ========== TOOLS SECTION ==========
class ToolsSection : public juce::Component,
    private juce::Timer
{
public:
    enum class FeatureToolCategory
    {
        General,
        Index,
        Lfo,
        Adsr
    };

    static constexpr int kNumFeatureToolButtons = 15;

    ToolsSection(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        startTimerHz(30);

        for (int i = 0; i < kNumFeatureToolButtons; ++i)
        {
            addAndMakeVisible(featureToolButtons[i]);
            featureToolButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colours::white);
            featureToolButtons[i].setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        }

        featureToolButtons[4].onClick = [this] { setFeatureToolCategory(FeatureToolCategory::General); };
        featureToolButtons[9].onClick = [this] { setFeatureToolCategory(FeatureToolCategory::Index); };
        featureToolButtons[14].onClick = [this] { setFeatureToolCategory(FeatureToolCategory::Lfo); };

        featureToolButtons[10].onClick = [this]
            {
                processor.toggleAdsrCutItselfMode();
                updateEffectButtonStates();
            };

        startControl = std::make_unique<StartLengthControl>(StartLengthControl::Type::Start, processor);
        addChildComponent(startControl.get());

        lengthControl = std::make_unique<StartLengthControl>(StartLengthControl::Type::Length, processor);
        addChildComponent(lengthControl.get());

        adsrPanel = std::make_unique<ADSRPanel>(processor);
        addChildComponent(adsrPanel.get());

        restoreLastCategory();
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
        auto area = getLocalBounds().reduced(10);

        const int cols = 3;
        const int rows = 5;
        const int gapX = 10;
        const int gapY = 10;
        const int buttonH = 45;

        const int totalGapX = gapX * (cols - 1);
        const int colW = (area.getWidth() - totalGapX) / cols;

        int startX = area.getX();
        int startY = area.getY();

        for (int i = 0; i < kNumFeatureToolButtons; ++i)
        {
            const int col = i % cols;
            const int row = i / cols;

            const int x = startX + col * (colW + gapX);
            const int y = startY + row * (buttonH + gapY);

            featureToolButtons[i].setBounds(x, y, colW, buttonH);
        }

        if (adsrPanel && adsrPanel->isVisible())
        {
            adsrPanel->setBounds(area);
        }
    }

    void timerCallback() override
    {
        if (startControl && startControl->isVisible())
            startControl->updateFromProcessor();

        if (lengthControl && lengthControl->isVisible())
            lengthControl->updateFromProcessor();
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;
    std::array<FeatureToolButton, kNumFeatureToolButtons> featureToolButtons;

    std::unique_ptr<StartLengthControl> startControl;
    std::unique_ptr<StartLengthControl> lengthControl;
    std::unique_ptr<ADSRPanel> adsrPanel;

    FeatureToolCategory currentFeatureToolCategory = FeatureToolCategory::General;
    std::unique_ptr<juce::PropertiesFile> uiProperties;

    void setFeatureToolCategory(FeatureToolCategory newCategory, bool skipSave = false)
    {
        if (currentFeatureToolCategory == newCategory)
            return;

        currentFeatureToolCategory = newCategory;

        if (currentFeatureToolCategory == FeatureToolCategory::Adsr)
        {
            for (auto& btn : featureToolButtons)
                btn.setVisible(false);

            if (startControl) startControl->setVisible(false);
            if (lengthControl) lengthControl->setVisible(false);

            if (adsrPanel)
                adsrPanel->setVisible(true);
        }
        else
        {
            if (adsrPanel)
                adsrPanel->setVisible(false);

            updateFeatureToolButtonsForCategory();
        }

        resized();

        if (!skipSave)
        {
            ensureUIProperties();

            if (uiProperties)
            {
                juce::String value = "general";
                auto cat = currentFeatureToolCategory;
                if (cat == FeatureToolCategory::Index) value = "index";
                else if (cat == FeatureToolCategory::Lfo) value = "lfo";
                else if (cat == FeatureToolCategory::Adsr) value = "adsr";

                uiProperties->setValue("FeatureToolsCategory", value);
                uiProperties->saveIfNeeded();
            }
        }
    }

    void updateFeatureToolButtonsForCategory()
    {
        auto setCategoryButton = [&](int idx, bool selected)
            {
                featureToolButtons[idx].setColour(
                    juce::TextButton::buttonColourId,
                    selected ? juce::Colour(0xff7c3aed) : juce::Colours::white);
                featureToolButtons[idx].setColour(
                    juce::TextButton::textColourOffId,
                    selected ? juce::Colours::white : juce::Colours::black);
            };

        const bool isGeneral = (currentFeatureToolCategory == FeatureToolCategory::General);
        const bool isIndex = (currentFeatureToolCategory == FeatureToolCategory::Index);
        const bool isLfo = (currentFeatureToolCategory == FeatureToolCategory::Lfo);

        setCategoryButton(4, isGeneral);
        setCategoryButton(9, isIndex);
        setCategoryButton(14, isLfo);

        for (int i = 0; i < kNumFeatureToolButtons; ++i)
        {
            const bool isCategory = (i == 4 || i == 9 || i == 14);
            const bool isADSR = (i == 10);

            if (!isCategory && !isADSR)
            {
                featureToolButtons[i].setVisible(false);
                featureToolButtons[i].setButtonText("");
                featureToolButtons[i].onClick = nullptr;
            }
            else if (isCategory)
            {
                featureToolButtons[i].setVisible(true);
            }
        }

        if (isGeneral)
        {
            if (startControl) startControl->setVisible(false);
            if (lengthControl) lengthControl->setVisible(false);

            static const int generalButtons[] = { 0, 1, 2, 3, 5, 6, 7, 8, 10, 11, 12, 13 };
            static const char* generalLabels[] = {
                "START", "LENGTH", "TRIM", "STRETCH",
                "NORMALIZE", "STEREO", "REVERSE", "BOOST",
                "ADSR", "ARP", "EQ", "KEY"
            };

            for (int i = 0; i < 12; ++i)
            {
                int btnIdx = generalButtons[i];
                if (btnIdx != 2 && btnIdx != 5 && btnIdx != 10)
                {
                    featureToolButtons[btnIdx].setButtonText(generalLabels[i]);
                }
                featureToolButtons[btnIdx].setVisible(true);
            }

            featureToolButtons[2].setVisible(true);
            featureToolButtons[2].onClick = [this]
                {
                    processor.toggleTrim();
                    updateEffectButtonStates();
                };

            featureToolButtons[5].setVisible(true);
            featureToolButtons[5].onClick = [this]
                {
                    processor.toggleNormalize();
                    updateEffectButtonStates();
                };

            featureToolButtons[7].setVisible(true);
            featureToolButtons[7].onClick = [this]
                {
                    processor.toggleReverse();
                    updateEffectButtonStates();
                };

            featureToolButtons[8].setVisible(true);
            featureToolButtons[8].onClick = [this]
                {
                    processor.toggleBoost();
                    updateEffectButtonStates();
                };

            featureToolButtons[8].onMouseWheel = [this](const juce::MouseWheelDetails& wheel)
                {
                    float currentBoostDb = processor.isBoostActive() ? processor.getEffectStateManager().getBoostDb() : 0.0f;
                    float deltaDb = wheel.deltaY * 2.0f;
                    float newBoostDb = juce::jlimit(-24.0f, 24.0f, currentBoostDb + deltaDb);
                    processor.setBoostLevel(newBoostDb);
                    updateEffectButtonStates();
                };

            featureToolButtons[10].setVisible(true);
        }
        else if (isIndex)
        {
            if (startControl) startControl->setVisible(false);
            if (lengthControl) lengthControl->setVisible(false);

            static const int indexButtons[] = { 0, 1, 2, 3, 10 };
            static const char* indexLabels[] = {
                "brush", "line", "region", "scale", ""
            };

            for (int i = 0; i < 5; ++i)
            {
                int btnIdx = indexButtons[i];
                if (btnIdx != 10)
                {
                    featureToolButtons[btnIdx].setButtonText(indexLabels[i]);
                }
                featureToolButtons[btnIdx].setVisible(true);
            }

            // INDEX-specific actions would go here
            // featureToolButtons[0].onClick = [this] { /* brush tool */ };
            // etc.

            featureToolButtons[10].setVisible(true);
        }
        else if (isLfo)
        {
            if (startControl) startControl->setVisible(false);
            if (lengthControl) lengthControl->setVisible(false);
            featureToolButtons[10].setVisible(true);
        }

        updateEffectButtonStates();
    }

    void updateEffectButtonStates()
    {
        bool adsrCutItself = processor.isAdsrCutItselfMode();
        featureToolButtons[10].setColour(
            juce::TextButton::buttonColourId,
            adsrCutItself ? juce::Colour(0xff8b5cf6) : juce::Colours::white);
        featureToolButtons[10].setColour(
            juce::TextButton::textColourOffId,
            adsrCutItself ? juce::Colours::white : juce::Colours::black);

        if (adsrCutItself)
            featureToolButtons[10].setButtonText("CUT ITSELF");
        else
            featureToolButtons[10].setButtonText("ADSR");

        bool trimActive = processor.isTrimActive() && currentFeatureToolCategory == FeatureToolCategory::General;
        featureToolButtons[2].setColour(
            juce::TextButton::buttonColourId,
            trimActive ? juce::Colour(0xff059669) : juce::Colours::white);
        featureToolButtons[2].setColour(
            juce::TextButton::textColourOffId,
            trimActive ? juce::Colours::white : juce::Colours::black);

        if (trimActive)
            featureToolButtons[2].setButtonText("TRIM*");
        else
            featureToolButtons[2].setButtonText("TRIM");

        bool normalizeActive = processor.isNormalizeActive() && currentFeatureToolCategory == FeatureToolCategory::General;
        featureToolButtons[5].setColour(
            juce::TextButton::buttonColourId,
            normalizeActive ? juce::Colour(0xff059669) : juce::Colours::white);
        featureToolButtons[5].setColour(
            juce::TextButton::textColourOffId,
            normalizeActive ? juce::Colours::white : juce::Colours::black);

        if (normalizeActive)
        {
            auto& effectState = processor.getEffectStateManager();
            float targetDb = effectState.getNormalizeTargetDb();
            featureToolButtons[5].setButtonText("NORM " + juce::String(targetDb, 0) + "dB*");
        }
        else
        {
            featureToolButtons[5].setButtonText("NORMALIZE");
        }

        bool reverseActive = processor.isReverseActive() && currentFeatureToolCategory == FeatureToolCategory::General;
        featureToolButtons[7].setColour(
            juce::TextButton::buttonColourId,
            reverseActive ? juce::Colour(0xff8b5cf6) : juce::Colours::white);
        featureToolButtons[7].setColour(
            juce::TextButton::textColourOffId,
            reverseActive ? juce::Colours::white : juce::Colours::black);

        bool boostActive = processor.isBoostActive() && currentFeatureToolCategory == FeatureToolCategory::General;
        featureToolButtons[8].setColour(
            juce::TextButton::buttonColourId,
            boostActive ? juce::Colour(0xff10b981) : juce::Colours::white);
        featureToolButtons[8].setColour(
            juce::TextButton::textColourOffId,
            boostActive ? juce::Colours::white : juce::Colours::black);
    }

    void ensureUIProperties()
    {
        if (uiProperties) return;

        juce::PropertiesFile::Options options;
        options.applicationName = "NoiseBasedSampler";
        options.filenameSuffix = ".ui";
        options.folderName = "NoiseBasedSampler";
        options.osxLibrarySubFolder = "Application Support";

        uiProperties = std::make_unique<juce::PropertiesFile>(
            options.getDefaultFile(), options);

        auto saved = uiProperties->getValue("FeatureToolsCategory", "general");
        FeatureToolCategory cat = FeatureToolCategory::General;
        if (saved == "index") cat = FeatureToolCategory::Index;
        else if (saved == "lfo") cat = FeatureToolCategory::Lfo;
        else if (saved == "adsr") cat = FeatureToolCategory::Adsr;

        setFeatureToolCategory(cat, true);
    }

    void restoreLastCategory()
    {
        ensureUIProperties();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolsSection)
};