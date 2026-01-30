/*
PatternDetector.h
Детектор паттернов в difference data с поддержкой спектральных индексов.
Функции:

detectPatterns: главный метод, принимает difference buffer + опционально SpectralIndexDatabase
НОВОЕ: три метода с индексами:

detectTransientPatternsWithIndices: находит атаки/удары через transient indices
detectHarmonicPatternsWithIndices: находит устойчивые гармоники через peak indices
detectRhythmicPatternsWithIndices: находит ритмические паттерны через RMS energy


Fallback: старые методы без индексов (detectPeriodicSpikes, detectWavePatterns и т.д.)
Настройки чувствительности и минимальной уверенности

Контекст и связи:

Вызывается из PluginProcessor::searchForPatterns
Получает SpectralIndexDatabase для улучшенного анализа
Результаты добавляются в PatternLibrary
ВАЖНО: использование индексов значительно повышает точность детекции
*/

#pragma once
#include <JuceHeader.h>
#include "Pattern.h"
#include "SpectralIndexDatabase.h"  // НОВОЕ
#include <vector>
#include <cmath>

class PatternDetector
{
public:
    PatternDetector() = default;

    // ОБНОВЛЁННЫЙ метод: теперь принимает базу индексов
    std::vector<Pattern> detectPatterns(
        const juce::AudioBuffer<float>& differenceData,
        double sampleRate,
        SpectralIndexDatabase* indexDatabase = nullptr)  // НОВОЕ: опциональный параметр
    {
        if (differenceData.getNumSamples() == 0)
            return {};

        std::vector<Pattern> foundPatterns;

        DBG("===========================================");
        DBG("PATTERN DETECTION WITH SPECTRAL INDICES");
        DBG("===========================================");

        // Если есть база индексов - используем её для улучшенного анализа
        if (indexDatabase && indexDatabase->hasSampleLoaded())
        {
            DBG("✅ Using spectral indices for enhanced pattern detection");

            // 1. Поиск паттернов с использованием индексов
            auto transientPatterns = detectTransientPatternsWithIndices(
                differenceData, sampleRate, indexDatabase);
            foundPatterns.insert(foundPatterns.end(),
                transientPatterns.begin(), transientPatterns.end());

            auto harmonicPatterns = detectHarmonicPatternsWithIndices(
                differenceData, sampleRate, indexDatabase);
            foundPatterns.insert(foundPatterns.end(),
                harmonicPatterns.begin(), harmonicPatterns.end());

            auto rhythmicPatterns = detectRhythmicPatternsWithIndices(
                differenceData, sampleRate, indexDatabase);
            foundPatterns.insert(foundPatterns.end(),
                rhythmicPatterns.begin(), rhythmicPatterns.end());
        }
        else
        {
            DBG("⚠️ No spectral indices - using basic detection");

            // Fallback: старые методы без индексов
            auto periodicSpikes = detectPeriodicSpikes(differenceData, sampleRate);
            foundPatterns.insert(foundPatterns.end(), periodicSpikes.begin(), periodicSpikes.end());

            auto wavePatterns = detectWavePatterns(differenceData, sampleRate);
            foundPatterns.insert(foundPatterns.end(), wavePatterns.begin(), wavePatterns.end());

            auto sequences = detectSequencePatterns(differenceData, sampleRate);
            foundPatterns.insert(foundPatterns.end(), sequences.begin(), sequences.end());

            auto anomalies = detectAmplitudeAnomalies(differenceData, sampleRate);
            foundPatterns.insert(foundPatterns.end(), anomalies.begin(), anomalies.end());

            auto harmonics = detectHarmonicClusters(differenceData, sampleRate);
            foundPatterns.insert(foundPatterns.end(), harmonics.begin(), harmonics.end());
        }

        DBG("PatternDetector: Found " + juce::String(foundPatterns.size()) + " patterns");

        return foundPatterns;
    }

    // Настройки чувствительности
    void setSensitivity(float value) { sensitivity = juce::jlimit(0.0f, 1.0f, value); }
    void setMinConfidence(float value) { minConfidence = juce::jlimit(0.0f, 1.0f, value); }

private:
    float sensitivity = 0.5f;
    float minConfidence = 0.7f;

    // ==========================================================================
    // НОВЫЕ МЕТОДЫ: Детекция паттернов с использованием индексов
    // ==========================================================================

    // 1. Транзиент паттерны (атаки, удары)
    std::vector<Pattern> detectTransientPatternsWithIndices(
        const juce::AudioBuffer<float>& data,
        double sampleRate,
        SpectralIndexDatabase* indexDB)
    {
        std::vector<Pattern> patterns;

        const auto* overviewIndices = indexDB->getOverviewIndices();
        if (!overviewIndices) return patterns;

        DBG("🔍 Detecting transient patterns from indices...");

        // Собираем все транзиенты из индексов
        std::vector<int> transientPositions;

        for (int frame = 0; frame < overviewIndices->getNumFrames(); ++frame)
        {
            const auto& indexFrame = overviewIndices->getFrame(frame);

            // Проверяем есть ли транзиенты в этом фрейме
            bool hasTransient = false;
            for (const auto& index : indexFrame.indices)
            {
                if (index.isTransient)
                {
                    hasTransient = true;
                    break;
                }
            }

            if (hasTransient)
            {
                // Конвертируем время фрейма в sample position
                int samplePos = static_cast<int>(indexFrame.timePosition * sampleRate);
                transientPositions.push_back(samplePos);
            }
        }

        if (transientPositions.size() >= 3)
        {
            // Анализируем периодичность транзиентов
            std::vector<int> intervals;
            for (size_t i = 1; i < transientPositions.size(); ++i)
            {
                intervals.push_back(transientPositions[i] - transientPositions[i - 1]);
            }

            int avgInterval = 0;
            for (int interval : intervals)
                avgInterval += interval;
            avgInterval /= static_cast<int>(intervals.size());

            // Создаём паттерн
            PatternProperties props;
            props.frequencyOfOccurrence = static_cast<int>(transientPositions.size());
            props.durationSeconds = avgInterval / static_cast<float>(sampleRate);
            props.intervalLines = avgInterval;
            props.targetLine = avgInterval / 2;
            props.increaseMultiplier = 2.5f;
            props.amplitude = 0.8f;
            props.confidence = 0.9f;  // Высокая уверенность - данные из индексов!
            props.positions = transientPositions;

            patterns.emplace_back(PatternType::PeriodicSpike, props);

            DBG("  ✅ Found transient pattern: " + juce::String(transientPositions.size()) + " transients");
        }

        return patterns;
    }

    // 2. Гармонические паттерны (устойчивые частоты)
    std::vector<Pattern> detectHarmonicPatternsWithIndices(
        const juce::AudioBuffer<float>& data,
        double sampleRate,
        SpectralIndexDatabase* indexDB)
    {
        std::vector<Pattern> patterns;

        const auto* overviewIndices = indexDB->getOverviewIndices();
        if (!overviewIndices) return patterns;

        DBG("🔍 Detecting harmonic patterns from indices...");

        // Ищем устойчивые пики в спектре
        std::map<int, int> peakBinFrequency;  // bin -> сколько раз был пиком

        for (int frame = 0; frame < overviewIndices->getNumFrames(); ++frame)
        {
            const auto& indexFrame = overviewIndices->getFrame(frame);

            for (int bin = 0; bin < overviewIndices->getNumBins(); ++bin)
            {
                if (indexFrame.indices[bin].isPeak)
                {
                    peakBinFrequency[bin]++;
                }
            }
        }

        // Находим устойчивые гармоники (пики в >30% фреймов)
        int minOccurrences = overviewIndices->getNumFrames() / 3;
        std::vector<int> stablePeakBins;

        for (const auto& [bin, count] : peakBinFrequency)
        {
            if (count >= minOccurrences)
            {
                stablePeakBins.push_back(bin);
            }
        }

        if (stablePeakBins.size() >= 2)
        {
            PatternProperties props;
            props.frequencyOfOccurrence = static_cast<int>(stablePeakBins.size());
            props.durationSeconds = data.getNumSamples() / static_cast<float>(sampleRate);
            props.intervalLines = 100;
            props.targetLine = 7;
            props.increaseMultiplier = 1.5f;
            props.amplitude = 0.6f;
            props.confidence = 0.85f;

            patterns.emplace_back(PatternType::HarmonicCluster, props);

            DBG("  ✅ Found harmonic pattern: " + juce::String(stablePeakBins.size()) + " stable harmonics");
        }

        return patterns;
    }

    // 3. Ритмические паттерны (RMS энергия)
    std::vector<Pattern> detectRhythmicPatternsWithIndices(
        const juce::AudioBuffer<float>& data,
        double sampleRate,
        SpectralIndexDatabase* indexDB)
    {
        std::vector<Pattern> patterns;

        const auto* overviewIndices = indexDB->getOverviewIndices();
        if (!overviewIndices) return patterns;

        DBG("🔍 Detecting rhythmic patterns from indices...");

        // Используем RMS энергию фреймов для детекции ритма
        std::vector<float> energyProfile;
        std::vector<int> highEnergyPositions;

        float avgEnergy = 0.0f;
        for (int frame = 0; frame < overviewIndices->getNumFrames(); ++frame)
        {
            const auto& indexFrame = overviewIndices->getFrame(frame);
            energyProfile.push_back(indexFrame.rmsEnergy);
            avgEnergy += indexFrame.rmsEnergy;
        }
        avgEnergy /= overviewIndices->getNumFrames();

        float threshold = avgEnergy * 1.5f;

        // Находим пики энергии
        for (int frame = 1; frame < overviewIndices->getNumFrames() - 1; ++frame)
        {
            if (energyProfile[frame] > threshold &&
                energyProfile[frame] > energyProfile[frame - 1] &&
                energyProfile[frame] > energyProfile[frame + 1])
            {
                int samplePos = static_cast<int>(
                    overviewIndices->getFrame(frame).timePosition * sampleRate);
                highEnergyPositions.push_back(samplePos);
            }
        }

        if (highEnergyPositions.size() >= 4)
        {
            PatternProperties props;
            props.frequencyOfOccurrence = static_cast<int>(highEnergyPositions.size());
            props.durationSeconds = 0.2f;
            props.intervalLines = 500;
            props.targetLine = 5;
            props.increaseMultiplier = 2.0f;
            props.amplitude = 0.7f;
            props.confidence = 0.8f;
            props.positions = highEnergyPositions;

            patterns.emplace_back(PatternType::AmplitudeBurst, props);

            DBG("  ✅ Found rhythmic pattern: " + juce::String(highEnergyPositions.size()) + " energy peaks");
        }

        return patterns;
    }

    // ==========================================================================
    // СТАРЫЕ МЕТОДЫ (для fallback)
    // ==========================================================================

    // ... (все старые методы detectPeriodicSpikes, detectWavePatterns и т.д. остаются без изменений) ...

    std::vector<Pattern> detectPeriodicSpikes(const juce::AudioBuffer<float>& data, double sampleRate)
    {
        // ... существующий код ...
        std::vector<Pattern> patterns;
        // (оставляем весь существующий код)
        return patterns;
    }

    std::vector<Pattern> detectWavePatterns(const juce::AudioBuffer<float>& data, double sampleRate)
    {
        std::vector<Pattern> patterns;
        // (оставляем весь существующий код)
        return patterns;
    }

    std::vector<Pattern> detectSequencePatterns(const juce::AudioBuffer<float>& data, double sampleRate)
    {
        std::vector<Pattern> patterns;
        // (оставляем весь существующий код)
        return patterns;
    }

    std::vector<Pattern> detectAmplitudeAnomalies(const juce::AudioBuffer<float>& data, double sampleRate)
    {
        std::vector<Pattern> patterns;
        // (оставляем весь существующий код)
        return patterns;
    }

    std::vector<Pattern> detectHarmonicClusters(const juce::AudioBuffer<float>& data, double sampleRate)
    {
        std::vector<Pattern> patterns;
        // (оставляем весь существующий код)
        return patterns;
    }
};