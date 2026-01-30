/*
ClipboardSidebar.h - ИСПРАВЛЕНО
✅ Наследование от juce::Timer добавлено
✅ timerCallback() теперь корректно переопределяет базовый метод
*/

#pragma once
#include <JuceHeader.h>
#include "ClipboardManager.h"

// ========== SINGLE SLOT COMPONENT ==========

class ClipboardSlotComponent : public juce::Component
{
public:
    ClipboardSlotComponent(int slotIdx, ClipboardManager& mgr)
        : slotIndex(slotIdx), manager(mgr)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        const auto& slot = manager.getSlot(slotIndex);

        // Background
        juce::Colour bgColour = slot.isEmpty
            ? juce::Colour(0xff2d2d2d)
            : (isSelected ? juce::Colour(0xff3b82f6) : juce::Colour(0xff374151));

        g.setColour(bgColour);
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

        // Border
        if (isMouseOver && !slot.isEmpty)
        {
            g.setColour(juce::Colour(0xff60a5fa));
            g.drawRoundedRectangle(bounds.toFloat().reduced(1), 6.0f, 2.0f);
        }

        auto contentArea = bounds.reduced(8);

        // Slot number
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        auto headerArea = contentArea.removeFromTop(15);
        g.drawText("[" + juce::String(slotIndex + 1) + "]",
            headerArea, juce::Justification::centredLeft);

        if (slot.isEmpty)
        {
            g.setColour(juce::Colours::grey.withAlpha(0.5f));
            g.setFont(juce::Font(9.0f));
            g.drawText("Empty", contentArea, juce::Justification::centred);
            return;
        }

        // Type & Length
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        auto infoArea = contentArea.removeFromTop(12);
        g.drawText(slot.getTypeName(), infoArea, juce::Justification::centredLeft);

        g.setFont(juce::Font(8.0f));
        auto lengthArea = contentArea.removeFromTop(10);
        g.drawText(juce::String(slot.getLength()) + " samples",
            lengthArea, juce::Justification::centredLeft);

        contentArea.removeFromTop(5);

        // Mini waveform preview
        auto waveArea = contentArea.removeFromTop(40);
        drawMiniWaveform(g, waveArea, slot);
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        isMouseOver = true;
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        isMouseOver = false;
        repaint();
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        const auto& slot = manager.getSlot(slotIndex);

        if (slot.isEmpty)
            return;

        if (onClick)
            onClick(slotIndex);
    }

    void setSelected(bool selected)
    {
        isSelected = selected;
        repaint();
    }

    std::function<void(int)> onClick;

private:
    int slotIndex;
    ClipboardManager& manager;
    bool isMouseOver = false;
    bool isSelected = false;

    void drawMiniWaveform(juce::Graphics& g, juce::Rectangle<int> area,
        const ClipboardSlot& slot)
    {
        if (slot.previewData.empty())
            return;

        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(area.toFloat(), 3.0f);

        // Get color based on type
        juce::Colour waveColour;
        switch (slot.indexType)
        {
        case IndexType::Amplitude: waveColour = juce::Colour(0xff3b82f6); break;
        case IndexType::Frequency: waveColour = juce::Colour(0xff10b981); break;
        case IndexType::Phase: waveColour = juce::Colour(0xfff59e0b); break;
        case IndexType::Volume: waveColour = juce::Colour(0xffec4899); break;
        case IndexType::Pan: waveColour = juce::Colour(0xff06b6d4); break;
        }

        // Draw waveform
        juce::Path path;
        bool firstPoint = true;

        float minVal = *std::min_element(slot.previewData.begin(), slot.previewData.end());
        float maxVal = *std::max_element(slot.previewData.begin(), slot.previewData.end());
        float range = maxVal - minVal;
        if (range < 0.001f) range = 1.0f;

        int numPoints = static_cast<int>(slot.previewData.size());

        for (int i = 0; i < numPoints; ++i)
        {
            float x = area.getX() + (i / static_cast<float>(numPoints - 1)) * area.getWidth();
            float normalized = (slot.previewData[i] - minVal) / range;
            float y = area.getBottom() - normalized * area.getHeight();

            if (firstPoint)
            {
                path.startNewSubPath(x, y);
                firstPoint = false;
            }
            else
            {
                path.lineTo(x, y);
            }
        }

        g.setColour(waveColour);
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
};

// ========== CLIPBOARD SIDEBAR ==========

class ClipboardSidebar : public juce::Component,
    public juce::Timer,           // ✅ ИСПРАВЛЕНО: Добавлено наследование
    public juce::KeyListener
{
public:
    ClipboardSidebar(ClipboardManager& mgr)
        : manager(mgr)
    {
        // Create 5 slot components
        for (int i = 0; i < ClipboardManager::MAX_SLOTS; ++i)
        {
            auto* slot = new ClipboardSlotComponent(i, manager);
            slot->onClick = [this](int idx) { handleSlotClick(idx); };
            slotComponents.add(slot);
            addAndMakeVisible(slot);
        }

        // Paste mode controls
        addAndMakeVisible(pasteModeLabel);
        pasteModeLabel.setText("Paste Mode:", juce::dontSendNotification);
        pasteModeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
        pasteModeLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        addAndMakeVisible(replaceButton);
        replaceButton.setButtonText("Replace");
        replaceButton.setRadioGroupId(1);
        replaceButton.setClickingTogglesState(true);
        replaceButton.setToggleState(true, juce::dontSendNotification);
        replaceButton.onClick = [this] { currentPasteMode = PasteMode::Replace; };

        addAndMakeVisible(addButton);
        addButton.setButtonText("Add");
        addButton.setRadioGroupId(1);
        addButton.setClickingTogglesState(true);
        addButton.onClick = [this] { currentPasteMode = PasteMode::Add; };

        addAndMakeVisible(multiplyButton);
        multiplyButton.setButtonText("Multiply");
        multiplyButton.setRadioGroupId(1);
        multiplyButton.setClickingTogglesState(true);
        multiplyButton.onClick = [this] { currentPasteMode = PasteMode::Multiply; };

        addAndMakeVisible(mixButton);
        mixButton.setButtonText("Mix");
        mixButton.setRadioGroupId(1);
        mixButton.setClickingTogglesState(true);
        mixButton.onClick = [this] { currentPasteMode = PasteMode::Mix; };

        addAndMakeVisible(mixSlider);
        mixSlider.setRange(0.0, 1.0, 0.01);
        mixSlider.setValue(0.5);
        mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);

        setWantsKeyboardFocus(true);
        startTimer(30);  // ✅ ИСПРАВЛЕНО: Теперь работает корректно
    }

    ~ClipboardSidebar() override
    {
        stopTimer();  // ✅ Правильная очистка
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a1a));

        // Header
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("📋 Clipboard Slots", 10, 10, getWidth() - 20, 25,
            juce::Justification::centredLeft);

        // Instructions
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font(9.0f));
        g.drawText("Alt+1-5: Select slot | Ctrl+C/V: Copy/Paste",
            10, getHeight() - 20, getWidth() - 20, 15,
            juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);

        area.removeFromTop(40);  // Header

        // Slots
        for (int i = 0; i < slotComponents.size(); ++i)
        {
            slotComponents[i]->setBounds(area.removeFromTop(100));
            area.removeFromTop(5);
        }

        area.removeFromTop(10);

        // Paste mode controls
        pasteModeLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(5);

        replaceButton.setBounds(area.removeFromTop(25));
        area.removeFromTop(3);
        addButton.setBounds(area.removeFromTop(25));
        area.removeFromTop(3);
        multiplyButton.setBounds(area.removeFromTop(25));
        area.removeFromTop(3);
        mixButton.setBounds(area.removeFromTop(25));
        area.removeFromTop(5);
        mixSlider.setBounds(area.removeFromTop(25));
    }

    // ✅ ИСПРАВЛЕНО: Правильное переопределение
    void timerCallback() override
    {
        // Update selected slot visuals
        for (int i = 0; i < slotComponents.size(); ++i)
        {
            slotComponents[i]->setSelected(i == currentSlotIndex);
        }
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        // Alt+1-5 для выбора слота
        if (key.getModifiers().isAltDown())
        {
            if (key.getKeyCode() >= '1' && key.getKeyCode() <= '5')
            {
                int slotIdx = key.getKeyCode() - '1';
                setCurrentSlot(slotIdx);
                return true;
            }
        }

        return false;
    }

    void setCurrentSlot(int index)
    {
        if (index >= 0 && index < ClipboardManager::MAX_SLOTS)
        {
            currentSlotIndex = index;
            repaint();
        }
    }

    int getCurrentSlot() const { return currentSlotIndex; }
    PasteMode getCurrentPasteMode() const { return currentPasteMode; }
    float getMixAmount() const { return static_cast<float>(mixSlider.getValue()); }

    std::function<void(int)> onSlotSelected;

private:
    ClipboardManager& manager;
    juce::OwnedArray<ClipboardSlotComponent> slotComponents;

    juce::Label pasteModeLabel;
    juce::ToggleButton replaceButton;
    juce::ToggleButton addButton;
    juce::ToggleButton multiplyButton;
    juce::ToggleButton mixButton;
    juce::Slider mixSlider;

    int currentSlotIndex = 0;
    PasteMode currentPasteMode = PasteMode::Replace;

    void handleSlotClick(int slotIdx)
    {
        setCurrentSlot(slotIdx);

        if (onSlotSelected)
            onSlotSelected(slotIdx);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipboardSidebar)
};