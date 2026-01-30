#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>

class DifferenceEngine
{
public:
    DifferenceEngine() = default;

    // ✅ STEREO: difference = sample - noise (оба канала)
    void calculateDifference(const juce::AudioBuffer<float>& sample,
        const juce::AudioBuffer<float>& noise,
        juce::AudioBuffer<float>& outDifference)
    {
        jassert(sample.getNumSamples() == noise.getNumSamples());

        int numChannels = juce::jmin(sample.getNumChannels(), noise.getNumChannels());
        int numSamples = sample.getNumSamples();

        outDifference.setSize(numChannels, numSamples, false, false, false);

        // ✅ Обрабатываем ОБА канала
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* sampleData = sample.getReadPointer(ch);
            auto* noiseData = noise.getReadPointer(ch);
            auto* diffData = outDifference.getWritePointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                diffData[i] = sampleData[i] - noiseData[i];
            }
        }

        DBG("DifferenceEngine: Processed " + juce::String(numChannels) + " channels");
    }

    // ✅ STEREO reconstruction
    void reconstruct(const juce::AudioBuffer<float>& noise,
        const juce::AudioBuffer<float>& difference,
        juce::AudioBuffer<float>& output,
        float scale, float offset, int bitDepth)
    {
        jassert(noise.getNumSamples() == difference.getNumSamples());

        int numChannels = juce::jmin(noise.getNumChannels(), difference.getNumChannels());
        int numSamples = noise.getNumSamples();

        output.setSize(numChannels, numSamples, false, false, false);

        // ✅ Обрабатываем ОБА канала
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* noiseData = noise.getReadPointer(ch);
            auto* diffData = difference.getReadPointer(ch);
            auto* outData = output.getWritePointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                // Модификация difference
                float modifiedDiff = diffData[i] * scale + offset;

                // Bit crushing только если < 16
                if (bitDepth < 16)
                {
                    float levels = std::pow(2.0f, static_cast<float>(bitDepth - 1));
                    modifiedDiff = std::round(modifiedDiff * levels) / levels;
                }

                // Восстановление: sample = noise + difference
                float sample = noiseData[i] + modifiedDiff;
                outData[i] = sample;
            }
        }

        DBG("DifferenceEngine: Reconstructed " + juce::String(numChannels) + " channels");
    }

    // Статистика для анализа и ML
    struct Statistics
    {
        float min = 0.0f;
        float max = 0.0f;
        float mean = 0.0f;
        float rms = 0.0f;
        int numSamples = 0;
    };

    Statistics calculateStatistics(const juce::AudioBuffer<float>& buffer, int channel = 0) const
    {
        Statistics stats;

        if (buffer.getNumSamples() == 0 || channel >= buffer.getNumChannels())
            return stats;

        auto* data = buffer.getReadPointer(channel);
        stats.numSamples = buffer.getNumSamples();

        stats.min = *std::min_element(data, data + stats.numSamples);
        stats.max = *std::max_element(data, data + stats.numSamples);

        float sum = 0.0f;
        float sumSquares = 0.0f;

        for (int i = 0; i < stats.numSamples; ++i)
        {
            sum += data[i];
            sumSquares += data[i] * data[i];
        }

        stats.mean = sum / static_cast<float>(stats.numSamples);
        stats.rms = std::sqrt(sumSquares / static_cast<float>(stats.numSamples));

        return stats;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DifferenceEngine)
};