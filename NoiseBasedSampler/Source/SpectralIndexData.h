/*
SpectralIndexData.h - ИСПРАВЛЕННАЯ ВЕРСИЯ
✅ Bin-level modification tracking
✅ Frequency-selective synthesis support
✅ Все необходимые определения
*/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <map>

// ==========================================================================
// СПЕКТРАЛЬНЫЕ ИНДЕКСЫ
// ==========================================================================

// ✅ Один индекс с поддержкой bin-level tracking
struct SpectralIndex
{
    float magnitude = 0.0f;
    float phase = 0.0f;

    float spectralFlux = 0.0f;
    float spectralRolloff = 0.0f;

    bool isTransient = false;
    bool isPeak = false;

    // ✅ НОВОЕ: Bin-level modification tracking
    bool isModified = false;
    float originalMagnitude = 0.0f;
    float originalPhase = 0.0f;

    SpectralIndex() = default;

    SpectralIndex(float mag, float ph)
        : magnitude(mag), phase(ph),
        originalMagnitude(mag), originalPhase(ph) {
    }

    void modify(float newMag, float newPhase)
    {
        magnitude = newMag;
        phase = newPhase;
        isModified = true;
    }

    void resetToOriginal()
    {
        magnitude = originalMagnitude;
        phase = originalPhase;
        isModified = false;
    }
};

// Фрейм индексов
struct SpectralIndexFrame
{
    std::vector<SpectralIndex> indices;
    float timePosition = 0.0f;

    float rmsEnergy = 0.0f;
    float spectralCentroid = 0.0f;
    float spectralSpread = 0.0f;
    float zeroCrossingRate = 0.0f;

    SpectralIndexFrame(int numBins)
    {
        indices.resize(numBins);
    }

    bool hasAnyModifiedBins() const
    {
        for (const auto& idx : indices)
        {
            if (idx.isModified)
                return true;
        }
        return false;
    }

    int getModifiedBinCount() const
    {
        int count = 0;
        for (const auto& idx : indices)
        {
            if (idx.isModified)
                count++;
        }
        return count;
    }

    void calculateAggregatedIndices(float binWidth)
    {
        if (indices.empty()) return;

        float totalEnergy = 0.0f;
        float weightedFreqSum = 0.0f;
        float freqSquaredSum = 0.0f;

        for (size_t i = 0; i < indices.size(); ++i)
        {
            float mag = indices[i].magnitude;
            float freq = i * binWidth;

            totalEnergy += mag * mag;
            weightedFreqSum += freq * mag;
            freqSquaredSum += freq * freq * mag;
        }

        rmsEnergy = std::sqrt(totalEnergy / indices.size());

        if (totalEnergy > 0.001f)
        {
            spectralCentroid = weightedFreqSum / totalEnergy;
            spectralSpread = std::sqrt(freqSquaredSum / totalEnergy -
                spectralCentroid * spectralCentroid);
        }
    }
};

// ==========================================================================
// Уровни разрешения
// ==========================================================================
enum class IndexResolution
{
    Overview,   // FFT 2048, hop 256
    Medium,     // FFT 4096, hop 512
    Maximum     // FFT 8192, hop 256
};

struct IndexAnalysisParams
{
    int fftSize;
    int hopSize;
    double sampleRate;
    IndexResolution resolution;

    float getBinWidth() const
    {
        return static_cast<float>(sampleRate / fftSize);
    }

    int getNumBins() const
    {
        return fftSize / 2;
    }

    float getTimeResolutionMs() const
    {
        return (hopSize / sampleRate) * 1000.0f;
    }

    static IndexAnalysisParams forResolution(IndexResolution res, double sampleRate = 44100.0)
    {
        IndexAnalysisParams params;
        params.sampleRate = sampleRate;
        params.resolution = res;

        switch (res)
        {
        case IndexResolution::Overview:
            params.fftSize = 2048;
            params.hopSize = 256;
            break;

        case IndexResolution::Medium:
            params.fftSize = 4096;
            params.hopSize = 512;
            break;

        case IndexResolution::Maximum:
            params.fftSize = 8192;
            params.hopSize = 256;
            break;
        }

        return params;
    }
};

// ==========================================================================
// Класс SpectralIndexData
// ==========================================================================
class SpectralIndexData
{
public:
    SpectralIndexData() = default;
    SpectralIndexData(SpectralIndexData&&) noexcept = default;
    SpectralIndexData& operator=(SpectralIndexData&&) noexcept = default;
    SpectralIndexData(const SpectralIndexData&) = default;
    SpectralIndexData& operator=(const SpectralIndexData&) = default;

    SpectralIndexData(IndexResolution res, double sampleRate = 44100.0)
        : params(IndexAnalysisParams::forResolution(res, sampleRate))
    {
    }

    void addFrame(const SpectralIndexFrame& frame)
    {
        frames.push_back(frame);
    }

    // Базовые методы доступа
    int getNumFrames() const { return static_cast<int>(frames.size()); }
    int getNumBins() const { return params.getNumBins(); }
    float getBinWidth() const { return params.getBinWidth(); }

    const SpectralIndexFrame& getFrame(int index) const { return frames[index]; }
    SpectralIndexFrame& getFrameMutable(int index) { return frames[index]; }

    const SpectralIndex& getIndex(int frameIdx, int binIdx) const
    {
        static SpectralIndex emptyIndex;
        if (frameIdx >= 0 && frameIdx < getNumFrames() &&
            binIdx >= 0 && binIdx < getNumBins())
        {
            return frames[frameIdx].indices[binIdx];
        }
        return emptyIndex;
    }

    float getBinFrequency(int binIdx) const
    {
        return binIdx * params.getBinWidth();
    }

    // ✅ Методы модификации
    void setIndex(int frameIdx, int binIdx, const SpectralIndex& index)
    {
        if (frameIdx >= 0 && frameIdx < getNumFrames() &&
            binIdx >= 0 && binIdx < getNumBins())
        {
            frames[frameIdx].indices[binIdx] = index;
            frames[frameIdx].indices[binIdx].isModified = true;
        }
    }

    void modifyIndex(int frameIdx, int binIdx, float newMagnitude, float newPhase)
    {
        if (frameIdx >= 0 && frameIdx < getNumFrames() &&
            binIdx >= 0 && binIdx < getNumBins())
        {
            auto& index = frames[frameIdx].indices[binIdx];

            // ✅ КРИТИЧНО: Сохраняем оригинальные значения ОДИН РАЗ
            // При первой модификации
            if (!index.isModified)
            {
                index.originalMagnitude = index.magnitude;
                index.originalPhase = index.phase;
            }

            // Устанавливаем новые значения
            index.magnitude = newMagnitude;
            index.phase = newPhase;
            index.isModified = true;
        }
    }

    // ✅ Получение модифицированных bins
    struct ModifiedBinInfo
    {
        int frameIdx;
        int binIdx;
        float frequency;
    };

    std::vector<ModifiedBinInfo> getAllModifiedBins() const
    {
        std::vector<ModifiedBinInfo> modifiedBins;

        for (int f = 0; f < getNumFrames(); ++f)
        {
            for (int b = 0; b < getNumBins(); ++b)
            {
                if (frames[f].indices[b].isModified)
                {
                    ModifiedBinInfo info;
                    info.frameIdx = f;
                    info.binIdx = b;
                    info.frequency = getBinFrequency(b);
                    modifiedBins.push_back(info);
                }
            }
        }

        return modifiedBins;
    }

    std::vector<int> getModifiedFrameIndices() const
    {
        std::vector<int> modifiedFrames;
        for (int i = 0; i < getNumFrames(); ++i)
        {
            if (frames[i].hasAnyModifiedBins())
            {
                modifiedFrames.push_back(i);
            }
        }
        return modifiedFrames;
    }

    // ✅ Очистка модификаций
    void clearAllModifications()
    {
        for (auto& frame : frames)
        {
            for (auto& index : frame.indices)
            {
                index.resetToOriginal();
            }
        }
    }

    // ✅ Region определение (БЕЗ forward declaration!)
    struct Region
    {
        float startTime;
        float endTime;
        float minFreq;
        float maxFreq;

        bool contains(float time, float freq) const
        {
            return time >= startTime && time <= endTime &&
                freq >= minFreq && freq <= maxFreq;
        }

        float getDuration() const { return endTime - startTime; }
        float getFreqRange() const { return maxFreq - minFreq; }
    };

    void clearModificationsInRegion(const Region& region);

    // Статистика
    struct IndexStatistics
    {
        float maxMagnitude = 0.0f;
        float avgMagnitude = 0.0f;
        float magnitudeVariance = 0.0f;
        float avgSpectralCentroid = 0.0f;
        float avgSpectralSpread = 0.0f;
        int totalIndices = 0;
        int transientCount = 0;
        int peakCount = 0;
        float temporalCoherence = 0.0f;
        float spectralCoherence = 0.0f;
    };

    IndexStatistics calculateStatistics() const
    {
        IndexStatistics stats;
        stats.totalIndices = getNumFrames() * getNumBins();

        if (frames.empty()) return stats;

        float sumMag = 0.0f;
        float sumCentroid = 0.0f;
        float sumSpread = 0.0f;

        for (const auto& frame : frames)
        {
            sumCentroid += frame.spectralCentroid;
            sumSpread += frame.spectralSpread;

            for (const auto& index : frame.indices)
            {
                stats.maxMagnitude = std::max(stats.maxMagnitude, index.magnitude);
                sumMag += index.magnitude;

                if (index.isTransient) stats.transientCount++;
                if (index.isPeak) stats.peakCount++;
            }
        }

        stats.avgMagnitude = sumMag / stats.totalIndices;
        stats.avgSpectralCentroid = sumCentroid / getNumFrames();
        stats.avgSpectralSpread = sumSpread / getNumFrames();

        float varSum = 0.0f;
        for (const auto& frame : frames)
        {
            for (const auto& index : frame.indices)
            {
                float diff = index.magnitude - stats.avgMagnitude;
                varSum += diff * diff;
            }
        }
        stats.magnitudeVariance = varSum / stats.totalIndices;

        return stats;
    }

    // ✅ НОВОЕ: Экспорт для ML
    struct MLExportData
    {
        std::vector<std::vector<float>> magnitudeMatrix;
        std::vector<std::vector<float>> phaseMatrix;

        std::vector<float> frameTimes;
        std::vector<float> frameEnergies;
        std::vector<float> frameCentroids;

        int numFrames;
        int numBins;
        float binWidth;
        float timeResolution;
    };

    MLExportData exportForML() const
    {
        MLExportData data;

        data.numFrames = getNumFrames();
        data.numBins = getNumBins();
        data.binWidth = getBinWidth();
        data.timeResolution = params.getTimeResolutionMs();

        data.magnitudeMatrix.resize(data.numFrames);
        data.phaseMatrix.resize(data.numFrames);

        for (int f = 0; f < data.numFrames; ++f)
        {
            const auto& frame = frames[f];

            data.frameTimes.push_back(frame.timePosition);
            data.frameEnergies.push_back(frame.rmsEnergy);
            data.frameCentroids.push_back(frame.spectralCentroid);

            data.magnitudeMatrix[f].resize(data.numBins);
            data.phaseMatrix[f].resize(data.numBins);

            for (int b = 0; b < data.numBins; ++b)
            {
                data.magnitudeMatrix[f][b] = frame.indices[b].magnitude;
                data.phaseMatrix[f][b] = frame.indices[b].phase;
            }
        }

        return data;
    }

    // ✅ НОВОЕ: Извлечение индексов региона
    std::vector<SpectralIndexFrame> extractRegionIndices(const Region& region) const
    {
        std::vector<SpectralIndexFrame> regionFrames;

        for (const auto& frame : frames)
        {
            if (frame.timePosition >= region.startTime &&
                frame.timePosition <= region.endTime)
            {
                SpectralIndexFrame regionFrame(getNumBins());
                regionFrame.timePosition = frame.timePosition;
                regionFrame.rmsEnergy = frame.rmsEnergy;
                regionFrame.spectralCentroid = frame.spectralCentroid;
                regionFrame.spectralSpread = frame.spectralSpread;

                for (int bin = 0; bin < getNumBins(); ++bin)
                {
                    float freq = getBinFrequency(bin);
                    if (freq >= region.minFreq && freq <= region.maxFreq)
                    {
                        regionFrame.indices[bin] = frame.indices[bin];
                    }
                }

                regionFrames.push_back(regionFrame);
            }
        }

        return regionFrames;
    }

    const IndexAnalysisParams& getParams() const { return params; }
    IndexResolution getResolution() const { return params.resolution; }
    const std::vector<SpectralIndexFrame>& getAllFrames() const { return frames; }

private:
    IndexAnalysisParams params;
    std::vector<SpectralIndexFrame> frames;
};

// Implementation of clearModificationsInRegion (needs Region to be defined first)
inline void SpectralIndexData::clearModificationsInRegion(const Region& region)
{
    for (int f = 0; f < getNumFrames(); ++f)
    {
        float time = frames[f].timePosition;
        if (time < region.startTime || time > region.endTime)
            continue;

        for (int b = 0; b < getNumBins(); ++b)
        {
            float freq = getBinFrequency(b);
            if (freq >= region.minFreq && freq <= region.maxFreq)
            {
                frames[f].indices[b].resetToOriginal();
            }
        }
    }
}