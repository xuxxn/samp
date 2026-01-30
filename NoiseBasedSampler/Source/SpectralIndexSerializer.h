/*
SpectralIndexSerializer.h
Утилиты для экспорта спектральных индексов в форматы для AI/ML анализа.
Функции:

exportToJSON: полный экспорт индексов в JSON (version, resolution, params, statistics, ML data с матрицами magnitudes)
exportToCSV: экспорт в CSV формат для ML библиотек (pandas, numpy) - построчно: frame, time, bin, frequency, magnitude, phase, isTransient, isPeak
exportAggregatedFeatures: экспорт frame-level features (time, rmsEnergy, spectralCentroid, spectralSpread, zeroCrossingRate)
importFromJSON: загрузка индексов (TODO, пока не реализовано)
getResolutionString: конвертация enum в строку

Контекст и связи:

Используется для экспорта данных для внешнего AI анализа
JSON формат: человеко-читаемый, полная метаинформация
CSV формат: для Python ML библиотек (scikit-learn, tensorflow, pytorch)
Aggregated features: для classical ML (SVM, Random Forest) без работы с сырым спектром
Будущее:

Import для восстановления индексов из сохраненных данных
Binary format для быстрого I/O больших объемов
Integration с cloud ML services (AWS, GCP)


Типичный workflow:

Analyze sample → create indices
Export to CSV → load in Python
Train ML model → deploy back to plugin
*/

#pragma once
#include <JuceHeader.h>
#include "SpectralIndexData.h"

// ==========================================================================
// СЕРИАЛИЗАЦИЯ СПЕКТРАЛЬНЫХ ИНДЕКСОВ
// Сохранение и загрузка для AI/ML анализа
// ==========================================================================

class SpectralIndexSerializer
{
public:
    // ==========================================================================
    // ЭКСПОРТ ДЛЯ AI/ML
    // ==========================================================================

    // Экспорт индексов в JSON (для ML обучения)
    static bool exportToJSON(
        const SpectralIndexData& indices,
        const juce::File& outputFile)
    {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();

        root->setProperty("version", "1.0");
        root->setProperty("type", "spectral_indices");
        root->setProperty("resolution", getResolutionString(indices.getResolution()));

        // Параметры
        auto params = indices.getParams();
        juce::DynamicObject::Ptr paramsObj = new juce::DynamicObject();
        paramsObj->setProperty("fftSize", params.fftSize);
        paramsObj->setProperty("hopSize", params.hopSize);
        paramsObj->setProperty("sampleRate", params.sampleRate);
        paramsObj->setProperty("binWidth", params.getBinWidth());
        root->setProperty("params", paramsObj.get());

        // Статистика
        auto stats = indices.calculateStatistics();
        juce::DynamicObject::Ptr statsObj = new juce::DynamicObject();
        statsObj->setProperty("totalIndices", stats.totalIndices);
        statsObj->setProperty("maxMagnitude", stats.maxMagnitude);
        statsObj->setProperty("avgMagnitude", stats.avgMagnitude);
        statsObj->setProperty("transientCount", stats.transientCount);
        statsObj->setProperty("peakCount", stats.peakCount);
        root->setProperty("statistics", statsObj.get());

        // Экспортируем для ML
        auto mlData = indices.exportForML();

        juce::DynamicObject::Ptr mlObj = new juce::DynamicObject();
        mlObj->setProperty("numFrames", mlData.numFrames);
        mlObj->setProperty("numBins", mlData.numBins);

        // Сохраняем только основные матрицы (сжато)
        juce::Array<juce::var> magnitudesArray;
        for (const auto& frame : mlData.magnitudeMatrix)
        {
            juce::Array<juce::var> frameArray;
            for (float mag : frame)
            {
                frameArray.add(mag);
            }
            magnitudesArray.add(frameArray);
        }
        mlObj->setProperty("magnitudes", magnitudesArray);

        root->setProperty("mlData", mlObj.get());

        // Сохраняем
        juce::var jsonVar(root.get());
        juce::String jsonString = juce::JSON::toString(jsonVar, true);

        return outputFile.replaceWithText(jsonString);
    }

    // Экспорт в CSV (для ML библиотек типа pandas)
    static bool exportToCSV(
        const SpectralIndexData& indices,
        const juce::File& outputFile)
    {
        juce::String csv;

        // Заголовок
        csv << "frame,time,bin,frequency,magnitude,phase,isTransient,isPeak\n";

        // Данные
        for (int f = 0; f < indices.getNumFrames(); ++f)
        {
            const auto& frame = indices.getFrame(f);

            for (int b = 0; b < indices.getNumBins(); ++b)
            {
                const auto& index = frame.indices[b];
                float freq = indices.getBinFrequency(b);

                csv << juce::String(f) << ","
                    << juce::String(frame.timePosition, 6) << ","
                    << juce::String(b) << ","
                    << juce::String(freq, 2) << ","
                    << juce::String(index.magnitude, 6) << ","
                    << juce::String(index.phase, 6) << ","
                    << (index.isTransient ? "1" : "0") << ","
                    << (index.isPeak ? "1" : "0") << "\n";
            }
        }

        return outputFile.replaceWithText(csv);
    }

    // Экспорт агрегированных индексов (frame-level features для ML)
    static bool exportAggregatedFeatures(
        const SpectralIndexData& indices,
        const juce::File& outputFile)
    {
        juce::String csv;

        // Заголовок
        csv << "frame,time,rmsEnergy,spectralCentroid,spectralSpread,zeroCrossingRate\n";

        // Данные
        for (int f = 0; f < indices.getNumFrames(); ++f)
        {
            const auto& frame = indices.getFrame(f);

            csv << juce::String(f) << ","
                << juce::String(frame.timePosition, 6) << ","
                << juce::String(frame.rmsEnergy, 6) << ","
                << juce::String(frame.spectralCentroid, 2) << ","
                << juce::String(frame.spectralSpread, 2) << ","
                << juce::String(frame.zeroCrossingRate, 4) << "\n";
        }

        return outputFile.replaceWithText(csv);
    }

    // ==========================================================================
    // ЗАГРУЗКА
    // ==========================================================================

    // Загрузка из JSON (TODO: реализовать при необходимости)
    static SpectralIndexData importFromJSON(const juce::File& inputFile)
    {
        // TODO: implement if needed
        return SpectralIndexData();
    }

private:
    static juce::String getResolutionString(IndexResolution res)
    {
        switch (res)
        {
        case IndexResolution::Overview: return "overview";
        case IndexResolution::Medium: return "medium";
        case IndexResolution::Maximum: return "maximum";
        }
        return "unknown";
    }
};