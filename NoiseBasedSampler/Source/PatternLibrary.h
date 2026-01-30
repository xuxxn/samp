/*
PatternLibrary.h
Библиотека для хранения и управления коллекцией паттернов.
Функции:

addPattern, addPatterns, clearPatterns
getPatternById, removePattern
Фильтрация: getPatternsByType, getTopPatternsByPreference, getPatternsByConfidence
Сохранение/загрузка через DataSerializer
Статистика: количество каждого типа, средняя confidence

Контекст и связи:

Используется PluginProcessor для хранения найденных паттернов
UI компоненты получают паттерны через getPattern методы
PatternResearchPanel отображает и редактирует паттерны из библиотеки
MLEvolutionEngine обучается на основе сохраненных/удаленных паттернов
Thread-safe (CriticalSection)
*/

#pragma once
#include <JuceHeader.h>
#include "Pattern.h"
#include "DataSerializer.h"
#include <vector>
#include <memory>

class PatternLibrary
{
public:
    PatternLibrary() = default;
    
    // ========== УПРАВЛЕНИЕ ПАТТЕРНАМИ ==========
    
    // Добавить паттерн
    void addPattern(const Pattern& pattern)
    {
        const juce::ScopedLock sl(lock);
        patterns.push_back(pattern);
        DBG("PatternLibrary: Added pattern #" + juce::String(pattern.getId()));
    }
    
    // Добавить несколько паттернов
    void addPatterns(const std::vector<Pattern>& newPatterns)
    {
        const juce::ScopedLock sl(lock);
        patterns.insert(patterns.end(), newPatterns.begin(), newPatterns.end());
        DBG("PatternLibrary: Added " + juce::String(newPatterns.size()) + " patterns");
    }
    
    // Очистить все паттерны
    void clearPatterns()
    {
        const juce::ScopedLock sl(lock);
        patterns.clear();
        DBG("PatternLibrary: Cleared all patterns");
    }
    
    // Получить все паттерны
    std::vector<Pattern> getAllPatterns() const
    {
        const juce::ScopedLock sl(lock);
        return patterns;
    }
    
    // Получить паттерн по ID
    Pattern* getPatternById(int id)
    {
        const juce::ScopedLock sl(lock);
        for (auto& pattern : patterns)
        {
            if (pattern.getId() == id)
                return &pattern;
        }
        return nullptr;
    }
    
    // Удалить паттерн
    void removePattern(int id)
    {
        const juce::ScopedLock sl(lock);
        patterns.erase(
            std::remove_if(patterns.begin(), patterns.end(),
                [id](const Pattern& p) { return p.getId() == id; }),
            patterns.end()
        );
        DBG("PatternLibrary: Removed pattern #" + juce::String(id));
    }
    
    // Количество паттернов
    int getPatternCount() const
    {
        const juce::ScopedLock sl(lock);
        return static_cast<int>(patterns.size());
    }
    
    // ========== ФИЛЬТРАЦИЯ И ПОИСК ==========
    
    // Получить паттерны по типу
    std::vector<Pattern> getPatternsByType(PatternType type) const
    {
        const juce::ScopedLock sl(lock);
        std::vector<Pattern> filtered;
        
        for (const auto& pattern : patterns)
        {
            if (pattern.getType() == type)
                filtered.push_back(pattern);
        }
        
        return filtered;
    }
    
    // Получить топ N паттернов по предпочтениям пользователя
    std::vector<Pattern> getTopPatternsByPreference(int count) const
    {
        const juce::ScopedLock sl(lock);
        std::vector<Pattern> sorted = patterns;
        
        std::sort(sorted.begin(), sorted.end(),
            [](const Pattern& a, const Pattern& b) {
                return a.getUserPreferenceScore() > b.getUserPreferenceScore();
            });
        
        if (count < static_cast<int>(sorted.size()))
            sorted.resize(count);
        
        return sorted;
    }
    
    // Получить паттерны с confidence выше порога
    std::vector<Pattern> getPatternsByConfidence(float minConfidence) const
    {
        const juce::ScopedLock sl(lock);
        std::vector<Pattern> filtered;
        
        for (const auto& pattern : patterns)
        {
            if (pattern.getProperties().confidence >= minConfidence)
                filtered.push_back(pattern);
        }
        
        return filtered;
    }
    
    // ========== СОХРАНЕНИЕ/ЗАГРУЗКА ==========
    
    // Сохранить библиотеку паттернов
    bool saveToFile(const juce::File& file)
    {
        const juce::ScopedLock sl(lock);
        bool success = DataSerializer::exportPatterns(patterns, file);
        
        if (success)
            DBG("PatternLibrary: Saved to " + file.getFullPathName());
        else
            DBG("PatternLibrary: Failed to save");
        
        return success;
    }
    
    // Загрузить библиотеку паттернов
    bool loadFromFile(const juce::File& file)
    {
        auto loadedPatterns = DataSerializer::importPatterns(file);
        
        if (!loadedPatterns.empty())
        {
            const juce::ScopedLock sl(lock);
            patterns = loadedPatterns;
            DBG("PatternLibrary: Loaded " + juce::String(patterns.size()) + " patterns from " + file.getFullPathName());
            return true;
        }
        
        DBG("PatternLibrary: Failed to load from " + file.getFullPathName());
        return false;
    }
    
    // Добавить паттерны из файла (не заменяя текущие)
    bool importFromFile(const juce::File& file)
    {
        auto loadedPatterns = DataSerializer::importPatterns(file);
        
        if (!loadedPatterns.empty())
        {
            addPatterns(loadedPatterns);
            return true;
        }
        
        return false;
    }
    
    // ========== СТАТИСТИКА ==========
    
    struct Statistics
    {
        int totalPatterns = 0;
        int periodicSpikes = 0;
        int waveOscillations = 0;
        int sequenceDecays = 0;
        int amplitudeBursts = 0;
        int harmonicClusters = 0;
        float averageConfidence = 0.0f;
    };
    
    Statistics getStatistics() const
    {
        const juce::ScopedLock sl(lock);
        Statistics stats;
        
        stats.totalPatterns = static_cast<int>(patterns.size());
        
        float totalConfidence = 0.0f;
        
        for (const auto& pattern : patterns)
        {
            totalConfidence += pattern.getProperties().confidence;
            
            switch (pattern.getType())
            {
                case PatternType::PeriodicSpike: stats.periodicSpikes++; break;
                case PatternType::WaveOscillation: stats.waveOscillations++; break;
                case PatternType::SequenceDecay: stats.sequenceDecays++; break;
                case PatternType::AmplitudeBurst: stats.amplitudeBursts++; break;
                case PatternType::HarmonicCluster: stats.harmonicClusters++; break;
                default: break;
            }
        }
        
        if (stats.totalPatterns > 0)
            stats.averageConfidence = totalConfidence / stats.totalPatterns;
        
        return stats;
    }

private:
    std::vector<Pattern> patterns;
    mutable juce::CriticalSection lock;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternLibrary)
};