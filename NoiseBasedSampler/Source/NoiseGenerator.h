#pragma once
#include <JuceHeader.h>

class NoiseGenerator
{
public:
    NoiseGenerator() : seed(12345), state(12345) {}

    void setSeed(uint64_t newSeed)
    {
        seed = newSeed;
        state = seed;
    }

    uint64_t getSeed() const { return seed; }

    // Xorshift64 PRNG
    float nextFloat()
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;

        return (static_cast<float>(state) / static_cast<float>(UINT64_MAX)) * 2.0f - 1.0f;
    }

    // ✅ STEREO: Генерирует НЕЗАВИСИМЫЙ шум для каждого канала!
    void generateNoise(juce::AudioBuffer<float>& outBuffer)
    {
        int numChannels = outBuffer.getNumChannels();
        int numSamples = outBuffer.getNumSamples();

        // ✅ КРИТИЧНО: Каждый канал с разным seed offset
        // Это даёт настоящий STEREO шум, а не копию!

        for (int ch = 0; ch < numChannels; ++ch)
        {
            // Сброс состояния с offset для каждого канала
            state = seed + (ch * 999999);  // ← Different seed per channel!

            auto* data = outBuffer.getWritePointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                data[i] = nextFloat();
            }
        }

        DBG("NoiseGenerator: Generated STEREO noise (" +
            juce::String(numChannels) + " independent channels)");
    }

private:
    uint64_t seed;
    uint64_t state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGenerator)
};