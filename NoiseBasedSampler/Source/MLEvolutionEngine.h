/*
MLEvolutionEngine.h
ML движок для обучения на предпочтениях пользователя.
Функции:

registerPatternSaved: пользователь сохранил паттерн → увеличить приоритет этого типа
registerPatternDeleted: пользователь удалил паттерн → уменьшить приоритет
registerPatternEdited: анализ изменений для выявления трендов
getTypePriority: влияние на детекцию (приоритет 0.5-1.5)
shouldPrioritizeType: рекомендация искать этот тип активнее
Статистика обучения и прогресс (0-1)
Сохранение/загрузка состояния

Контекст и связи:

Интегрируется с PatternResearchPanel через PluginProcessor
Влияет на PatternDetector через приоритеты типов
Обучается на действиях пользователя в реальном времени
Хранит предпочтения конкретных паттернов и типов
Анализирует тренды редактирования (duration up/down, multiplier changes и т.д.)
Thread-safe (CriticalSection)
*/

#pragma once
#include <JuceHeader.h>
#include "Pattern.h"
#include "DataSerializer.h"
#include <map>
#include <vector>

class MLEvolutionEngine
{
public:
    MLEvolutionEngine() = default;
    
    // ========== ОБУЧЕНИЕ НА ПРЕДПОЧТЕНИЯХ ==========
    
    // Пользователь сохранил паттерн → ML запоминает что такие нравятся
    void registerPatternSaved(const Pattern& pattern)
    {
        const juce::ScopedLock sl(lock);
        
        int patternId = pattern.getId();
        PatternType type = pattern.getType();
        
        // Увеличиваем счетчик для конкретного паттерна
        patternPreferences[patternId]++;
        
        // Увеличиваем счетчик для типа паттерна
        typePreferences[type]++;
        
        // Запоминаем характеристики паттерна
        learnPatternCharacteristics(pattern);
        
        totalInteractions++;
        
        DBG("MLEvolution: User saved pattern #" + juce::String(patternId) + 
            " (type: " + pattern.getTypeName() + ")");
    }
    
    // Пользователь удалил паттерн → ML запоминает что такие не нравятся
    void registerPatternDeleted(const Pattern& pattern)
    {
        const juce::ScopedLock sl(lock);
        
        PatternType type = pattern.getType();
        
        // Уменьшаем предпочтение для типа
        typePreferences[type] = std::max(0, typePreferences[type] - 1);
        
        DBG("MLEvolution: User deleted pattern type " + pattern.getTypeName());
    }
    
    // Пользователь отредактировал паттерн → ML учится на изменениях
    void registerPatternEdited(const Pattern& oldPattern, const Pattern& newPattern)
    {
        const juce::ScopedLock sl(lock);
        
        // Анализируем что изменилось
        auto oldProps = oldPattern.getProperties();
        auto newProps = newPattern.getProperties();
        
        // Запоминаем тренды изменений
        if (newProps.increaseMultiplier > oldProps.increaseMultiplier)
            editTrends["increase_multiplier_up"]++;
        else if (newProps.increaseMultiplier < oldProps.increaseMultiplier)
            editTrends["increase_multiplier_down"]++;
        
        if (newProps.durationSeconds > oldProps.durationSeconds)
            editTrends["duration_up"]++;
        else if (newProps.durationSeconds < oldProps.durationSeconds)
            editTrends["duration_down"]++;
        
        DBG("MLEvolution: User edited pattern, learning preferences...");
    }
    
    // ========== ВЛИЯНИЕ НА ДЕТЕКЦИЮ ==========
    
    // Получить приоритет типа паттерна (чем выше - тем больше искать)
    float getTypePriority(PatternType type) const
    {
        const juce::ScopedLock sl(lock);
        
        auto it = typePreferences.find(type);
        if (it == typePreferences.end() || totalInteractions == 0)
            return 1.0f; // Базовый приоритет
        
        // Нормализуем: 0.5 до 1.5 (чтобы не игнорировать другие типы)
        float normalized = static_cast<float>(it->second) / totalInteractions * 5.0f;
        return juce::jlimit(0.5f, 1.5f, normalized);
    }
    
    // Получить все приоритеты типов
    std::map<PatternType, float> getAllTypePriorities() const
    {
        std::map<PatternType, float> priorities;
        
        priorities[PatternType::PeriodicSpike] = getTypePriority(PatternType::PeriodicSpike);
        priorities[PatternType::WaveOscillation] = getTypePriority(PatternType::WaveOscillation);
        priorities[PatternType::SequenceDecay] = getTypePriority(PatternType::SequenceDecay);
        priorities[PatternType::AmplitudeBurst] = getTypePriority(PatternType::AmplitudeBurst);
        priorities[PatternType::HarmonicCluster] = getTypePriority(PatternType::HarmonicCluster);
        
        return priorities;
    }
    
    // Рекомендация: стоит ли искать этот тип паттерна активнее
    bool shouldPrioritizeType(PatternType type) const
    {
        return getTypePriority(type) > 1.1f;
    }
    
    // ========== СТАТИСТИКА ==========
    
    struct Statistics
    {
        int totalInteractions = 0;
        int mostPreferredType = 0;
        juce::String mostPreferredTypeName;
        float evolutionProgress = 0.0f; // 0-1, насколько обучена модель
        std::map<PatternType, int> typeScores;
    };
    
    Statistics getStatistics() const
    {
        const juce::ScopedLock sl(lock);
        Statistics stats;
        
        stats.totalInteractions = totalInteractions;
        stats.typeScores = typePreferences;
        
        // Находим самый предпочитаемый тип
        int maxScore = 0;
        PatternType mostPreferred = PatternType::Unknown;
        
        for (const auto& [type, score] : typePreferences)
        {
            if (score > maxScore)
            {
                maxScore = score;
                mostPreferred = type;
            }
        }
        
        stats.mostPreferredType = static_cast<int>(mostPreferred);
        
        // Прогресс обучения (0-1)
        // Считаем что после 20 взаимодействий модель хорошо обучена
        stats.evolutionProgress = std::min(1.0f, totalInteractions / 20.0f);
        
        return stats;
    }
    
    // ========== СОХРАНЕНИЕ/ЗАГРУЗКА ==========
    
    bool saveState(const juce::File& file)
    {
        const juce::ScopedLock sl(lock);
        
        // Конвертируем в map<int, int> для сериализации
        std::map<int, int> prefsAsInt;
        for (const auto& [type, score] : typePreferences)
        {
            prefsAsInt[static_cast<int>(type)] = score;
        }
        
        bool success = DataSerializer::exportMLState(prefsAsInt, file);
        
        if (success)
            DBG("MLEvolution: State saved to " + file.getFullPathName());
        
        return success;
    }
    
    bool loadState(const juce::File& file)
    {
        auto prefsAsInt = DataSerializer::importMLState(file);
        
        if (!prefsAsInt.empty())
        {
            const juce::ScopedLock sl(lock);
            
            typePreferences.clear();
            for (const auto& [typeInt, score] : prefsAsInt)
            {
                typePreferences[static_cast<PatternType>(typeInt)] = score;
                totalInteractions += score;
            }
            
            DBG("MLEvolution: State loaded from " + file.getFullPathName());
            return true;
        }
        
        return false;
    }
    
    // Сброс обучения
    void reset()
    {
        const juce::ScopedLock sl(lock);
        
        patternPreferences.clear();
        typePreferences.clear();
        characteristicAverages.clear();
        editTrends.clear();
        totalInteractions = 0;
        
        DBG("MLEvolution: Reset to initial state");
    }
    
    // Получить карту предпочтений (для UI)
    std::map<int, int> getPatternPreferences() const
    {
        const juce::ScopedLock sl(lock);
        return patternPreferences;
    }

private:
    mutable juce::CriticalSection lock;
    
    // Предпочтения конкретных паттернов (ID → score)
    std::map<int, int> patternPreferences;
    
    // Предпочтения типов паттернов (Type → score)
    std::map<PatternType, int> typePreferences;
    
    // Средние характеристики предпочитаемых паттернов
    std::map<juce::String, float> characteristicAverages;
    
    // Тренды редактирования
    std::map<juce::String, int> editTrends;
    
    // Общее количество взаимодействий
    int totalInteractions = 0;
    
    // Обучение на характеристиках паттерна
    void learnPatternCharacteristics(const Pattern& pattern)
    {
        auto props = pattern.getProperties();
        
        // Обновляем средние значения характеристик
        updateAverage("duration", props.durationSeconds);
        updateAverage("interval", static_cast<float>(props.intervalLines));
        updateAverage("multiplier", props.increaseMultiplier);
        updateAverage("amplitude", props.amplitude);
    }
    
    void updateAverage(const juce::String& key, float value)
    {
        auto it = characteristicAverages.find(key);
        if (it == characteristicAverages.end())
        {
            characteristicAverages[key] = value;
        }
        else
        {
            // Плавное обновление среднего
            characteristicAverages[key] = it->second * 0.9f + value * 0.1f;
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MLEvolutionEngine)
};