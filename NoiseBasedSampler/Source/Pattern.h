/*
Pattern.h
Структура для хранения и манипуляции паттернами.
Функции:

Enum PatternType: PeriodicSpike, WaveOscillation, SequenceDecay, AmplitudeBurst, HarmonicCluster
PatternProperties: параметры паттерна (duration, interval, targetLine, multiplier, amplitude, confidence, positions)
Методы редактирования параметров
applyToBuffer: применяет паттерн к difference buffer
Цветовая кодировка для UI
User preference score для ML обучения

Контекст и связи:

Используется PatternDetector для создания найденных паттернов
Хранится в PatternLibrary
Редактируется через PatternResearchPanel
Применяется через PluginProcessor::applyPatternToSample
Сериализация через DataSerializer
*/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <map>

enum class PatternType
{
    PeriodicSpike,
    WaveOscillation,
    SequenceDecay,
    AmplitudeBurst,
    HarmonicCluster,
    Unknown
};

struct PatternProperties
{
    int frequencyOfOccurrence = 0;      // Сколько раз встречается
    float durationSeconds = 0.0f;       // [2] seconds
    int intervalLines = 0;              // every [5] line
    int targetLine = 0;                 // get [4]th line
    float increaseMultiplier = 0.0f;    // by size [2.5]x times
    
    // Дополнительные параметры
    float amplitude = 0.0f;
    float confidence = 0.0f;            // 0-1, уверенность детекции
    
    // Позиции где найден паттерн в difference data
    std::vector<int> positions;
};

class Pattern
{
public:
    Pattern() = default;
    
    Pattern(PatternType type, const PatternProperties& props)
        : type(type), properties(props), id(generateId())
    {
        updateDescription();
    }
    
    // Getters
    int getId() const { return id; }
    PatternType getType() const { return type; }
    juce::String getTypeName() const { return patternTypeToString(type); }
    const PatternProperties& getProperties() const { return properties; }
    juce::String getDescription() const { return description; }
    juce::Colour getColour() const { return getColourForType(type); }
    
    // Setters (для редактирования параметров)
    void setDuration(float seconds) 
    { 
        properties.durationSeconds = seconds;
        updateDescription();
    }
    
    void setIntervalLines(int lines) 
    { 
        properties.intervalLines = lines;
        updateDescription();
    }
    
    void setTargetLine(int line) 
    { 
        properties.targetLine = line;
        updateDescription();
    }
    
    void setIncreaseMultiplier(float multiplier) 
    { 
        properties.increaseMultiplier = multiplier;
        updateDescription();
    }
    
    // Применение паттерна к difference data
    void applyToBuffer(juce::AudioBuffer<float>& differenceBuffer, float intensity = 1.0f);
    
    // Сериализация для сохранения
    juce::var toVar() const;
    static Pattern fromVar(const juce::var& v);
    
    // Для ML: насколько паттерн "нравится" пользователю
    void incrementUserPreference() { userPreferenceScore++; }
    int getUserPreferenceScore() const { return userPreferenceScore; }

private:
    int id = 0;
    PatternType type = PatternType::Unknown;
    PatternProperties properties;
    juce::String description;
    int userPreferenceScore = 0;  // Для ML эволюции
    
    void updateDescription()
    {
        description = juce::String::formatted(
            "pattern works for [%.1f] seconds, every [%d] line of information about [frequency] get [%d]th line increase by size [%.1f]x times",
            properties.durationSeconds,
            properties.intervalLines,
            properties.targetLine,
            properties.increaseMultiplier
        );
    }
    
    static int generateId()
    {
        static int counter = 0;
        return ++counter;
    }
    
    static juce::String patternTypeToString(PatternType type)
    {
        switch (type)
        {
            case PatternType::PeriodicSpike: return "Periodic Spike";
            case PatternType::WaveOscillation: return "Wave Oscillation";
            case PatternType::SequenceDecay: return "Sequence Decay";
            case PatternType::AmplitudeBurst: return "Amplitude Burst";
            case PatternType::HarmonicCluster: return "Harmonic Cluster";
            default: return "Unknown";
        }
    }
    
    static juce::Colour getColourForType(PatternType type)
    {
        switch (type)
        {
            case PatternType::PeriodicSpike: return juce::Colour(0xff3b82f6);
            case PatternType::WaveOscillation: return juce::Colour(0xff10b981);
            case PatternType::SequenceDecay: return juce::Colour(0xfff59e0b);
            case PatternType::AmplitudeBurst: return juce::Colour(0xffef4444);
            case PatternType::HarmonicCluster: return juce::Colour(0xff8b5cf6);
            default: return juce::Colours::grey;
        }
    }

};

// Реализация метода applyToBuffer
inline void Pattern::applyToBuffer(juce::AudioBuffer<float>& differenceBuffer, float intensity)
{
    if (differenceBuffer.getNumSamples() == 0)
        return;

    auto* data = differenceBuffer.getWritePointer(0);
    int numSamples = differenceBuffer.getNumSamples();

    // Применяем модификации паттерна к буферу
    for (int pos : properties.positions)
    {
        if (pos >= 0 && pos < numSamples)
        {
            // Применяем эффект паттерна
            int affectedRange = std::min(properties.intervalLines, numSamples - pos);

            for (int i = 0; i < affectedRange; ++i)
            {
                int idx = pos + i;
                if (idx >= numSamples) break;

                // Применяем multiplier к целевой линии
                if (i == properties.targetLine)
                {
                    data[idx] *= properties.increaseMultiplier * intensity;
                }
            }
        }
    }
}
