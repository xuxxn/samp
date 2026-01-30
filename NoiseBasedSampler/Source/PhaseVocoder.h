/*
PhaseVocoder.h - STEREO VERSION
✅ Может синтезировать как MONO так и STEREO
✅ Правильная нормализация
✅ Сохраняет полную длину аудио
*/

class PhaseVocoder
{
public:
    PhaseVocoder()
        : fft(fftOrder)
    {
        fftData.resize(fftSize * 2);
        windowData.resize(fftSize);
        createHannWindow();
    }

    // ==========================================================================
    // ANALYSIS: Audio → Features (только MONO анализ)
    // ==========================================================================

    FeatureData analyzeAudio(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        FeatureData features;
        int numSamples = buffer.getNumSamples();
        features.setSize(numSamples);

        // ✅ Анализируем ЛЕВЫЙ канал (или mono)
        const auto* data = buffer.getReadPointer(0);

        int hopSize = fftSize / 4;
        int numFrames = (numSamples - fftSize) / hopSize + 1;

        DBG("PhaseVocoder: Analyzing " + juce::String(numFrames) + " frames");

        std::vector<std::vector<float>> frameMagnitudes;
        std::vector<std::vector<float>> framePhases;
        frameMagnitudes.reserve(numFrames);
        framePhases.reserve(numFrames);

        for (int frame = 0; frame < numFrames; ++frame)
        {
            int startSample = frame * hopSize;

            for (int i = 0; i < fftSize; ++i)
            {
                int sampleIdx = startSample + i;
                if (sampleIdx < numSamples)
                    fftData[i] = data[sampleIdx] * windowData[i];
                else
                    fftData[i] = 0.0f;
            }

            fft.performFrequencyOnlyForwardTransform(fftData.data());

            std::vector<float> magnitudes(fftSize / 2);
            std::vector<float> phases(fftSize / 2);

            for (int bin = 0; bin < fftSize / 2; ++bin)
            {
                float real = fftData[bin * 2];
                float imag = fftData[bin * 2 + 1];

                magnitudes[bin] = std::sqrt(real * real + imag * imag);
                phases[bin] = std::atan2(imag, real);
            }

            frameMagnitudes.push_back(magnitudes);
            framePhases.push_back(phases);

            float maxMag = 0.0f;
            int dominantBin = 0;

            for (int bin = 1; bin < fftSize / 2; ++bin)
            {
                if (magnitudes[bin] > maxMag)
                {
                    maxMag = magnitudes[bin];
                    dominantBin = bin;
                }
            }

            float dominantFreq = dominantBin * sampleRate / fftSize;
            float dominantPhase = phases[dominantBin];

            for (int i = 0; i < hopSize && (startSample + i) < numSamples; ++i)
            {
                int idx = startSample + i;
                features[idx].amplitude = data[idx];
                features[idx].frequency = dominantFreq;
                features[idx].phase = dominantPhase;
            }
        }

        lastAnalyzedMagnitudes = frameMagnitudes;
        lastAnalyzedPhases = framePhases;
        lastAnalyzedHopSize = hopSize;
        lastAnalyzedNumSamples = numSamples;
        lastAnalyzedSampleRate = sampleRate;
        cacheValid = true;

        DBG("PhaseVocoder: Analysis complete - cached " + juce::String(numFrames) + " spectra");

        return features;
    }

    // ==========================================================================
    // SYNTHESIS: Features → Audio (MONO или STEREO)
    // ==========================================================================

    void synthesizeAudio(const FeatureData& features,
        juce::AudioBuffer<float>& outputBuffer,
        double sampleRate)
    {
        int numSamples = features.getNumSamples();
        int outputChannels = outputBuffer.getNumChannels();

        // Проверка кэша
        if (!isCacheValid(numSamples, sampleRate))
        {
            DBG("⚠️ Cached spectra INVALID - using simple synthesis");
            synthesizeSimple(features, outputBuffer, sampleRate);
            return;
        }

        DBG("✅ Cache VALID - using cached spectra");

        // ✅ Обеспечиваем полную длину output
        outputBuffer.setSize(outputChannels, numSamples, false, true, false);
        outputBuffer.clear();

        int hopSize = lastAnalyzedHopSize;
        int numFrames = static_cast<int>(lastAnalyzedMagnitudes.size());

        // ✅ Accumulation buffers для overlap-add
        std::vector<float> accumBuffer(numSamples + fftSize, 0.0f);
        std::vector<float> windowAccum(numSamples + fftSize, 0.0f);

        DBG("PhaseVocoder: Synthesizing " + juce::String(numFrames) + " frames → " +
            juce::String(outputChannels) + " channels");

        // Вычисляем amplitude scale
        float amplitudeScale = 1.0f;

        if (numFrames > 0 && numSamples > 0)
        {
            auto stats = features.calculateStatistics();
            float currentAvgAmp = stats.avgAmplitude;

            float originalAvgAmp = 0.0f;
            for (const auto& mag : lastAnalyzedMagnitudes[0])
                originalAvgAmp += mag;
            originalAvgAmp /= lastAnalyzedMagnitudes[0].size();

            if (originalAvgAmp > 0.001f)
                amplitudeScale = currentAvgAmp / originalAvgAmp;

            amplitudeScale = juce::jlimit(0.5f, 2.0f, amplitudeScale);

            DBG("Amplitude scale: " + juce::String(amplitudeScale, 3));
        }

        // ✅ Synthesis loop (создаём MONO базу)
        for (int frame = 0; frame < numFrames; ++frame)
        {
            int startSample = frame * hopSize;

            const auto& magnitudes = lastAnalyzedMagnitudes[frame];
            const auto& phases = lastAnalyzedPhases[frame];

            std::fill(fftData.begin(), fftData.end(), 0.0f);

            // Reconstruct spectrum
            for (int bin = 0; bin < fftSize / 2; ++bin)
            {
                float modifiedMag = magnitudes[bin] * amplitudeScale;
                float modifiedPhase = phases[bin];

                fftData[bin * 2] = modifiedMag * std::cos(modifiedPhase);
                fftData[bin * 2 + 1] = modifiedMag * std::sin(modifiedPhase);
            }

            // IFFT
            fft.performRealOnlyInverseTransform(fftData.data());

            // Overlap-add
            for (int i = 0; i < fftSize; ++i)
            {
                int outputIdx = startSample + i;

                if (outputIdx >= 0 && outputIdx < static_cast<int>(accumBuffer.size()))
                {
                    float windowValue = windowData[i];
                    float sample = fftData[i] * windowValue;

                    accumBuffer[outputIdx] += sample;
                    windowAccum[outputIdx] += windowValue * windowValue;
                }
            }
        }

        // ✅ Нормализация с сохранением энергии
        float inputEnergy = 0.0f;
        float outputEnergy = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            if (windowAccum[i] > 0.001f)
            {
                accumBuffer[i] /= windowAccum[i];
                outputEnergy += accumBuffer[i] * accumBuffer[i];
            }
            else
            {
                accumBuffer[i] = 0.0f;
            }

            inputEnergy += features[i].amplitude * features[i].amplitude;
        }

        // Энергетическая компенсация
        if (outputEnergy > 0.0f && inputEnergy > 0.0f)
        {
            float energyRatio = std::sqrt(inputEnergy / outputEnergy);
            energyRatio = juce::jlimit(0.5f, 2.0f, energyRatio);

            if (std::abs(energyRatio - 1.0f) > 0.1f)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    accumBuffer[i] *= energyRatio;
                }
                DBG("Applied energy compensation: " + juce::String(energyRatio, 3) + "x");
            }
        }

        // ✅ Soft clipping
        float maxValue = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            maxValue = std::max(maxValue, std::abs(accumBuffer[i]));
        }

        if (maxValue > 0.95f)
        {
            DBG("⚠️ Signal clipping detected, applying soft limiter");

            for (int i = 0; i < numSamples; ++i)
            {
                if (std::abs(accumBuffer[i]) > 0.8f)
                {
                    float sign = (accumBuffer[i] > 0.0f) ? 1.0f : -1.0f;
                    float absVal = std::abs(accumBuffer[i]);
                    float compressed = 0.8f + (absVal - 0.8f) * 0.5f;
                    accumBuffer[i] = sign * juce::jlimit(0.0f, 1.0f, compressed);
                }
            }
        }

        // ✅ КОПИРУЕМ во ВСЕ выходные каналы
        // (Pan будет применён позже в FeatureData::applyToAudioBuffer)
        for (int ch = 0; ch < outputChannels; ++ch)
        {
            auto* output = outputBuffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                output[i] = accumBuffer[i];
            }
        }

        DBG("PhaseVocoder: Synthesis complete");
        DBG("  Output channels: " + juce::String(outputChannels));
        DBG("  Output length: " + juce::String(numSamples) + " samples");
    }

    void invalidateCache()
    {
        cacheValid = false;
        lastAnalyzedMagnitudes.clear();
        lastAnalyzedPhases.clear();
        lastAnalyzedHopSize = 0;
        lastAnalyzedNumSamples = 0;
        lastAnalyzedSampleRate = 0.0;

        DBG("PhaseVocoder: Cache invalidated");
    }

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    juce::dsp::FFT fft;
    std::vector<float> windowData;
    std::vector<float> fftData;

    std::vector<std::vector<float>> lastAnalyzedMagnitudes;
    std::vector<std::vector<float>> lastAnalyzedPhases;
    int lastAnalyzedHopSize = 0;
    int lastAnalyzedNumSamples = 0;
    double lastAnalyzedSampleRate = 0.0;
    bool cacheValid = false;

    bool isCacheValid(int numSamples, double sampleRate) const
    {
        if (!cacheValid || lastAnalyzedMagnitudes.empty())
            return false;

        const float SAMPLE_TOLERANCE = 0.01f;

        bool samplesMatch = std::abs(numSamples - lastAnalyzedNumSamples) <
            (lastAnalyzedNumSamples * SAMPLE_TOLERANCE);

        bool sampleRateMatch = std::abs(sampleRate - lastAnalyzedSampleRate) < 1.0;

        return samplesMatch && sampleRateMatch;
    }

    void createHannWindow()
    {
        for (int i = 0; i < fftSize; ++i)
        {
            windowData[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
        }
    }

    void synthesizeSimple(const FeatureData& features,
        juce::AudioBuffer<float>& outputBuffer,
        double sampleRate)
    {
        DBG("Using simple synthesis fallback");

        int numSamples = features.getNumSamples();
        int outputChannels = outputBuffer.getNumChannels();

        outputBuffer.setSize(outputChannels, numSamples, false, true, false);

        for (int ch = 0; ch < outputChannels; ++ch)
        {
            auto* output = outputBuffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                output[i] = features[i].amplitude;
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseVocoder)
};