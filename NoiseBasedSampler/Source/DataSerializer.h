/*
DataSerializer.h
Утилиты для сериализации/десериализации данных в JSON.
Функции:

exportDifferenceData: экспорт difference buffer + статистика в JSON
exportPattern/importPattern: сериализация одного паттерна
exportPatterns/importPatterns: сериализация коллекции паттернов
exportMLState/importMLState: сохранение/загрузка состояния ML движка
Внутренние методы: patternToJson, jsonToPattern, saveJsonToFile, loadJsonFromFile

Контекст и связи:

Используется PluginProcessor для exportDifferenceData
Используется PatternLibrary для save/load
Используется MLEvolutionEngine для save/load state
Используется PatternResearchPanel при сохранении паттернов
Формат JSON для человеко-читаемости
*/

#pragma once
#include <JuceHeader.h>
#include "Pattern.h"
#include <vector>

class DataSerializer
{
public:
    DataSerializer() = default;
    
    // ========== DIFFERENCE DATA ==========
    
    // Экспорт difference data (уже есть у тебя)
    static bool exportDifferenceData(const juce::AudioBuffer<float>& differenceBuffer,
                                    const juce::File& outputFile,
                                    double sampleRate,
                                    int seed)
    {
        juce::DynamicObject::Ptr jsonData = new juce::DynamicObject();
        jsonData->setProperty("version", "1.0");
        jsonData->setProperty("type", "difference_data");
        jsonData->setProperty("seed", seed);
        jsonData->setProperty("length", differenceBuffer.getNumSamples());
        jsonData->setProperty("sampleRate", sampleRate);
        
        // Статистика
        auto stats = calculateStatistics(differenceBuffer);
        juce::DynamicObject::Ptr statsObj = new juce::DynamicObject();
        statsObj->setProperty("min", stats.min);
        statsObj->setProperty("max", stats.max);
        statsObj->setProperty("mean", stats.mean);
        statsObj->setProperty("rms", stats.rms);
        jsonData->setProperty("statistics", statsObj.get());
        
        // Данные (первые 1000 сэмплов)
        juce::Array<juce::var> dataArray;
        auto* data = differenceBuffer.getReadPointer(0);
        int samplesToExport = std::min(1000, differenceBuffer.getNumSamples());
        
        for (int i = 0; i < samplesToExport; ++i)
            dataArray.add(data[i]);
        
        jsonData->setProperty("differenceData", dataArray);
        
        return saveJsonToFile(jsonData.get(), outputFile);
    }
    
    // ========== PATTERNS ==========
    
    // Экспорт одного паттерна
    static bool exportPattern(const Pattern& pattern, const juce::File& outputFile)
    {
        auto jsonData = patternToJson(pattern);
        return saveJsonToFile(jsonData.get(), outputFile);
    }
    
    // Экспорт списка паттернов
    static bool exportPatterns(const std::vector<Pattern>& patterns, const juce::File& outputFile)
    {
        juce::DynamicObject::Ptr jsonData = new juce::DynamicObject();
        jsonData->setProperty("version", "1.0");
        jsonData->setProperty("type", "pattern_library");
        jsonData->setProperty("count", static_cast<int>(patterns.size()));
        
        juce::Array<juce::var> patternsArray;
        for (const auto& pattern : patterns)
        {
            patternsArray.add(patternToJson(pattern).get());
        }
        jsonData->setProperty("patterns", patternsArray);
        
        return saveJsonToFile(jsonData.get(), outputFile);
    }
    
    // Импорт паттерна
    static Pattern importPattern(const juce::File& inputFile)
    {
        auto jsonData = loadJsonFromFile(inputFile);
        if (jsonData.isObject())
        {
            return jsonToPattern(jsonData);
        }
        return Pattern();
    }
    
    // Импорт списка паттернов
    static std::vector<Pattern> importPatterns(const juce::File& inputFile)
    {
        std::vector<Pattern> patterns;
        auto jsonData = loadJsonFromFile(inputFile);
        
        if (jsonData.isObject())
        {
            auto patternsArray = jsonData.getProperty("patterns", juce::var());
            
            if (patternsArray.isArray())
            {
                auto* array = patternsArray.getArray();
                for (int i = 0; i < array->size(); ++i)
                {
                    patterns.push_back(jsonToPattern(array->getUnchecked(i)));
                }
            }
        }
        
        return patterns;
    }
    
    // ========== ML MODEL STATE ==========
    
    // Экспорт состояния ML (предпочтения пользователя)
    static bool exportMLState(const std::map<int, int>& patternPreferences,
                             const juce::File& outputFile)
    {
        juce::DynamicObject::Ptr jsonData = new juce::DynamicObject();
        jsonData->setProperty("version", "1.0");
        jsonData->setProperty("type", "ml_state");
        
        juce::DynamicObject::Ptr prefsObj = new juce::DynamicObject();
        for (const auto& [patternId, score] : patternPreferences)
        {
            prefsObj->setProperty(juce::String(patternId), score);
        }
        jsonData->setProperty("preferences", prefsObj.get());
        
        return saveJsonToFile(jsonData.get(), outputFile);
    }
    
    // Импорт состояния ML
    static std::map<int, int> importMLState(const juce::File& inputFile)
    {
        std::map<int, int> preferences;
        auto jsonData = loadJsonFromFile(inputFile);
        
        if (jsonData.isObject())
        {
            auto prefsVar = jsonData.getProperty("preferences", juce::var());
            if (prefsVar.isObject())
            {
                auto* prefsObj = prefsVar.getDynamicObject();
                if (prefsObj != nullptr)
                {
                    auto props = prefsObj->getProperties();
                    for (int i = 0; i < props.size(); ++i)
                    {
                        auto name = props.getName(i);
                        int patternId = name.toString().getIntValue();
                        int score = props.getValueAt(i);
                        preferences[patternId] = score;
                    }
                }
            }
        }
        
        return preferences;
    }

private:
    struct Statistics
    {
        float min = 0.0f;
        float max = 0.0f;
        float mean = 0.0f;
        float rms = 0.0f;
    };
    
    static Statistics calculateStatistics(const juce::AudioBuffer<float>& buffer)
    {
        Statistics stats;
        if (buffer.getNumSamples() == 0) return stats;
        
        auto* data = buffer.getReadPointer(0);
        int numSamples = buffer.getNumSamples();
        
        stats.min = *std::min_element(data, data + numSamples);
        stats.max = *std::max_element(data, data + numSamples);
        
        float sum = 0.0f;
        float sumSquares = 0.0f;
        
        for (int i = 0; i < numSamples; ++i)
        {
            sum += data[i];
            sumSquares += data[i] * data[i];
        }
        
        stats.mean = sum / numSamples;
        stats.rms = std::sqrt(sumSquares / numSamples);
        
        return stats;
    }
    
    // Конвертация Pattern в JSON
    static juce::DynamicObject::Ptr patternToJson(const Pattern& pattern)
    {
        juce::DynamicObject::Ptr json = new juce::DynamicObject();
        
        json->setProperty("id", pattern.getId());
        json->setProperty("type", static_cast<int>(pattern.getType()));
        json->setProperty("typeName", pattern.getTypeName());
        
        auto props = pattern.getProperties();
        juce::DynamicObject::Ptr propsJson = new juce::DynamicObject();
        propsJson->setProperty("frequencyOfOccurrence", props.frequencyOfOccurrence);
        propsJson->setProperty("durationSeconds", props.durationSeconds);
        propsJson->setProperty("intervalLines", props.intervalLines);
        propsJson->setProperty("targetLine", props.targetLine);
        propsJson->setProperty("increaseMultiplier", props.increaseMultiplier);
        propsJson->setProperty("amplitude", props.amplitude);
        propsJson->setProperty("confidence", props.confidence);
        
        json->setProperty("properties", propsJson.get());
        json->setProperty("description", pattern.getDescription());
        json->setProperty("userPreferenceScore", pattern.getUserPreferenceScore());
        
        return json;
    }
    
    // Конвертация JSON в Pattern
    static Pattern jsonToPattern(const juce::var& json)
    {
        if (!json.isObject()) return Pattern();

        int typeInt = json.getProperty("type", 0);
        PatternType type = static_cast<PatternType>(typeInt);  // ИСПРАВЛЕНО

        PatternProperties props;
        auto propsVar = json.getProperty("properties", juce::var());
        if (propsVar.isObject())
        {
            props.frequencyOfOccurrence = propsVar.getProperty("frequencyOfOccurrence", 0);
            props.durationSeconds = static_cast<float>(propsVar.getProperty("durationSeconds", 0.0));
            props.intervalLines = propsVar.getProperty("intervalLines", 0);
            props.targetLine = propsVar.getProperty("targetLine", 0);
            props.increaseMultiplier = static_cast<float>(propsVar.getProperty("increaseMultiplier", 1.0));
            props.amplitude = static_cast<float>(propsVar.getProperty("amplitude", 0.0));
            props.confidence = static_cast<float>(propsVar.getProperty("confidence", 0.0));
        }

        return Pattern(type, props);
    }
    
    // Сохранение JSON в файл
    static bool saveJsonToFile(juce::DynamicObject* jsonData, const juce::File& file)
    {
        juce::var jsonVar(jsonData);
        juce::String jsonString = juce::JSON::toString(jsonVar, true);
        return file.replaceWithText(jsonString);
    }
    
    // Загрузка JSON из файла
    static juce::var loadJsonFromFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return juce::var();
        
        juce::String jsonString = file.loadFileAsString();
        return juce::JSON::parse(jsonString);
    }
};