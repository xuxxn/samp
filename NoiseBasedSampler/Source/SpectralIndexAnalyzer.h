/*
SpectralIndexAnalyzer.h - ИСПРАВЛЕННАЯ ВЕРСИЯ
✅ Правильная нормализация FFT магнитуд (делим на fftSize)
✅ Компенсация для Hann window (умножаем на 2.0)
✅ Теперь спектр показывает РЕАЛЬНЫЕ значения
*/

#pragma once
#include <JuceHeader.h>
#include "SpectralIndexData.h"

// ==========================================================================
// АНАЛИЗАТОР СПЕКТРАЛЬНЫХ ИНДЕКСОВ
// Создание SpectralIndexData из AudioBuffer с разными уровнями разрешения
// ==========================================================================

class SpectralIndexAnalyzer
{
public:
    SpectralIndexAnalyzer() = default;

    // Анализ всего семпла с указанным разрешением
    SpectralIndexData analyzeWithResolution(
        const juce::AudioBuffer<float>& buffer,
        double sampleRate,
        IndexResolution resolution)
    {
        SpectralIndexData data(resolution, sampleRate);
        auto params = data.getParams();

        DBG("===========================================");
        DBG("ANALYZING WITH RESOLUTION: " + getResolutionName(resolution));
        DBG("FFT Size: " + juce::String(params.fftSize));
        DBG("Hop Size: " + juce::String(params.hopSize));
        DBG("Bin Width: " + juce::String(params.getBinWidth(), 2) + " Hz");
        DBG("===========================================");

        // Работаем с моно (левый канал или усреднённый)
        const float* audioData = buffer.getReadPointer(0);
        int numSamples = buffer.getNumSamples();

        // FFT
        juce::dsp::FFT fft(static_cast<int>(std::log2(params.fftSize)));
        juce::dsp::WindowingFunction<float> window(
            params.fftSize,
            juce::dsp::WindowingFunction<float>::hann,
            false  // ✅ ВАЖНО: normalise = false
        );

        std::vector<float> fftData(params.fftSize * 2, 0.0f);
        std::vector<float> windowedData(params.fftSize);

        int numFrames = (numSamples - params.fftSize) / params.hopSize + 1;
        DBG("Will analyze " + juce::String(numFrames) + " frames");

        // ✅ КРИТИЧНО: Коэффициенты компенсации для JUCE FFT
        // 1. JUCE FFT выдаёт магнитуды * fftSize (слишком большие)
        // 2. Hann window имеет среднее значение 0.5 (теряем 6 dB)
        // Итого: нужно разделить на fftSize и умножить на 2.0
        const float fftNormalization = 2.0f / static_cast<float>(params.fftSize);

        DBG("FFT normalization factor: " + juce::String(fftNormalization, 6));

        for (int frame = 0; frame < numFrames; ++frame)
        {
            int startSample = frame * params.hopSize;
            float timePosition = startSample / static_cast<float>(sampleRate);

            SpectralIndexFrame indexFrame(params.getNumBins());
            indexFrame.timePosition = timePosition;

            // Копируем данные в буфер
            for (int i = 0; i < params.fftSize; ++i)
            {
                int sampleIdx = startSample + i;
                windowedData[i] = (sampleIdx < numSamples) ? audioData[sampleIdx] : 0.0f;
            }

            // Применяем window
            window.multiplyWithWindowingTable(windowedData.data(), params.fftSize);

            // Копируем в FFT буфер
            for (int i = 0; i < params.fftSize; ++i)
            {
                fftData[i] = windowedData[i];
            }

            // Выполняем FFT
            fft.performFrequencyOnlyForwardTransform(fftData.data());

            // ✅ КРИТИЧНО: Извлекаем магнитуды с ПРАВИЛЬНОЙ нормализацией
            float totalEnergy = 0.0f;
            float maxMag = 0.0f;

            for (int bin = 0; bin < params.getNumBins(); ++bin)
            {
                // ✅ ИСПРАВЛЕНИЕ: Применяем нормализацию JUCE FFT
                float rawMagnitude = fftData[bin];
                float correctedMagnitude = rawMagnitude * fftNormalization;

                // Вычисляем фазу (если нужна в будущем)
                // Для performFrequencyOnlyForwardTransform фазы нет,
                // но можем использовать 0 как placeholder
                float phase = 0.0f;

                indexFrame.indices[bin].magnitude = correctedMagnitude;
                indexFrame.indices[bin].phase = phase;
                indexFrame.indices[bin].originalMagnitude = correctedMagnitude;
                indexFrame.indices[bin].originalPhase = phase;

                totalEnergy += correctedMagnitude * correctedMagnitude;
                maxMag = juce::jmax(maxMag, correctedMagnitude);
            }

            // Агрегированные индексы фрейма
            indexFrame.calculateAggregatedIndices(params.getBinWidth());

            // Детектируем transients и peaks
            if (frame > 0)
            {
                detectTransientsAndPeaks(indexFrame, data.getFrame(frame - 1));
            }

            data.addFrame(indexFrame);

            // Дебаг для первого фрейма
            if (frame == 0)
            {
                DBG("First frame analysis:");
                DBG("  Max magnitude: " + juce::String(maxMag, 6));
                DBG("  Total energy: " + juce::String(totalEnergy, 2));
                DBG("  Spectral centroid: " + juce::String(indexFrame.spectralCentroid, 1) + " Hz");

                // Проверяем несколько бинов
                for (int b = 0; b < juce::jmin(5, params.getNumBins()); ++b)
                {
                    float freq = b * params.getBinWidth();
                    float mag = indexFrame.indices[b].magnitude;
                    float magDB = 20.0f * std::log10(mag + 1e-10f);
                    DBG("  Bin " + juce::String(b) + " (" + juce::String(freq, 0) +
                        " Hz): mag=" + juce::String(mag, 6) +
                        ", dB=" + juce::String(magDB, 1));
                }
            }
        }

        auto stats = data.calculateStatistics();
        DBG("Analysis complete:");
        DBG("  Frames: " + juce::String(data.getNumFrames()));
        DBG("  Max magnitude: " + juce::String(stats.maxMagnitude, 6));
        DBG("  Avg magnitude: " + juce::String(stats.avgMagnitude, 6));
        DBG("  Transients: " + juce::String(stats.transientCount));
        DBG("  Peaks: " + juce::String(stats.peakCount));
        DBG("===========================================");

        return data;
    }

    // Анализ конкретного региона с указанным разрешением
    SpectralIndexData analyzeRegion(
        const juce::AudioBuffer<float>& buffer,
        double sampleRate,
        const SpectralIndexData::Region& region,
        IndexResolution resolution)
    {
        // Извлекаем регион
        int startSample = static_cast<int>(region.startTime * sampleRate);
        int endSample = static_cast<int>(region.endTime * sampleRate);

        startSample = juce::jlimit(0, buffer.getNumSamples() - 1, startSample);
        endSample = juce::jlimit(startSample, buffer.getNumSamples(), endSample);

        int regionLength = endSample - startSample;

        DBG("Analyzing region: " +
            juce::String(region.startTime, 3) + "s - " +
            juce::String(region.endTime, 3) + "s (" +
            juce::String(regionLength) + " samples)");

        // Создаём временный буфер с регионом
        juce::AudioBuffer<float> regionBuffer(1, regionLength);
        regionBuffer.copyFrom(0, 0, buffer, 0, startSample, regionLength);

        // Анализируем регион
        SpectralIndexData regionData = analyzeWithResolution(
            regionBuffer,
            sampleRate,
            resolution
        );

        // Корректируем timePosition для всех фреймов
        for (int f = 0; f < regionData.getNumFrames(); ++f)
        {
            auto& frame = regionData.getFrameMutable(f);
            frame.timePosition += region.startTime;
        }

        return regionData;
    }

private:
    void detectTransientsAndPeaks(
        SpectralIndexFrame& currentFrame,
        const SpectralIndexFrame& previousFrame)
    {
        // Spectral flux для transient detection
        float flux = 0.0f;
        for (size_t i = 0; i < currentFrame.indices.size(); ++i)
        {
            float diff = currentFrame.indices[i].magnitude -
                previousFrame.indices[i].magnitude;
            if (diff > 0.0f)
                flux += diff;
        }
        currentFrame.indices[0].spectralFlux = flux;

        // Transient threshold
        const float transientThreshold = 0.5f;
        if (flux > transientThreshold)
        {
            for (auto& idx : currentFrame.indices)
                idx.isTransient = true;
        }

        // Peak detection (локальные максимумы)
        for (size_t i = 1; i < currentFrame.indices.size() - 1; ++i)
        {
            float curr = currentFrame.indices[i].magnitude;
            float prev = currentFrame.indices[i - 1].magnitude;
            float next = currentFrame.indices[i + 1].magnitude;

            if (curr > prev && curr > next && curr > 0.01f)
            {
                currentFrame.indices[i].isPeak = true;
            }
        }
    }

    juce::String getResolutionName(IndexResolution res) const
    {
        switch (res)
        {
        case IndexResolution::Overview: return "OVERVIEW";
        case IndexResolution::Medium: return "MEDIUM";
        case IndexResolution::Maximum: return "MAXIMUM";
        }
        return "UNKNOWN";
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralIndexAnalyzer)
};