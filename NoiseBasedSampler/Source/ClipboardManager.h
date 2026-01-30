/*
ClipboardManager.h - ИСПРАВЛЕНО
✅ Добавлен #include "FeatureData.h"
✅ FeaturePoint заменён на FeatureSample
✅ Исправлены операторы сравнения (|| вместо |)
*/

#pragma once
#include <JuceHeader.h>
#include "FeatureData.h"  // ✅ КРИТИЧНО: Определяет FeatureSample
#include <vector>

enum class IndexType
{
    Amplitude,
    Frequency,
    Phase,
    Volume,
    Pan
};

enum class PasteMode
{
    Replace,    // Полная замена
    Add,        // Добавление к существующим
    Multiply,   // Умножение
    Mix         // Линейная интерполяция
};

struct ClipboardSlot
{
    bool isEmpty = true;
    IndexType indexType = IndexType::Amplitude;
    std::vector<float> values;
    int originalStartSample = 0;
    int originalEndSample = 0;
    juce::String description;

    // Mini preview для UI (64 точки max)
    std::vector<float> previewData;

    void clear()
    {
        isEmpty = true;
        values.clear();
        previewData.clear();
        description = "";
    }

    int getLength() const { return static_cast<int>(values.size()); }

    juce::String getTypeName() const
    {
        switch (indexType)
        {
        case IndexType::Amplitude: return "Amplitude";
        case IndexType::Frequency: return "Frequency";
        case IndexType::Phase: return "Phase";
        case IndexType::Volume: return "Volume";
        case IndexType::Pan: return "Pan";
        }
        return "";
    }
};

class ClipboardManager
{
public:
    ClipboardManager() = default;

    // ========== COPY OPERATIONS ==========

    bool copyRegion(const FeatureData& features,
        IndexType type,
        int startSample,
        int endSample,
        int slotIndex = 0)
    {
        // ✅ ИСПРАВЛЕНО: || вместо |
        if (slotIndex < 0 || slotIndex >= MAX_SLOTS)
            return false;

        // ✅ ИСПРАВЛЕНО: || вместо |
        if (startSample < 0 || endSample >= features.getNumSamples() ||
            startSample > endSample)
            return false;

        auto& slot = slots[slotIndex];
        slot.clear();

        int length = endSample - startSample + 1;
        slot.values.reserve(length);
        slot.indexType = type;
        slot.originalStartSample = startSample;
        slot.originalEndSample = endSample;

        // Копируем значения
        for (int i = startSample; i <= endSample; ++i)
        {
            float value = extractValue(features[i], type);
            slot.values.push_back(value);
        }

        // Создаём preview (downsampled до 64 точек max)
        createPreview(slot);

        // Описание
        slot.description = slot.getTypeName() + " (" +
            juce::String(length) + " samples)";
        slot.isEmpty = false;

        DBG("📋 Copied to slot " + juce::String(slotIndex) + ": " + slot.description);

        return true;
    }

    // ========== PASTE OPERATIONS ==========

    bool paste(FeatureData& features,
        int pastePosition,
        int slotIndex = 0,
        PasteMode mode = PasteMode::Replace,
        float mixAmount = 0.5f)
    {
        // ✅ ИСПРАВЛЕНО: || вместо |
        if (slotIndex < 0 || slotIndex >= MAX_SLOTS)
            return false;

        const auto& slot = slots[slotIndex];

        if (slot.isEmpty)
        {
            DBG("⚠️ Slot " + juce::String(slotIndex) + " is empty");
            return false;
        }

        int numSamples = features.getNumSamples();
        int copyLength = slot.getLength();

        // ✅ ИСПРАВЛЕНО: || вместо |
        if (pastePosition < 0 || pastePosition >= numSamples)
            return false;

        // Вычисляем сколько можем вставить
        int availableSpace = numSamples - pastePosition;
        int pasteLength = std::min(copyLength, availableSpace);

        DBG("📋 Pasting " + juce::String(pasteLength) + " samples to position " +
            juce::String(pastePosition) + " (mode: " + getModeName(mode) + ")");

        // Применяем в зависимости от режима
        for (int i = 0; i < pasteLength; ++i)
        {
            int targetIdx = pastePosition + i;
            float copiedValue = slot.values[i];
            float currentValue = extractValue(features[targetIdx], slot.indexType);
            float newValue = 0.0f;

            switch (mode)
            {
            case PasteMode::Replace:
                newValue = copiedValue;
                break;

            case PasteMode::Add:
                newValue = currentValue + copiedValue;
                break;

            case PasteMode::Multiply:
                newValue = currentValue * copiedValue;
                break;

            case PasteMode::Mix:
                newValue = currentValue * (1.0f - mixAmount) + copiedValue * mixAmount;
                break;
            }

            // Применяем с учётом диапазонов
            applyValue(features, targetIdx, slot.indexType, newValue);
        }

        return true;
    }

    // ========== SLOT MANAGEMENT ==========

    const ClipboardSlot& getSlot(int index) const
    {
        static ClipboardSlot emptySlot;
        // ✅ ИСПРАВЛЕНО: || вместо |
        if (index < 0 || index >= MAX_SLOTS)
            return emptySlot;
        return slots[index];
    }

    void clearSlot(int index)
    {
        if (index >= 0 && index < MAX_SLOTS)
        {
            slots[index].clear();
            DBG("🗑 Cleared slot " + juce::String(index));
        }
    }

    void clearAllSlots()
    {
        for (auto& slot : slots)
            slot.clear();
        DBG("🗑 Cleared all clipboard slots");
    }

    bool isSlotEmpty(int index) const
    {
        if (index < 0 || index >= MAX_SLOTS)
            return true;
        return slots[index].isEmpty;
    }

    // ========== UTILITY ==========

    static constexpr int MAX_SLOTS = 5;

    juce::String getModeName(PasteMode mode) const
    {
        switch (mode)
        {
        case PasteMode::Replace: return "Replace";
        case PasteMode::Add: return "Add";
        case PasteMode::Multiply: return "Multiply";
        case PasteMode::Mix: return "Mix";
        }
        return "";
    }

private:
    std::array<ClipboardSlot, MAX_SLOTS> slots;

    // ✅ ИСПРАВЛЕНО: FeaturePoint → FeatureSample
    float extractValue(const FeatureSample& point, IndexType type) const
    {
        switch (type)
        {
        case IndexType::Amplitude: return point.amplitude;
        case IndexType::Frequency: return point.frequency;
        case IndexType::Phase: return point.phase;
        case IndexType::Volume: return point.volume;
        case IndexType::Pan: return point.pan;
        }
        return 0.0f;
    }

    // Применение значения к FeaturePoint
    void applyValue(FeatureData& features, int index, IndexType type, float value)
    {
        switch (type)
        {
        case IndexType::Amplitude:
            features.setAmplitudeAt(index, juce::jlimit(-1.0f, 1.0f, value));
            break;

        case IndexType::Frequency:
            features.setFrequencyAt(index, juce::jlimit(20.0f, 20000.0f, value));
            break;

        case IndexType::Phase:
        {
            float clampedPhase = std::fmod(value, juce::MathConstants<float>::twoPi);
            if (clampedPhase < 0)
                clampedPhase += juce::MathConstants<float>::twoPi;
            features.setPhaseAt(index, clampedPhase);
            break;
        }

        case IndexType::Volume:
            features.setVolumeAt(index, juce::jlimit(0.0f, 2.0f, value));
            break;

        case IndexType::Pan:
            features.setPanAt(index, juce::jlimit(0.0f, 1.0f, value));
            break;
        }
    }

    // Создание preview (downsampling)
    void createPreview(ClipboardSlot& slot)
    {
        const int MAX_PREVIEW_POINTS = 64;

        if (slot.values.empty())
            return;

        int sourceLength = static_cast<int>(slot.values.size());

        if (sourceLength <= MAX_PREVIEW_POINTS)
        {
            // Просто копируем
            slot.previewData = slot.values;
        }
        else
        {
            // Downsample
            slot.previewData.resize(MAX_PREVIEW_POINTS);

            for (int i = 0; i < MAX_PREVIEW_POINTS; ++i)
            {
                float position = i * (sourceLength - 1) / static_cast<float>(MAX_PREVIEW_POINTS - 1);
                int index = static_cast<int>(position);

                if (index < sourceLength - 1)
                {
                    // Linear interpolation
                    float frac = position - index;
                    slot.previewData[i] = slot.values[index] * (1.0f - frac) +
                        slot.values[index + 1] * frac;
                }
                else
                {
                    slot.previewData[i] = slot.values[sourceLength - 1];
                }
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipboardManager)
};