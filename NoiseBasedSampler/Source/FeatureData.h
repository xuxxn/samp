/*
FeatureData.h - LOCALIZED STEREO CHANGES
✅ Изменения применяются ТОЛЬКО в изменённых регионах
✅ Стерео сохраняется через pan-based processing
✅ Windowing для плавных переходов
*/

#pragma once
#include <JuceHeader.h>
#include <vector>

struct FeatureSample
{
    float amplitude = 0.0f;
    float frequency = 440.0f;
    float phase = 0.0f;
    float volume = 1.0f;
    float pan = 0.5f;

    // ✅ Флаг "был изменён пользователем"
    bool wasModified = false;

    // ✅ НОВОЕ: Флаги готовности (для Lazy Loading)
    bool frequencyComputed = false;
    bool phaseComputed = false;
    bool volumeComputed = false;
    bool panComputed = false;
    // Amplitude всегда готов (вычисляется сразу)

    bool operator==(const FeatureSample& other) const
    {
        return std::abs(amplitude - other.amplitude) < 0.0001f &&
            std::abs(frequency - other.frequency) < 0.01f &&
            std::abs(phase - other.phase) < 0.0001f &&
            std::abs(volume - other.volume) < 0.0001f &&
            std::abs(pan - other.pan) < 0.0001f;
    }
};

class FeatureData
{
public:
    FeatureData() = default;

    FeatureData(const FeatureData& other)
        : samples(other.samples)
    {
    }

    FeatureData& operator=(const FeatureData& other)
    {
        if (this != &other)
        {
            samples = other.samples;
        }
        return *this;
    }

    FeatureData(FeatureData&& other) noexcept
        : samples(std::move(other.samples))
    {
    }

    FeatureData& operator=(FeatureData&& other) noexcept
    {
        if (this != &other)
        {
            samples = std::move(other.samples);
        }
        return *this;
    }

    ~FeatureData() = default;

    void setSize(int numSamples)
    {
        samples.resize(numSamples);
    }

    int getNumSamples() const
    {
        return static_cast<int>(samples.size());
    }

    const FeatureSample& operator[](int index) const
    {
        jassert(index >= 0 && index < static_cast<int>(samples.size()));
        return samples[index];
    }

    FeatureSample& operator[](int index)
    {
        jassert(index >= 0 && index < static_cast<int>(samples.size()));
        return samples[index];
    }

    // ✅ ВАЖНО: Setters теперь помечают сэмплы как изменённые
    void setAmplitudeAt(int index, float value)
    {
        if (index >= 0 && index < getNumSamples())
        {
            samples[index].amplitude = value;
            samples[index].wasModified = true;  // ← Ключевое изменение!
        }
    }

    void setFrequencyAt(int index, float value)
    {
        if (index >= 0 && index < getNumSamples())
        {
            samples[index].frequency = value;
            samples[index].wasModified = true;
        }
    }

    bool areFrequenciesComputed() const
    {
        if (samples.empty()) return true;
        return samples[0].frequencyComputed;
    }

    bool arePhasesComputed() const
    {
        if (samples.empty()) return true;
        return samples[0].phaseComputed;
    }

    bool areVolumesComputed() const
    {
        if (samples.empty()) return true;
        return samples[0].volumeComputed;
    }

    bool arePansComputed() const
    {
        if (samples.empty()) return true;
        return samples[0].panComputed;
    }

    // Пометить все индексы как вычисленные (при user edit)
    void markAllComputed()
    {
        for (auto& sample : samples)
        {
            sample.frequencyComputed = true;
            sample.phaseComputed = true;
            sample.volumeComputed = true;
            sample.panComputed = true;
        }
    }

    void setPhaseAt(int index, float value)
    {
        if (index >= 0 && index < getNumSamples())
        {
            samples[index].phase = value;
            samples[index].wasModified = true;
        }
    }

    void setVolumeAt(int index, float value)
    {
        if (index >= 0 && index < getNumSamples())
        {
            samples[index].volume = juce::jlimit(0.0f, 1.0f, value);
            samples[index].wasModified = true;
        }
    }

    void setPanAt(int index, float value)
    {
        if (index >= 0 && index < getNumSamples())
        {
            samples[index].pan = juce::jlimit(0.0f, 1.0f, value);
            samples[index].wasModified = true;
        }
    }

    // ✅ НОВОЕ: Сброс флагов после применения
    void clearModificationFlags()
    {
        for (auto& sample : samples)
            sample.wasModified = false;
    }

    // ==========================================================================
    // ✅ КЛЮЧЕВОЕ: LOCALIZED STEREO APPLICATION
    // ==========================================================================

    void applyToAudioBuffer(
        juce::AudioBuffer<float>& buffer,
        double sampleRate,
        const juce::AudioBuffer<float>* originalStereo = nullptr) const
    {
        if (samples.empty())
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        // Ensure STEREO
        if (numChannels < 2)
        {
            DBG("⚠️ Converting to stereo");
            juce::AudioBuffer<float> stereoBuffer(2, numSamples);
            stereoBuffer.clear();

            if (numChannels == 1)
            {
                stereoBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
                stereoBuffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
            }

            buffer = stereoBuffer;
        }

        // ✅ STRATEGY: Detect modified regions and apply LOCALLY
        if (originalStereo &&
            originalStereo->getNumChannels() >= 2 &&
            originalStereo->getNumSamples() == numSamples)
        {
            DBG("✅ Applying LOCALIZED changes to stereo");

            // Start with original
            buffer.makeCopyOf(*originalStereo);

            // Apply ONLY in modified regions
            applyLocalizedModifications(buffer, sampleRate);
        }
        else
        {
            DBG("🎵 Fresh synthesis (no original)");
            synthesizeFreshAudio(buffer, sampleRate);
        }
    }

    struct Statistics
    {
        float minAmplitude = 0.0f;
        float maxAmplitude = 0.0f;
        float avgAmplitude = 0.0f;
        float minFrequency = 0.0f;
        float maxFrequency = 0.0f;
        float avgFrequency = 0.0f;
        float minVolume = 0.0f;
        float maxVolume = 1.0f;
        float avgVolume = 1.0f;
        float minPan = 0.5f;
        float maxPan = 0.5f;
        float avgPan = 0.5f;
    };

    Statistics calculateStatistics() const
    {
        Statistics stats;

        if (samples.empty())
            return stats;

        stats.minAmplitude = samples[0].amplitude;
        stats.maxAmplitude = samples[0].amplitude;
        stats.minFrequency = samples[0].frequency;
        stats.maxFrequency = samples[0].frequency;
        stats.minVolume = samples[0].volume;
        stats.maxVolume = samples[0].volume;
        stats.minPan = samples[0].pan;
        stats.maxPan = samples[0].pan;

        float sumAmp = 0.0f;
        float sumFreq = 0.0f;
        float sumVol = 0.0f;
        float sumPan = 0.0f;

        for (const auto& sample : samples)
        {
            stats.minAmplitude = juce::jmin(stats.minAmplitude, sample.amplitude);
            stats.maxAmplitude = juce::jmax(stats.maxAmplitude, sample.amplitude);
            stats.minFrequency = juce::jmin(stats.minFrequency, sample.frequency);
            stats.maxFrequency = juce::jmax(stats.maxFrequency, sample.frequency);
            stats.minVolume = juce::jmin(stats.minVolume, sample.volume);
            stats.maxVolume = juce::jmax(stats.maxVolume, sample.volume);
            stats.minPan = juce::jmin(stats.minPan, sample.pan);
            stats.maxPan = juce::jmax(stats.maxPan, sample.pan);

            sumAmp += sample.amplitude;
            sumFreq += sample.frequency;
            sumVol += sample.volume;
            sumPan += sample.pan;
        }

        stats.avgAmplitude = sumAmp / samples.size();
        stats.avgFrequency = sumFreq / samples.size();
        stats.avgVolume = sumVol / samples.size();
        stats.avgPan = sumPan / samples.size();

        return stats;
    }

private:
    std::vector<FeatureSample> samples;

    // ==========================================================================
    // ✅ LOCALIZED MODIFICATION ENGINE
    // ==========================================================================

    struct ModifiedRegion
    {
        int start;
        int end;
    };

    // Detect continuous modified regions
    std::vector<ModifiedRegion> detectModifiedRegions() const
    {
        std::vector<ModifiedRegion> regions;

        int regionStart = -1;

        for (int i = 0; i < static_cast<int>(samples.size()); ++i)
        {
            if (samples[i].wasModified)
            {
                if (regionStart < 0)
                    regionStart = i;  // Start new region
            }
            else
            {
                if (regionStart >= 0)
                {
                    // Close current region
                    regions.push_back({ regionStart, i - 1 });
                    regionStart = -1;
                }
            }
        }

        // Close last region if still open
        if (regionStart >= 0)
        {
            regions.push_back({ regionStart, static_cast<int>(samples.size()) - 1 });
        }

        return regions;
    }

    void applyLocalizedModifications(
        juce::AudioBuffer<float>& buffer,
        double sampleRate) const
    {
        auto regions = detectModifiedRegions();

        if (regions.empty())
        {
            DBG("  No modifications detected");
            return;
        }

        DBG("  Found " + juce::String(regions.size()) + " modified regions");

        // Process each region independently
        for (const auto& region : regions)
        {
            applyModificationToRegion(buffer, region, sampleRate);
        }
    }

    void applyModificationToRegion(
        juce::AudioBuffer<float>& buffer,
        const ModifiedRegion& region,
        double sampleRate) const
    {
        // ✅ Window parameters
        const int FADE_SAMPLES = 64;  // Crossfade для плавности

        int start = region.start;
        int end = region.end;
        int regionLength = end - start + 1;

        DBG("    Processing region: " + juce::String(start) + "-" + juce::String(end) +
            " (" + juce::String(regionLength) + " samples)");

        // ✅ Create windowed modification buffer
        juce::AudioBuffer<float> modification(2, regionLength + FADE_SAMPLES * 2);
        modification.clear();

        // Synthesize ONLY this region
        for (int i = 0; i < regionLength; ++i)
        {
            int sampleIdx = start + i;
            const auto& feature = samples[sampleIdx];

            // Simple amplitude replacement (можно улучшить до синтеза)
            float newValue = feature.amplitude * feature.volume;

            // Apply pan to stereo
            float leftGain = std::sqrt(1.0f - feature.pan);
            float rightGain = std::sqrt(feature.pan);

            modification.setSample(0, i + FADE_SAMPLES, newValue * leftGain);
            modification.setSample(1, i + FADE_SAMPLES, newValue * rightGain);
        }

        // ✅ Apply Hann window для плавных краёв
        applyHannWindow(modification, FADE_SAMPLES);

        // ✅ Crossfade with original
        for (int i = 0; i < regionLength + FADE_SAMPLES * 2; ++i)
        {
            int bufferPos = start + i - FADE_SAMPLES;

            if (bufferPos < 0 || bufferPos >= buffer.getNumSamples())
                continue;

            // Compute crossfade weight
            float weight = 1.0f;

            if (i < FADE_SAMPLES)
            {
                // Fade in
                weight = i / static_cast<float>(FADE_SAMPLES);
            }
            else if (i >= regionLength + FADE_SAMPLES)
            {
                // Fade out
                int fadeOutPos = i - (regionLength + FADE_SAMPLES);
                weight = 1.0f - (fadeOutPos / static_cast<float>(FADE_SAMPLES));
            }

            // Mix with original using crossfade
            for (int ch = 0; ch < 2; ++ch)
            {
                float original = buffer.getSample(ch, bufferPos);
                float modified = modification.getSample(ch, i);

                // Crossfade: original * (1 - weight) + modified * weight
                buffer.setSample(ch, bufferPos, original * (1.0f - weight) + modified * weight);
            }
        }
    }

    void applyHannWindow(juce::AudioBuffer<float>& buffer, int fadeLength) const
    {
        int totalLength = buffer.getNumSamples();

        for (int i = 0; i < fadeLength; ++i)
        {
            // Fade in (left edge)
            float fadeIn = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * i / fadeLength));

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                buffer.setSample(ch, i, buffer.getSample(ch, i) * fadeIn);
            }

            // Fade out (right edge)
            int fadeOutIdx = totalLength - 1 - i;
            float fadeOut = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * i / fadeLength));

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                buffer.setSample(ch, fadeOutIdx, buffer.getSample(ch, fadeOutIdx) * fadeOut);
            }
        }
    }

    void synthesizeFreshAudio(
        juce::AudioBuffer<float>& buffer,
        double sampleRate) const
    {
        buffer.clear();

        const int numSamples = juce::jmin(
            buffer.getNumSamples(),
            static_cast<int>(samples.size())
        );

        for (int i = 0; i < numSamples; ++i)
        {
            const auto& feature = samples[i];

            // Simple synthesis (можно улучшить)
            float synthValue = feature.amplitude * feature.volume;

            // Stereo panning
            float leftGain = std::sqrt(1.0f - feature.pan);
            float rightGain = std::sqrt(feature.pan);

            buffer.setSample(0, i, synthValue * leftGain);
            buffer.setSample(1, i, synthValue * rightGain);
        }
    }

    JUCE_LEAK_DETECTOR(FeatureData)
};