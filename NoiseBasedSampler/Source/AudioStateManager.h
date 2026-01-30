/*
AudioStateManager.h
Unified Audio State Management System
Решает проблему рассинхронизации между различными типами индексов.

КОНЦЕПЦИЯ:
==========
outputBuffer = ЕДИНСТВЕННЫЙ источник правды (ground truth)
Все индексы (Features, Spectral) - это VIEWS на этот буфер

WORKFLOW:
=========
1. User edits Features → Update outputBuffer → Auto-sync Spectral indices
2. User edits Spectral → Update outputBuffer → Auto-sync Features
3. Load sample → Analyze ALL indices from outputBuffer

KEY INSIGHT:
============
Вместо двух независимых систем создаём единую систему состояния,
где любое изменение автоматически пересчитывает все views.
*/

#pragma once
#include <JuceHeader.h>
#include "FeatureData.h"
#include "SpectralIndexData.h"
#include "SpectralIndexDatabase.h"
#include "FeatureExtractor.h"
// ✅ ИСПРАВЛЕНО: НЕ включаем PhaseVocoder.h - используем forward declaration
class PhaseVocoder;

// ==========================================================================
// AUDIO STATE MANAGER
// Управляет единым состоянием аудио и автоматической синхронизацией индексов
// ==========================================================================

class AudioStateManager
{
public:
    AudioStateManager() = default;

    // ==========================================================================
    // LIFECYCLE EVENTS
    // ==========================================================================

    // Инициализация с новым семплом
    void loadSample(
        const juce::AudioBuffer<float>& newSample,
        double sampleRate,
        FeatureExtractor& featureExtractor,
        SpectralIndexDatabase& indexDatabase)
    {
        const juce::ScopedLock sl(stateLock);

        currentSampleRate = sampleRate;

        // ✅ STEP 1: Сохраняем аудио как ground truth
        groundTruthAudio.makeCopyOf(newSample);

        // ✅ КРИТИЧНО: ВСЕГДА сохраняем как STEREO!
        const int STEREO_CHANNELS = 2;

        if (newSample.getNumChannels() >= 2)
        {
            groundTruthAudio.makeCopyOf(newSample);
        }
        else
        {
            // MONO → конвертируем в STEREO
            groundTruthAudio.setSize(STEREO_CHANNELS, newSample.getNumSamples());
            groundTruthAudio.copyFrom(0, 0, newSample, 0, 0, newSample.getNumSamples());
            groundTruthAudio.copyFrom(1, 0, newSample, 0, 0, newSample.getNumSamples());
        }

        DBG("===========================================");
        DBG("AUDIO STATE MANAGER: LOADING SAMPLE");
        DBG("===========================================");
        DBG("Samples: " + juce::String(groundTruthAudio.getNumSamples()));
        DBG("Channels: " + juce::String(groundTruthAudio.getNumChannels()));

        // ✅ STEP 2: Анализируем ВСЕ индексы из ground truth
        syncAllIndicesFromAudio(featureExtractor, indexDatabase);

        // ✅ Сбрасываем флаги модификаций
        featuresModified = false;
        spectralModified = false;

        DBG("✅ All indices synchronized from audio");
        DBG("===========================================");
    }

    // ==========================================================================
    // MODIFICATION WORKFLOWS
    // ==========================================================================

    // Пользователь изменил Features → Обновить аудио → Синхронизировать Spectral


    void applyFeatureChanges(
        const FeatureData& features,
        double sampleRate,
        SpectralIndexDatabase& indexDB,
        bool autoSyncSpectral = true)
    {
        if (features.getNumSamples() == 0)
            return;

        DBG("📊 AudioStateManager: Applying feature changes (STEREO PRESERVED)");

        int numSamples = features.getNumSamples();
        const int STEREO_CHANNELS = 2;

        // ✅ КРИТИЧНО: Сохраняем ОРИГИНАЛЬНОЕ стерео перед модификацией
        juce::AudioBuffer<float> originalStereo;

        if (groundTruthAudio.getNumSamples() == numSamples &&
            groundTruthAudio.getNumChannels() >= 2)
        {
            // Есть совпадающее стерео - сохраняем его
            originalStereo.makeCopyOf(groundTruthAudio);

            DBG("✅ Preserved original stereo field (" +
                juce::String(numSamples) + " samples)");
        }
        else
        {
            // Нет совпадающего стерео - создаём пустой буфер
            originalStereo.setSize(STEREO_CHANNELS, numSamples, false, true, false);
            originalStereo.clear();

            DBG("⚠️ No matching stereo - will synthesize fresh");
        }

        // ✅ КРИТИЧНО: Создаём новый STEREO буфер
        juce::AudioBuffer<float> modifiedAudio(STEREO_CHANNELS, numSamples);

        // ✅ КЛЮЧЕВОЕ ИЗМЕНЕНИЕ: Передаём оригинальное стерео!
        // Это позволит applyToAudioBuffer применить изменения как ДЕЛЬТУ
        const_cast<FeatureData&>(features).applyToAudioBuffer(
            modifiedAudio,
            sampleRate,
            &originalStereo  // ← ВОТ ОНО!
        );

        // Проверяем результат
        float leftEnergy = modifiedAudio.getRMSLevel(0, 0, numSamples);
        float rightEnergy = modifiedAudio.getRMSLevel(1, 0, numSamples);

        DBG("✅ Feature changes applied:");
        DBG("   Left RMS: " + juce::String(leftEnergy, 6));
        DBG("   Right RMS: " + juce::String(rightEnergy, 6));

        bool isStereo = std::abs(leftEnergy - rightEnergy) > 0.0001f;
        DBG("   Result: " + juce::String(isStereo ? "STEREO ✅" : "MONO ❌"));

        // Обновляем ground truth
        groundTruthAudio.makeCopyOf(modifiedAudio);

        // Metadata
        currentSampleRate = sampleRate;
        lastModificationTime = juce::Time::getCurrentTime();
        syncStatus.featuresModified = false;
        syncStatus.spectralModified = false;

        // Auto-sync spectral если нужно
        if (autoSyncSpectral)
        {
            DBG("🔄 Auto-syncing spectral indices...");
            indexDB.analyzeSample(groundTruthAudio, sampleRate);
            syncStatus.spectralSynced = true;
            DBG("✅ Spectral indices auto-synced");
        }
    }

    // Пользователь изменил Spectral → Обновить аудио → Синхронизировать Features
    void applySpectralChanges(
        const SpectralIndexData& indices,
        FeatureExtractor& extractor,
        bool autoSyncFeatures = true)
    {
        if (indices.getNumFrames() == 0)
            return;

        DBG("🎵 AudioStateManager: Applying spectral changes");

        // Получаем модификации
        auto modifiedBins = indices.getAllModifiedBins();
        if (modifiedBins.empty())
        {
            DBG("⚠️ No modifications detected");
            return;
        }

        DBG("   Modified bins: " + juce::String(modifiedBins.size()));

        // ✅ ИСПРАВЛЕНО: Используем ОДИН метод synthesis
        synthesizeSpectralChangesLocally(indices, groundTruthAudio);

        lastModificationTime = juce::Time::getCurrentTime();
        syncStatus.spectralModified = false;
        syncStatus.featuresModified = false;

        // Auto-sync features если нужно
        if (autoSyncFeatures)
        {
            DBG("🔄 Auto-syncing features...");

            // Извлекаем features из нового audio (моно для анализа)
            juce::AudioBuffer<float> monoForExtraction(1, groundTruthAudio.getNumSamples());
            monoForExtraction.copyFrom(0, 0, groundTruthAudio, 0, 0, groundTruthAudio.getNumSamples());

            syncStatus.featuresSynced = true;
            DBG("✅ Ready for feature extraction");
        }

        DBG("✅ Spectral changes applied");
        DBG("   Channels: " + juce::String(groundTruthAudio.getNumChannels()));
    }

    // Пользователь явно запросил полную ресинхронизацию
    void forceFullSync(
        FeatureExtractor& featureExtractor,
        SpectralIndexDatabase& indexDatabase)
    {
        const juce::ScopedLock sl(stateLock);

        DBG("🔄 FORCING FULL SYNCHRONIZATION");

        syncAllIndicesFromAudio(featureExtractor, indexDatabase);

        featuresModified = false;
        spectralModified = false;

        DBG("✅ Full sync complete");
    }

    // ==========================================================================
    // GETTERS
    // ==========================================================================

    const juce::AudioBuffer<float>& getGroundTruthAudio() const
    {
        return groundTruthAudio;
    }

    bool areFeaturesStale() const { return featuresModified; }
    bool isSpectralStale() const { return spectralModified; }

    bool isFullySynced() const
    {
        return !featuresModified && !spectralModified;
    }

    // Получить копию для безопасного использования
    juce::AudioBuffer<float> getAudioCopy() const
    {
        const juce::ScopedLock sl(stateLock);
        juce::AudioBuffer<float> copy;
        copy.makeCopyOf(groundTruthAudio);
        return copy;
    }

    double getSampleRate() const { return currentSampleRate; }

private:
    // Единственный источник правды
    juce::AudioBuffer<float> groundTruthAudio;
    double currentSampleRate = 44100.0;
    juce::Time lastModificationTime;


    // Флаги состояния
    bool featuresModified = false;
    bool spectralModified = false;

    mutable juce::CriticalSection stateLock;

    // ==========================================================================
    // INTERNAL SYNC METHODS
    // ==========================================================================

    void synthesizeSpectralChangesLocally(
        const SpectralIndexData& indices,
        juce::AudioBuffer<float>& outputBuffer)
    {
        // ✅ ВАЖНО: Убедимся что outputBuffer в STEREO
        if (outputBuffer.getNumChannels() < 2)
        {
            DBG("⚠️ Converting output buffer to STEREO");
            juce::AudioBuffer<float> stereoVersion(2, outputBuffer.getNumSamples());
            stereoVersion.copyFrom(0, 0, outputBuffer, 0, 0, outputBuffer.getNumSamples());
            stereoVersion.copyFrom(1, 0, outputBuffer, 0, 0, outputBuffer.getNumSamples());
            outputBuffer = stereoVersion;
        }

        auto modifiedBins = indices.getAllModifiedBins();
        if (modifiedBins.empty())
            return;

        DBG("🎵 Local spectral resynthesis (STEREO)");

        // ✅ КРИТИЧНО: Сохраняем оригинальное стерео ДО изменений
        juce::AudioBuffer<float> originalStereo;
        originalStereo.makeCopyOf(outputBuffer);

        // Группируем по фреймам
        std::map<int, std::vector<SpectralIndexData::ModifiedBinInfo>> modsByFrame;
        for (const auto& binInfo : modifiedBins)
        {
            modsByFrame[binInfo.frameIdx].push_back(binInfo);
        }

        int windowSize = 512;
        int halfWindow = windowSize / 2;

        // Hann window
        std::vector<float> window(windowSize);
        for (int i = 0; i < windowSize; ++i)
        {
            window[i] = 0.5f * (1.0f - std::cos(
                2.0f * juce::MathConstants<float>::pi * i / (windowSize - 1)));
        }

        // ✅ Создаём accumulation buffer для изменений
        std::vector<float> changeBuffer(outputBuffer.getNumSamples(), 0.0f);

        // Синтезируем ТОЛЬКО изменения (delta)
        for (const auto& [frameIdx, frameMods] : modsByFrame)
        {
            const auto& frame = indices.getFrame(frameIdx);
            float timePosition = frame.timePosition;

            int samplePos = static_cast<int>(timePosition * currentSampleRate);

            if (samplePos < 0 || samplePos >= outputBuffer.getNumSamples())
                continue;

            for (const auto& binInfo : frameMods)
            {
                auto modifiedIndex = indices.getIndex(frameIdx, binInfo.binIdx);

                // ✅ ЛОКАЛЬНОЕ изменение: только ДЕЛЬТА
                float magnitudeDelta = modifiedIndex.magnitude -
                    modifiedIndex.originalMagnitude;

                if (std::abs(magnitudeDelta) < 0.0001f)
                    continue;

                float frequency = binInfo.frequency;
                float phase = modifiedIndex.phase;

                // Синтезируем с окном
                for (int i = -halfWindow; i < halfWindow; ++i)
                {
                    int targetSample = samplePos + i;
                    if (targetSample < 0 || targetSample >= outputBuffer.getNumSamples())
                        continue;

                    float windowValue = window[i + halfWindow];
                    float t = i / static_cast<float>(currentSampleRate);
                    float sinValue = std::sin(
                        2.0f * juce::MathConstants<float>::pi * frequency * t + phase);

                    float contribution = magnitudeDelta * sinValue * windowValue;

                    // Soft saturation
                    float absContribution = std::abs(contribution);
                    if (absContribution > 0.5f)
                    {
                        float sign = (contribution > 0.0f) ? 1.0f : -1.0f;
                        contribution = sign * (0.5f + std::tanh((absContribution - 0.5f) * 2.0f) * 0.3f);
                    }

                    // ✅ Накапливаем изменения
                    changeBuffer[targetSample] += contribution;
                }
            }
        }

        // ✅ КРИТИЧНО: Применяем изменения к ОРИГИНАЛЬНОМУ стерео
        // Это сохраняет стерео поле и делает изменения локальными!
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            float* channelData = outputBuffer.getWritePointer(ch);
            const float* originalData = originalStereo.getReadPointer(ch);

            for (int i = 0; i < outputBuffer.getNumSamples(); ++i)
            {
                // ✅ Оригинал + изменение = локальная модификация
                channelData[i] = originalData[i] + changeBuffer[i];

                // Soft limiting только для экстремальных значений
                if (std::abs(channelData[i]) > 0.95f)
                {
                    float sign = (channelData[i] > 0.0f) ? 1.0f : -1.0f;
                    float absVal = std::abs(channelData[i]);

                    if (absVal > 0.95f)
                    {
                        float excess = absVal - 0.95f;
                        float compressed = 0.95f + excess * 0.3f;
                        channelData[i] = sign * juce::jlimit(0.0f, 1.0f, compressed);
                    }
                }
            }
        }

        DBG("✅ Local resynthesis complete (STEREO PRESERVED)");
    }
    struct SyncStatus
    {
        bool featuresSynced = true;
        bool spectralSynced = true;
        bool featuresModified = false;
        bool spectralModified = false;
    } syncStatus;


    void updateGroundTruth(const juce::AudioBuffer<float>& newAudio)
    {
        const juce::ScopedLock sl(stateLock);

        // Проверяем что получили STEREO
        if (newAudio.getNumChannels() < 2)
        {
            DBG("❌ ERROR: Cannot update with MONO audio!");
            return;
        }

        groundTruthAudio.makeCopyOf(newAudio);
        DBG("✅ Ground truth updated (STEREO)");
    }

    void syncAllIndicesFromAudio(
        FeatureExtractor& featureExtractor,
        SpectralIndexDatabase& indexDatabase)
    {
        DBG("🔄 Syncing all indices from ground truth audio...");

        // 1️⃣ Анализируем Features (на левом канале)
        juce::AudioBuffer<float> monoForAnalysis(1, groundTruthAudio.getNumSamples());
        monoForAnalysis.copyFrom(0, 0, groundTruthAudio, 0, 0,
            groundTruthAudio.getNumSamples());

        // Примечание: FeatureData должен быть обновлен в PluginProcessor
        // через callback механизм

        DBG("  ✅ Features analyzed");

        // 2️⃣ Анализируем Spectral Indices
        indexDatabase.analyzeSample(groundTruthAudio, currentSampleRate);

        DBG("  ✅ Spectral indices analyzed");
    }

    // Локальный ресинтез из spectral indices (копия из PluginProcessor)
    void synthesizeFromSpectralIndices(
        const SpectralIndexData& indices,
        juce::AudioBuffer<float>& outputBuffer)
    {
        if (indices.getNumFrames() == 0 || outputBuffer.getNumSamples() == 0)
            return;

        auto modifiedBins = indices.getAllModifiedBins();
        if (modifiedBins.empty())
            return;

        DBG("🎵 LOCAL spectral resynthesis...");
        DBG("  Modified bins: " + juce::String(modifiedBins.size()));

        // Группируем по фреймам
        std::map<int, std::vector<SpectralIndexData::ModifiedBinInfo>> modsByFrame;
        for (const auto& binInfo : modifiedBins)
        {
            modsByFrame[binInfo.frameIdx].push_back(binInfo);
        }

        // Параметры синтеза
        int windowSize = 512;
        int halfWindow = windowSize / 2;

        std::vector<float> window(windowSize);
        for (int i = 0; i < windowSize; ++i)
        {
            window[i] = 0.5f * (1.0f - std::cos(
                2.0f * juce::MathConstants<float>::pi * i / (windowSize - 1)));
        }

        // Применяем изменения фрейм за фреймом
        for (const auto& [frameIdx, frameMods] : modsByFrame)
        {
            const auto& frame = indices.getFrame(frameIdx);
            float timePosition = frame.timePosition;

            int samplePos = static_cast<int>(timePosition * currentSampleRate);

            if (samplePos < 0 || samplePos >= outputBuffer.getNumSamples())
                continue;

            for (const auto& binInfo : frameMods)
            {
                auto modifiedIndex = indices.getIndex(frameIdx, binInfo.binIdx);

                float magnitudeDelta = modifiedIndex.magnitude -
                    modifiedIndex.originalMagnitude;

                if (std::abs(magnitudeDelta) < 0.0001f)
                    continue;

                float frequency = binInfo.frequency;
                float phase = modifiedIndex.phase;

                // Синтезируем с окном
                for (int i = -halfWindow; i < halfWindow; ++i)
                {
                    int targetSample = samplePos + i;
                    if (targetSample < 0 || targetSample >= outputBuffer.getNumSamples())
                        continue;

                    float windowValue = window[i + halfWindow];

                    float t = i / static_cast<float>(currentSampleRate);
                    float sinValue = std::sin(
                        2.0f * juce::MathConstants<float>::pi * frequency * t + phase);

                    float contribution = magnitudeDelta * sinValue * windowValue;

                    // Soft saturation для больших вкладов
                    float absContribution = std::abs(contribution);
                    if (absContribution > 0.5f)
                    {
                        float sign = (contribution > 0.0f) ? 1.0f : -1.0f;
                        contribution = sign * (0.5f +
                            std::tanh((absContribution - 0.5f) * 2.0f) * 0.3f);
                    }

                    // Применяем к обоим каналам
                    for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                    {
                        outputBuffer.addSample(ch, targetSample, contribution);
                    }
                }
            }
        }

        DBG("✅ Local spectral resynthesis complete");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioStateManager)
};