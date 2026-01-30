/*
PatternAnalyzer.h - С РАСШИРЕННЫМ PROGRESS CALLBACK
✅ Detailed progress information для визуализации
✅ Real-time pattern discovery notifications
✅ Thread-safe updates
*/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <map>
#include <atomic>

// ==========================================================================
// PATTERN STRUCTURE
// ==========================================================================

struct IndexPattern
{
    int patternId;
    std::vector<float> values;
    std::vector<int> occurrencePositions;
    int occurrenceCount;
    float averageValue;
    float variance;

    IndexPattern()
        : patternId(0), occurrenceCount(0), averageValue(0.0f), variance(0.0f) {
    }
};

// ==========================================================================
// PROGRESS INFO STRUCTURE - ДЛЯ ВИЗУАЛИЗАЦИИ
// ==========================================================================

struct PatternSearchProgress
{
    float overallProgress = 0.0f;
    int currentPatternLength = 0;
    int currentStartPosition = 0;
    int totalDataPoints = 0;
    int patternsFoundSoFar = 0;
    int totalBlocksToCheck = 0;
    int blocksCheckedSoFar = 0;
    bool newPatternFound = false;
    IndexPattern lastFoundPattern;
    std::atomic<bool>* shouldCancel = nullptr;

    // ✅ НОВОЕ: Оценка времени
    double estimatedTimeSeconds = 0.0;
    double elapsedTimeSeconds = 0.0;
    double remainingTimeSeconds = 0.0;
};

// ==========================================================================
// PATTERN ANALYZER - С ВИЗУАЛИЗАЦИЕЙ
// ==========================================================================

class PatternAnalyzer
{
public:
    PatternAnalyzer() = default;

    // ==========================================================================
    // CONFIGURATION
    // ==========================================================================

    struct AnalysisSettings
    {
        int minPatternLength = 2;
        int maxPatternLength = 10;
        int minOccurrences = 2;
        float tolerance = 0.01f;
        bool enableProgressCallback = true;
    };

    void setSettings(const AnalysisSettings& settings)
    {
        analysisSettings = settings;
    }

    // ==========================================================================
    // PATTERN DETECTION - С DETAILED PROGRESS
    // ==========================================================================

    std::vector<IndexPattern> analyzeIndex(
        const std::vector<float>& indexData,
        std::function<bool(const PatternSearchProgress&)> progressCallback = nullptr)
    {
        if (indexData.size() < static_cast<size_t>(analysisSettings.minPatternLength))
        {
            DBG("⚠️ Index too short for pattern detection");
            return {};
        }

        DBG("===========================================");
        DBG("PATTERN ANALYSIS STARTED (WITH VISUALIZATION)");
        DBG("===========================================");
        DBG("Data points: " + juce::String(indexData.size()));
        DBG("Min pattern length: " + juce::String(analysisSettings.minPatternLength));
        DBG("Max pattern length: " + juce::String(analysisSettings.maxPatternLength));

        patterns.clear();
        int patternIdCounter = 1;

        // Вычисляем общее количество итераций
        int totalIterations = 0;
        for (int len = analysisSettings.minPatternLength; len <= analysisSettings.maxPatternLength; ++len)
        {
            totalIterations += static_cast<int>(indexData.size()) - len + 1;
        }

        PatternSearchProgress progress;
        progress.totalDataPoints = static_cast<int>(indexData.size());
        progress.totalBlocksToCheck = totalIterations;
        progress.blocksCheckedSoFar = 0;
        progress.patternsFoundSoFar = 0;

        std::atomic<bool> cancelFlag(false);
        progress.shouldCancel = &cancelFlag;

        int currentIteration = 0;

        // Основной цикл анализа
        for (int patternLength = analysisSettings.minPatternLength;
            patternLength <= analysisSettings.maxPatternLength && !cancelFlag.load();
            ++patternLength)
        {
            progress.currentPatternLength = patternLength;

            for (size_t startPos = 0;
                startPos <= indexData.size() - patternLength && !cancelFlag.load();
                ++startPos)
            {
                progress.currentStartPosition = static_cast<int>(startPos);

                // Извлекаем потенциальный паттерн
                std::vector<float> candidate(
                    indexData.begin() + startPos,
                    indexData.begin() + startPos + patternLength
                );

                // Проверяем, не был ли уже найден
                if (isPatternAlreadyFound(candidate))
                {
                    currentIteration++;
                    progress.blocksCheckedSoFar = currentIteration;
                    continue;
                }

                // Ищем вхождения
                std::vector<int> occurrences = findOccurrences(indexData, candidate, startPos);

                // Если нашли достаточно вхождений
                if (static_cast<int>(occurrences.size()) >= analysisSettings.minOccurrences)
                {
                    IndexPattern pattern;
                    pattern.patternId = patternIdCounter++;
                    pattern.values = candidate;
                    pattern.occurrencePositions = occurrences;
                    pattern.occurrenceCount = static_cast<int>(occurrences.size());

                    calculatePatternStats(pattern);
                    patterns.push_back(pattern);

                    // Уведомляем о новом паттерне
                    progress.newPatternFound = true;
                    progress.lastFoundPattern = pattern;
                    progress.patternsFoundSoFar = static_cast<int>(patterns.size());

                    DBG("✅ Pattern #" + juce::String(pattern.patternId) +
                        " found: length=" + juce::String(patternLength) +
                        ", occurrences=" + juce::String(pattern.occurrenceCount));
                }
                else
                {
                    progress.newPatternFound = false;
                }

                // Обновляем прогресс
                currentIteration++;
                progress.blocksCheckedSoFar = currentIteration;
                progress.overallProgress = currentIteration / static_cast<float>(totalIterations);

                // Callback для обновления UI
                if (progressCallback && analysisSettings.enableProgressCallback)
                {
                    // Вызываем не чаще чем каждые 50 итераций (для производительности)
                    if (currentIteration % 50 == 0 || progress.newPatternFound)
                    {
                        bool shouldContinue = progressCallback(progress);
                        if (!shouldContinue)
                        {
                            cancelFlag.store(true);
                            DBG("⚠️ Analysis cancelled by user");
                            break;
                        }
                    }
                }
            }

            // Оптимизация: прекращаем поиск более длинных паттернов
            if (!cancelFlag.load() && patternLength > analysisSettings.minPatternLength)
            {
                bool foundAnyAtThisLength = false;
                for (const auto& p : patterns)
                {
                    if (static_cast<int>(p.values.size()) == patternLength)
                    {
                        foundAnyAtThisLength = true;
                        break;
                    }
                }

                if (!foundAnyAtThisLength && patternLength > analysisSettings.minPatternLength + 2)
                {
                    DBG("No patterns of length " + juce::String(patternLength) +
                        " found, stopping search");
                    break;
                }
            }
        }

        // Финальный callback
        if (progressCallback)
        {
            progress.overallProgress = 1.0f;
            progress.blocksCheckedSoFar = totalIterations;
            progressCallback(progress);
        }

        if (cancelFlag.load())
        {
            DBG("===========================================");
            DBG("PATTERN ANALYSIS CANCELLED");
            DBG("Partial results: " + juce::String(patterns.size()) + " patterns");
            DBG("===========================================");
            return {};
        }

        DBG("===========================================");
        DBG("PATTERN ANALYSIS COMPLETE");
        DBG("Total patterns found: " + juce::String(patterns.size()));
        DBG("===========================================");

        // Сортируем по количеству появлений
        std::sort(patterns.begin(), patterns.end(),
            [](const IndexPattern& a, const IndexPattern& b) {
                return a.occurrenceCount > b.occurrenceCount;
            });

        return patterns;
    }

    // ==========================================================================
    // RESULTS ACCESS
    // ==========================================================================

    const std::vector<IndexPattern>& getPatterns() const
    {
        return patterns;
    }

private:
    AnalysisSettings analysisSettings;
    std::vector<IndexPattern> patterns;

    // ==========================================================================
    // HELPER METHODS
    // ==========================================================================

    bool valuesMatch(float a, float b) const
    {
        return std::abs(a - b) <= analysisSettings.tolerance;
    }

    bool patternsMatch(const std::vector<float>& a, const std::vector<float>& b) const
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
        {
            if (!valuesMatch(a[i], b[i]))
                return false;
        }

        return true;
    }

    bool isPatternAlreadyFound(const std::vector<float>& candidate) const
    {
        for (const auto& existingPattern : patterns)
        {
            if (patternsMatch(candidate, existingPattern.values))
                return true;
        }
        return false;
    }

    std::vector<int> findOccurrences(
        const std::vector<float>& data,
        const std::vector<float>& pattern,
        size_t skipUntil = 0) const
    {
        std::vector<int> occurrences;

        for (size_t pos = skipUntil; pos <= data.size() - pattern.size(); ++pos)
        {
            bool matches = true;

            for (size_t i = 0; i < pattern.size(); ++i)
            {
                if (!valuesMatch(data[pos + i], pattern[i]))
                {
                    matches = false;
                    break;
                }
            }

            if (matches)
            {
                occurrences.push_back(static_cast<int>(pos));
            }
        }

        return occurrences;
    }

    void calculatePatternStats(IndexPattern& pattern)
    {
        if (pattern.values.empty())
            return;

        float sum = 0.0f;
        for (float val : pattern.values)
            sum += val;

        pattern.averageValue = sum / pattern.values.size();

        float varianceSum = 0.0f;
        for (float val : pattern.values)
        {
            float diff = val - pattern.averageValue;
            varianceSum += diff * diff;
        }
        pattern.variance = varianceSum / pattern.values.size();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternAnalyzer)
};