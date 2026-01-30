/*
FeatureExtractor.h - LAZY LOADING VERSION
✅ Быстрая загрузка: extractAmplitudeOnly()
✅ По требованию: computeFrequencies(), computePhases(), etc.
✅ Кэширование оригинального аудио для вычислений
*/

#pragma once
#include <JuceHeader.h>
#include "PhaseVocoder.h"
#include "FeatureData.h"
#include <cmath>

class FeatureExtractor
{
public:
    FeatureExtractor() = default;

    PhaseVocoder& getPhaseVocoder() { return phaseVocoder; }

    // ✅ НОВОЕ: Быстрое извлечение только Amplitude (мгновенно!)
    FeatureData extractAmplitudeOnly(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        FeatureData features;
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        if (numSamples == 0)
            return features;

        features.setSize(numSamples);

        const auto* leftData = buffer.getReadPointer(0);
        const auto* rightData = (numChannels >= 2) ? buffer.getReadPointer(1) : leftData;

        DBG("FeatureExtractor: Fast loading - Amplitude only (" + juce::String(numSamples) + " samples)");

        // ✅ ТОЛЬКО Amplitude (самое быстрое)
        for (int i = 0; i < numSamples; ++i)
        {
            features[i].amplitude = leftData[i];

            // Defaults для остальных (будут вычислены позже)
            features[i].frequency = 440.0f;
            features[i].phase = 0.0f;
            features[i].volume = 1.0f;
            features[i].pan = 0.5f;

            // Флаги готовности
            features[i].frequencyComputed = false;
            features[i].phaseComputed = false;
            features[i].volumeComputed = false;
            features[i].panComputed = false;
        }

        // ✅ Кэшируем аудио для последующих вычислений
        cachedAudioBuffer.makeCopyOf(buffer);
        cachedSampleRate = sampleRate;

        DBG("✅ Amplitude extraction complete (instant!)");

        return features;
    }

    // ✅ НОВОЕ: Вычислить Frequency индексы по требованию
    void computeFrequencies(FeatureData& features)
    {
        if (cachedAudioBuffer.getNumSamples() == 0)
        {
            DBG("⚠️ Cannot compute frequencies: no cached audio");
            return;
        }

        DBG("🔄 Computing Frequency indices...");

        int numSamples = features.getNumSamples();
        const auto* leftData = cachedAudioBuffer.getReadPointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            if (!features[i].frequencyComputed)
            {
                features[i].frequency = calculateLocalFrequency(leftData, i, numSamples, cachedSampleRate);
                features[i].frequencyComputed = true;
            }
        }

        auto stats = features.calculateStatistics();
        DBG("✅ Frequency computed: " + juce::String(stats.minFrequency, 1) + " to " +
            juce::String(stats.maxFrequency, 1) + " Hz");
    }

    // ✅ НОВОЕ: Вычислить Phase индексы по требованию
    void computePhases(FeatureData& features)
    {
        if (cachedAudioBuffer.getNumSamples() == 0)
        {
            DBG("⚠️ Cannot compute phases: no cached audio");
            return;
        }

        DBG("🔄 Computing Phase indices...");

        int numSamples = features.getNumSamples();
        const auto* leftData = cachedAudioBuffer.getReadPointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            if (!features[i].phaseComputed)
            {
                features[i].phase = calculateLocalPhase(leftData, i, numSamples);
                features[i].phaseComputed = true;
            }
        }

        auto stats = features.calculateStatistics();
        DBG("✅ Phase computed: " + juce::String(stats.minPhase, 3) + " to " +
            juce::String(stats.maxPhase, 3));
    }

    // ✅ НОВОЕ: Вычислить Volume индексы по требованию
    void computeVolumes(FeatureData& features)
    {
        if (cachedAudioBuffer.getNumSamples() == 0)
        {
            DBG("⚠️ Cannot compute volumes: no cached audio");
            return;
        }

        DBG("🔄 Computing Volume indices...");

        int numSamples = features.getNumSamples();
        const auto* leftData = cachedAudioBuffer.getReadPointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            if (!features[i].volumeComputed)
            {
                features[i].volume = calculateLocalVolume(leftData, i, numSamples, cachedSampleRate);
                features[i].volumeComputed = true;
            }
        }

        auto stats = features.calculateStatistics();
        DBG("✅ Volume computed: " + juce::String(stats.minVolume, 3) + " to " +
            juce::String(stats.maxVolume, 3));
    }

    // ✅ НОВОЕ: Вычислить Pan индексы по требованию
    void computePans(FeatureData& features)
    {
        if (cachedAudioBuffer.getNumSamples() == 0)
        {
            DBG("⚠️ Cannot compute pans: no cached audio");
            return;
        }

        int numChannels = cachedAudioBuffer.getNumChannels();

        if (numChannels < 2)
        {
            // Mono - все Pan = center
            DBG("🔄 Computing Pan indices (mono = center)...");

            for (int i = 0; i < features.getNumSamples(); ++i)
            {
                features[i].pan = 0.5f;
                features[i].panComputed = true;
            }

            DBG("✅ Pan computed (mono)");
            return;
        }

        DBG("🔄 Computing Pan indices (stereo)...");

        int numSamples = features.getNumSamples();
        const auto* leftData = cachedAudioBuffer.getReadPointer(0);
        const auto* rightData = cachedAudioBuffer.getReadPointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            if (!features[i].panComputed)
            {
                features[i].pan = calculateStereoPan(leftData[i], rightData[i]);
                features[i].panComputed = true;
            }
        }

        auto stats = features.calculateStatistics();
        DBG("✅ Pan computed: " + juce::String(stats.minPan, 3) + " to " +
            juce::String(stats.maxPan, 3));
    }

    // ✅ LEGACY: Старый метод для обратной совместимости
    FeatureData extractFeatures(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        // Сначала быстро извлекаем Amplitude
        auto features = extractAmplitudeOnly(buffer, sampleRate);

        // Затем вычисляем все остальные индексы
        computeFrequencies(features);
        computePhases(features);
        computeVolumes(features);
        computePans(features);

        DBG("✅ Full feature extraction complete (legacy mode)");

        return features;
    }

    // ✅ Очистить кэш (при unload сэмпла)
    void clearCache()
    {
        cachedAudioBuffer.setSize(0, 0);
        cachedSampleRate = 0.0;
    }

private:
    PhaseVocoder phaseVocoder;

    // ✅ НОВОЕ: Кэш для lazy вычислений
    juce::AudioBuffer<float> cachedAudioBuffer;
    double cachedSampleRate = 44100.0;

    // ✅ Вычисление STEREO PAN
    float calculateStereoPan(float leftSample, float rightSample) const
    {
        float leftAbs = std::abs(leftSample);
        float rightAbs = std::abs(rightSample);

        float totalEnergy = leftAbs + rightAbs;

        if (totalEnergy < 0.0001f)
            return 0.5f;

        float panBalance = rightAbs / totalEnergy;
        float pan = panBalance * panBalance;

        return juce::jlimit(0.0f, 1.0f, pan);
    }

    // ✅ Вычисление VOLUME
    float calculateLocalVolume(const float* data, int index, int length, double sampleRate)
    {
        int windowSize = 512;
        int halfWindow = windowSize / 2;

        int start = std::max(0, index - halfWindow);
        int end = std::min(length - 1, index + halfWindow);

        int actualWindowSize = end - start;
        if (actualWindowSize < 64)
            return 1.0f;

        float sumSquares = 0.0f;
        for (int i = start; i <= end; ++i)
        {
            sumSquares += data[i] * data[i];
        }

        float rms = std::sqrt(sumSquares / actualWindowSize);

        const float EPSILON = 0.00001f;
        float db = 20.0f * std::log10(rms + EPSILON);

        const float MIN_DB = -60.0f;
        const float MAX_DB = 6.0f;
        float range = MAX_DB - MIN_DB;

        float normalized = (db - MIN_DB) / range;
        normalized = juce::jlimit(0.0f, 1.0f, normalized) * 2.0f;

        return normalized;
    }

    // ✅ Вычисление FREQUENCY
    float calculateLocalFrequency(const float* data, int index, int length, double sampleRate)
    {
        int windowSize = 512;
        int halfWindow = windowSize / 2;

        int start = std::max(0, index - halfWindow);
        int end = std::min(length - 1, index + halfWindow);

        int actualWindowSize = end - start;
        if (actualWindowSize < 64)
            return 440.0f;

        int zeroCrossings = 0;
        for (int i = start + 1; i <= end; ++i)
        {
            if ((data[i] >= 0 && data[i - 1] < 0) || (data[i] < 0 && data[i - 1] >= 0))
                zeroCrossings++;
        }

        float timeWindow = actualWindowSize / static_cast<float>(sampleRate);
        float frequency = (zeroCrossings / 2.0f) / timeWindow;

        frequency = juce::jlimit(20.0f, 20000.0f, frequency);

        return frequency;
    }

    // ✅ Вычисление PHASE
    float calculateLocalPhase(const float* data, int index, int length)
    {
        if (index <= 0 || index >= length - 1)
            return 0.0f;

        int windowSize = 32;
        int halfWindow = windowSize / 2;

        int start = std::max(0, index - halfWindow);
        int end = std::min(length - 1, index + halfWindow);

        float localMax = 0.0001f;
        for (int i = start; i <= end; ++i)
        {
            localMax = std::max(localMax, std::abs(data[i]));
        }

        float normalized = data[index] / localMax;
        normalized = juce::jlimit(-1.0f, 1.0f, normalized);

        float phase = std::asin(normalized);

        if (index > 0)
        {
            float derivative = data[index] - data[index - 1];

            if (data[index] >= 0 && derivative < 0)
            {
                phase = juce::MathConstants<float>::pi - phase;
            }
            else if (data[index] < 0 && derivative < 0)
            {
                phase = juce::MathConstants<float>::pi - phase;
            }
            else if (data[index] < 0 && derivative >= 0)
            {
                phase = juce::MathConstants<float>::twoPi + phase;
            }
        }

        while (phase < 0)
            phase += juce::MathConstants<float>::twoPi;
        while (phase >= juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;

        return phase;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeatureExtractor)
};