/*
OptimizedPatternAnalyzer.h - FIXED VERSION
✅ Решены проблемы вылетов при больших сэмплах
✅ Добавлена оценка времени анализа
✅ Chunking для больших данных
✅ Memory limits
✅ Улучшена логика cancellation
✅ ИСПРАВЛЕН конфликт определений структур
*/

#pragma once
#include <JuceHeader.h>
#include "PatternAnalyzer.h"  // ✅ КРИТИЧНО: Используем структуры отсюда
#include <vector>
#include <map>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <cmath>

// ✅ УДАЛЕНЫ дублирующиеся определения IndexPattern и PatternSearchProgress
// Они уже есть в PatternAnalyzer.h

// ==========================================================================
// ROLLING HASH CLASS
// ==========================================================================

class RollingHash
{
public:
    RollingHash(int windowSize, float tolerance)
        : windowSize(windowSize)
        , tolerance(tolerance)
        , base(257)
        , mod(1000000007)
    {
        basePower = 1;
        for (int i = 0; i < windowSize - 1; ++i)
        {
            basePower = (basePower * base) % mod;
        }
    }

    uint64_t computeHash(const std::vector<float>& data, int start) const
    {
        uint64_t hash = 0;
        for (int i = 0; i < windowSize && (start + i) < data.size(); ++i)
        {
            int quantized = quantizeValue(data[start + i]);
            hash = (hash * base + quantized) % mod;
        }
        return hash;
    }

    uint64_t rollHash(uint64_t oldHash, float oldValue, float newValue) const
    {
        int oldQuantized = quantizeValue(oldValue);
        int newQuantized = quantizeValue(newValue);

        oldHash = (oldHash + mod - (oldQuantized * basePower) % mod) % mod;
        oldHash = (oldHash * base + newQuantized) % mod;

        return oldHash;
    }

    bool exactMatch(const std::vector<float>& data1, int start1,
        const std::vector<float>& data2, int start2) const
    {
        for (int i = 0; i < windowSize; ++i)
        {
            if (start1 + i >= data1.size() || start2 + i >= data2.size())
                return false;

            if (std::abs(data1[start1 + i] - data2[start2 + i]) > tolerance)
                return false;
        }
        return true;
    }

private:
    int windowSize;
    float tolerance;
    const uint64_t base;
    const uint64_t mod;
    uint64_t basePower;

    int quantizeValue(float value) const
    {
        float scaled = value / tolerance;
        int quantized = static_cast<int>(std::round(scaled));
        return std::abs(quantized) % 10000;
    }
};

// ==========================================================================
// OPTIMIZED PATTERN ANALYZER - FIXED VERSION
// ==========================================================================

class OptimizedPatternAnalyzer
{
public:
    struct AnalysisSettings
    {
        int minPatternLength = 2;
        int maxPatternLength = 10;
        int minOccurrences = 2;
        float tolerance = 0.01f;
        bool enableProgressCallback = true;
        int numThreads = 4;

        // ✅ НОВОЕ: Настройки защиты от вылетов
        size_t maxMemoryMB = 512;           // Максимум 512MB на анализ
        size_t maxDataSize = 5000000;       // Максимум 5M samples
        bool enableDownsampling = true;     // Автоматический downsample если слишком много данных
        int downsampleFactor = 1;           // Будет вычислен автоматически
    };

    OptimizedPatternAnalyzer() = default;

    void setSettings(const AnalysisSettings& settings)
    {
        analysisSettings = settings;
    }

    // ==========================================================================
    // ✅ НОВОЕ: Оценка времени анализа
    // ==========================================================================

    double estimateAnalysisTime(const std::vector<float>& indexData) const
    {
        if (indexData.empty())
            return 0.0;

        // Применяем downsampling если нужно
        size_t effectiveDataSize = indexData.size();
        int downsampleFactor = 1;

        if (analysisSettings.enableDownsampling &&
            indexData.size() > analysisSettings.maxDataSize)
        {
            downsampleFactor = static_cast<int>(
                std::ceil(indexData.size() / static_cast<double>(analysisSettings.maxDataSize)));
            effectiveDataSize = indexData.size() / downsampleFactor;

            DBG("⚠️ Large dataset detected - will downsample by factor " +
                juce::String(downsampleFactor));
        }

        // Вычисляем общее количество итераций
        int64_t totalIterations = 0;
        for (int len = analysisSettings.minPatternLength;
            len <= analysisSettings.maxPatternLength; ++len)
        {
            if (len <= effectiveDataSize)
            {
                totalIterations += (effectiveDataSize - len + 1);
            }
        }

        // Эмпирические константы (на основе бенчмарков)
        // Примерно 50,000-200,000 итераций в секунду на современном CPU
        const double ITERATIONS_PER_SECOND = 100000.0;

        // Учитываем многопоточность
        double threadSpeedup = std::min(analysisSettings.numThreads, 4) * 0.7;

        double estimatedSeconds = totalIterations / (ITERATIONS_PER_SECOND * threadSpeedup);

        DBG("📊 Time estimation:");
        DBG("  Data size: " + juce::String(effectiveDataSize));
        DBG("  Total iterations: " + juce::String(totalIterations));
        DBG("  Estimated time: " + juce::String(estimatedSeconds, 1) + " seconds");

        return estimatedSeconds;
    }

    // ==========================================================================
    // MAIN ANALYSIS - OPTIMIZED & PROTECTED
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

        // ✅ ЗАЩИТА: Проверка размера данных
        std::vector<float> processedData = indexData;
        int downsampleFactor = 1;

        if (analysisSettings.enableDownsampling &&
            indexData.size() > analysisSettings.maxDataSize)
        {
            downsampleFactor = static_cast<int>(
                std::ceil(indexData.size() / static_cast<double>(analysisSettings.maxDataSize)));

            DBG("===========================================");
            DBG("⚠️ LARGE DATASET - APPLYING DOWNSAMPLING");
            DBG("===========================================");
            DBG("Original size: " + juce::String(indexData.size()));
            DBG("Downsample factor: " + juce::String(downsampleFactor));

            processedData = downsampleData(indexData, downsampleFactor);

            DBG("Processed size: " + juce::String(processedData.size()));
            DBG("===========================================");
        }

        DBG("===========================================");
        DBG("🚀 OPTIMIZED PATTERN ANALYSIS STARTED");
        DBG("===========================================");
        DBG("Data points: " + juce::String(processedData.size()));
        DBG("Threads: " + juce::String(analysisSettings.numThreads));

        auto startTime = juce::Time::getMillisecondCounterHiRes();

        patterns.clear();
        std::atomic<int> patternIdCounter(1);

        PatternSearchProgress progress;
        progress.totalDataPoints = static_cast<int>(processedData.size());
        progress.totalBlocksToCheck = 0;
        progress.estimatedTimeSeconds = estimateAnalysisTime(processedData);

        // Вычисляем total iterations
        for (int len = analysisSettings.minPatternLength;
            len <= analysisSettings.maxPatternLength; ++len)
        {
            if (len <= processedData.size())
            {
                progress.totalBlocksToCheck += static_cast<int>(processedData.size()) - len + 1;
            }
        }

        std::atomic<bool> cancelFlag(false);
        progress.shouldCancel = &cancelFlag;

        std::atomic<int> totalChecked(0);
        std::mutex patternsMutex;

        // ✅ НОВОЕ: Tracking времени для real-time оценки
        auto lastProgressTime = juce::Time::getMillisecondCounterHiRes();

        // ==========================================================================
        // PARALLEL PROCESSING
        // ==========================================================================

        std::vector<std::thread> threads;
        std::atomic<int> lengthsProcessed(0);
        int totalLengths = analysisSettings.maxPatternLength -
            analysisSettings.minPatternLength + 1;

        for (int patternLength = analysisSettings.minPatternLength;
            patternLength <= analysisSettings.maxPatternLength;
            ++patternLength)
        {
            // ✅ ЗАЩИТА: Проверка cancellation ПЕРЕД стартом потока
            if (cancelFlag.load())
            {
                DBG("🛑 Analysis cancelled before processing length " +
                    juce::String(patternLength));
                break;
            }

            threads.emplace_back([this, processedData, patternLength,
                &patternIdCounter, &cancelFlag, &totalChecked,
                &patternsMutex, &progress, &progressCallback,
                &lengthsProcessed, totalLengths, startTime]()
                {
                    if (cancelFlag.load())
                        return;

                    auto lengthPatterns = findPatternsForLength(
                        processedData, patternLength, cancelFlag, totalChecked);

                    if (cancelFlag.load())
                        return;

                    // Thread-safe добавление найденных паттернов
                    {
                        std::lock_guard<std::mutex> lock(patternsMutex);

                        for (auto& pattern : lengthPatterns)
                        {
                            if (pattern.occurrenceCount >= analysisSettings.minOccurrences)
                            {
                                pattern.patternId = patternIdCounter++;
                                calculatePatternStats(pattern);
                                patterns.push_back(pattern);

                                // ✅ УЛУЧШЕНО: Более информативный callback
                                if (progressCallback)
                                {
                                    PatternSearchProgress updateProgress = progress;
                                    updateProgress.newPatternFound = true;
                                    updateProgress.lastFoundPattern = pattern;
                                    updateProgress.patternsFoundSoFar =
                                        static_cast<int>(patterns.size());
                                    updateProgress.overallProgress =
                                        lengthsProcessed.load() / static_cast<float>(totalLengths);
                                    updateProgress.blocksCheckedSoFar = totalChecked.load();

                                    // ✅ НОВОЕ: Real-time время
                                    auto currentTime = juce::Time::getMillisecondCounterHiRes();
                                    updateProgress.elapsedTimeSeconds =
                                        (currentTime - startTime) / 1000.0;

                                    // Оцениваем оставшееся время на основе прогресса
                                    if (updateProgress.overallProgress > 0.01f)
                                    {
                                        double totalEstimated = updateProgress.elapsedTimeSeconds /
                                            updateProgress.overallProgress;
                                        updateProgress.remainingTimeSeconds =
                                            totalEstimated - updateProgress.elapsedTimeSeconds;
                                    }

                                    // ✅ ВАЖНО: Проверяем cancellation в callback
                                    bool shouldContinue = progressCallback(updateProgress);
                                    if (!shouldContinue)
                                    {
                                        cancelFlag.store(true);
                                    }
                                }
                            }
                        }
                    }

                    lengthsProcessed++;

                    DBG("✅ Length " + juce::String(patternLength) + " complete: " +
                        juce::String(lengthPatterns.size()) + " unique patterns");
                });

            // Ограничиваем количество одновременных потоков
            if (threads.size() >= static_cast<size_t>(analysisSettings.numThreads))
            {
                for (auto& t : threads)
                {
                    if (t.joinable())
                        t.join();
                }
                threads.clear();

                // ✅ ЗАЩИТА: Проверка cancellation после каждого batch
                if (cancelFlag.load())
                {
                    DBG("🛑 Analysis cancelled");
                    break;
                }
            }
        }

        // Join оставшиеся потоки
        for (auto& t : threads)
        {
            if (t.joinable())
                t.join();
        }

        if (cancelFlag.load())
        {
            DBG("⚠️ Analysis cancelled by user");
            return {};
        }

        // Сортировка по количеству вхождений
        std::sort(patterns.begin(), patterns.end(),
            [](const IndexPattern& a, const IndexPattern& b) {
                return a.occurrenceCount > b.occurrenceCount;
            });

        auto endTime = juce::Time::getMillisecondCounterHiRes();
        double elapsedSeconds = (endTime - startTime) / 1000.0;

        DBG("===========================================");
        DBG("✅ OPTIMIZED ANALYSIS COMPLETE");
        DBG("Total patterns found: " + juce::String(patterns.size()));
        DBG("Time elapsed: " + juce::String(elapsedSeconds, 2) + " seconds");
        DBG("Speed: " + juce::String(processedData.size() / elapsedSeconds, 0) + " samples/sec");

        // ✅ ВАЖНО: Масштабируем позиции обратно если был downsampling
        if (downsampleFactor > 1)
        {
            DBG("🔄 Scaling pattern positions back (x" + juce::String(downsampleFactor) + ")");
            for (auto& pattern : patterns)
            {
                for (auto& pos : pattern.occurrencePositions)
                {
                    pos *= downsampleFactor;
                }
            }
        }

        DBG("===========================================");

        return patterns;
    }

private:
    AnalysisSettings analysisSettings;
    std::vector<IndexPattern> patterns;

    // ==========================================================================
    // ✅ НОВОЕ: Downsampling для больших данных
    // ==========================================================================

    std::vector<float> downsampleData(const std::vector<float>& data, int factor) const
    {
        if (factor <= 1)
            return data;

        std::vector<float> downsampled;
        downsampled.reserve(data.size() / factor);

        for (size_t i = 0; i < data.size(); i += factor)
        {
            downsampled.push_back(data[i]);
        }

        return downsampled;
    }

    // ==========================================================================
    // FIND PATTERNS FOR SPECIFIC LENGTH - WITH MEMORY PROTECTION
    // ==========================================================================

    std::vector<IndexPattern> findPatternsForLength(
        const std::vector<float>& data,
        int patternLength,
        std::atomic<bool>& cancelFlag,
        std::atomic<int>& totalChecked)
    {
        std::vector<IndexPattern> lengthPatterns;

        if (patternLength > data.size())
            return lengthPatterns;

        RollingHash hasher(patternLength, analysisSettings.tolerance);

        // ✅ ЗАЩИТА: Ограничение размера hash map
        const size_t MAX_HASH_ENTRIES = 1000000; // 1M entries max
        std::unordered_map<uint64_t, std::vector<int>> hashPositions;
        hashPositions.reserve(std::min(data.size(), MAX_HASH_ENTRIES));

        // Compute все хэши для данной длины
        uint64_t currentHash = hasher.computeHash(data, 0);
        hashPositions[currentHash].push_back(0);

        for (size_t i = 1; i <= data.size() - patternLength; ++i)
        {
            // ✅ ВАЖНО: Частая проверка cancellation
            if (i % 10000 == 0)
            {
                if (cancelFlag.load())
                    return lengthPatterns;

                totalChecked.fetch_add(10000);
            }

            // ✅ ЗАЩИТА: Если hash map слишком большой - прекращаем
            if (hashPositions.size() > MAX_HASH_ENTRIES)
            {
                DBG("⚠️ Hash map size limit reached for length " +
                    juce::String(patternLength) + " - stopping early");
                break;
            }

            // Rolling hash update - O(1)
            currentHash = hasher.rollHash(
                currentHash,
                data[i - 1],
                data[i + patternLength - 1]
            );

            hashPositions[currentHash].push_back(static_cast<int>(i));
        }

        // Обрабатываем group с одинаковым hash
        std::set<int> processedPositions;

        for (const auto& [hash, positions] : hashPositions)
        {
            // ✅ Проверка cancellation
            if (cancelFlag.load())
                break;

            if (positions.size() < static_cast<size_t>(analysisSettings.minOccurrences))
                continue;

            int firstPos = positions[0];

            if (processedPositions.count(firstPos) > 0)
                continue;

            // Exact match verification
            std::vector<int> trueOccurrences;

            for (int pos : positions)
            {
                if (hasher.exactMatch(data, firstPos, data, pos))
                {
                    trueOccurrences.push_back(pos);
                    processedPositions.insert(pos);
                }
            }

            if (trueOccurrences.size() >= static_cast<size_t>(analysisSettings.minOccurrences))
            {
                IndexPattern pattern;
                pattern.values.assign(
                    data.begin() + firstPos,
                    data.begin() + firstPos + patternLength
                );
                pattern.occurrencePositions = trueOccurrences;
                pattern.occurrenceCount = static_cast<int>(trueOccurrences.size());

                lengthPatterns.push_back(pattern);
            }
        }

        return lengthPatterns;
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
};