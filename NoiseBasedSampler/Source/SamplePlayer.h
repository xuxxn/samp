#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include "EffectStateManager.h"

// Forward declaration
class EffectStateManager;

class SamplePlayer
{
public:
    enum class InterpolationMode
    {
        Linear,
        Cubic,
        Sinc,
        Adaptive
    };

    // Voice structure for MainPanel access
    struct Voice
    {
        bool isPlaying = false;
        bool isReleasing = false;

        int midiNote = 0;
        float velocity = 1.0f;
        double currentPosition = 0.0;  // ✅ Всегда 0..rangeLength
        double pitchRatio = 1.0;
        int direction = 1;  // ✅ 1 = forward ➡️, -1 = reverse ⬅️

        juce::ADSR envelope;
    };

    SamplePlayer()
    {
        initializeSincTable();
        effectStateManager = nullptr;
        DBG("🔧 SamplePlayer: With FIXED Reverse support loaded");
    }

    void setEffectStateManager(class EffectStateManager* manager)
    {
        effectStateManager = manager;
    }

    void prepare(int numChannels, double sampleRate, int maximumBlockSize)
    {
        currentSampleRate = sampleRate;
        DBG("🔧 SamplePlayer prepared: " + juce::String(sampleRate, 0) + " Hz, " +
            juce::String(maximumBlockSize) + " samples, " +
            juce::String(numChannels) + " channels");
    }

    void setSample(const juce::AudioBuffer<float>& newSample)
    {
        const juce::ScopedLock sl(lock);

        sampleBuffer.makeCopyOf(newSample);
        sampleLength = sampleBuffer.getNumSamples();
        sampleChannels = sampleBuffer.getNumChannels();

        DBG("✅ Sample loaded: " + juce::String(sampleLength) + " samples, " +
            juce::String(sampleChannels) + " channels");

        // Check if sample has actual audio
        float maxAmp = 0.0f;
        for (int ch = 0; ch < sampleChannels; ++ch)
        {
            auto* data = sampleBuffer.getReadPointer(ch);
            for (int i = 0; i < sampleLength; ++i)
            {
                maxAmp = juce::jmax(maxAmp, std::abs(data[i]));
            }
        }

        if (maxAmp < 0.001f)
        {
            DBG("⚠️ Warning: Sample appears to be silent!");
        }
    }

    void setPlaybackRange(int startSample, int endSample)
    {
        playbackStartSample = juce::jmax(0, startSample);
        playbackEndSample = juce::jmax(playbackStartSample, endSample);
        hasPlaybackRange = true;

        DBG("🎛️ SamplePlayer range: " + juce::String(playbackStartSample) +
            " to " + juce::String(playbackEndSample) +
            " (length: " + juce::String(playbackEndSample - playbackStartSample) + ")");
    }

    void clearPlaybackRange()
    {
        playbackStartSample = 0;
        playbackEndSample = -1;
        hasPlaybackRange = false;
    }

    void setCutItselfMode(bool enabled)
    {
        cutItselfMode = enabled;

        if (enabled)
        {
            DBG("🎹 SamplePlayer: Cut Itself mode ON (FL Studio style)");
        }
        else
        {
            DBG("🎹 SamplePlayer: Cut Itself mode OFF (normal polyphony)");
        }
    }

    bool isCutItselfMode() const
    {
        return cutItselfMode;
    }

    void noteOn(int midiNoteNumber, float velocity)
    {
        if (sampleLength == 0)
        {
            DBG("⚠️ Cannot play: no sample loaded");
            return;
        }

        const juce::ScopedLock sl(lock);

        // ✅ Cut Itself Mode: Stop ALL currently playing voices BEFORE adding new one
        if (cutItselfMode)
        {
            DBG("🎹 Cut Itself: Stopping all voices before new note");

            for (auto& voice : voices)
            {
                if (voice->isPlaying)
                {
                    voice->isPlaying = false;
                    voice->isReleasing = false;
                }
            }

            voices.clear();
        }
        else
        {
            if (voices.size() >= static_cast<size_t>(maxVoices))
            {
                stealOldestVoice();
            }
        }

        // Create new voice
        auto voice = std::make_unique<Voice>();
        voice->isPlaying = true;
        voice->currentPosition = 0.0;
        voice->velocity = velocity;
        voice->midiNote = midiNoteNumber;

        // ✅ Direction is now handled by EffectStateManager - always play forward
        voice->direction = 1;

        // Calculate pitch ratio
        double semitones = midiNoteNumber - 60;
        voice->pitchRatio = std::pow(2.0, semitones / 12.0);
        voice->pitchRatio = juce::jlimit(0.00390625, 256.0, voice->pitchRatio);

        voice->envelope.setSampleRate(currentSampleRate);
        voice->envelope.setParameters(envelopeParams);
        voice->envelope.reset();
        voice->envelope.noteOn();

        voices.push_back(std::move(voice));

        DBG("🎵 Voice started: Note=" + juce::String(midiNoteNumber) +
            ", Velocity=" + juce::String(velocity, 2) +
            ", Ratio=" + juce::String(voice->pitchRatio, 3) +
            ", ADSR=" + (adsrEnabled ? "ON" : "OFF") +
            ", Cut Itself=" + (cutItselfMode ? "ON" : "OFF") +
            ", Direction=FORWARD ➡️ (Reverse handled by EffectStateManager)");
    }

    void noteOff(int midiNoteNumber)
    {
        const juce::ScopedLock sl(lock);

        for (auto& voice : voices)
        {
            if (voice->isPlaying && voice->midiNote == midiNoteNumber && !voice->isReleasing)
            {
                voice->envelope.noteOff();
                voice->isReleasing = true;
                DBG("🎵 Note OFF: Note=" + juce::String(midiNoteNumber) + " (release started)");
            }
        }
    }

    void allNotesOff()
    {
        const juce::ScopedLock sl(lock);

        for (auto& voice : voices)
        {
            if (voice->isPlaying)
            {
                voice->isReleasing = true;
                voice->envelope.noteOff();
            }
        }
        DBG("🔇 All notes OFF");
    }

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
    {
        const juce::ScopedLock sl(lock);
        if (sampleLength == 0) return;

        int numChannels = outputBuffer.getNumChannels();

        // Debug counter
        static int blockCounter = 0;
        blockCounter++;
        bool shouldLog = (blockCounter % 100 == 0); // Log every 100th block

        if (shouldLog && !voices.empty())
        {
            DBG("📊 Rendering: " + juce::String(voices.size()) + " active voices");
        }

        for (auto& voice : voices)
        {
            if (!voice->isPlaying) continue;

            float totalEnergy = 0.0f; // Track output energy for debugging

            // ✅ ИСПРАВЛЕНО: Правильное вычисление диапазона
            int rangeStart = hasPlaybackRange ? playbackStartSample : 0;
            int rangeEnd = hasPlaybackRange ? playbackEndSample : sampleLength;
            int rangeLength = rangeEnd - rangeStart;

            for (int i = 0; i < numSamples; ++i)
            {
                // ✅ ИСПРАВЛЕНО: Правильное вычисление readPosition
                double readPosition;

                // ✅ ALWAYS play forward - reverse handled by EffectStateManager
                readPosition = rangeStart + voice->currentPosition;

                // Check end boundary
                if (readPosition >= (rangeEnd - SINC_POINTS - 1))
                {
                    voice->isPlaying = false;
                    DBG("🏁 Voice finished");
                    break;
                }

                float envelopeValue = adsrEnabled ? voice->envelope.getNextSample() : 1.0f;

                if (adsrEnabled && voice->isReleasing && envelopeValue < 0.001f)
                {
                    voice->isPlaying = false;
                    DBG("🏁 Voice finished (envelope)");
                    break;
                }

                // Read samples using CUBIC (most reliable)
                float sampleLeft, sampleRight;
                if (sampleChannels == 1)
                {
                    sampleLeft = getCubicInterpolated(0, readPosition);
                    sampleRight = sampleLeft;
                }
                else
                {
                    sampleLeft = getCubicInterpolated(0, readPosition);
                    sampleRight = getCubicInterpolated(1, readPosition);
                }

                // Apply velocity and envelope
                sampleLeft *= voice->velocity * envelopeValue;
                sampleRight *= voice->velocity * envelopeValue;

                // Track energy
                totalEnergy += std::abs(sampleLeft) + std::abs(sampleRight);

                // Stereo width
                float mid = (sampleLeft + sampleRight) * 0.5f;
                float side = (sampleLeft - sampleRight) * 0.5f;
                side *= stereoWidth;

                // Panning
                float leftGain = std::cos(pan * juce::MathConstants<float>::halfPi);
                float rightGain = std::sin(pan * juce::MathConstants<float>::halfPi);

                if (numChannels >= 2)
                {
                    outputBuffer.addSample(0, startSample + i, (mid + side) * leftGain);
                    outputBuffer.addSample(1, startSample + i, (mid - side) * rightGain);
                }
                else if (numChannels == 1)
                {
                    outputBuffer.addSample(0, startSample + i, mid);
                }

                // ✅ ИСПРАВЛЕНО: currentPosition ВСЕГДА инкрементируется
                // (это "пройденное расстояние", не зависит от direction)
                voice->currentPosition += voice->pitchRatio;
            }

            // Log energy for this voice
            if (shouldLog)
            {
                float avgEnergy = totalEnergy / (numSamples * 2.0f);
                DBG("  Voice energy: " + juce::String(avgEnergy, 6) +
                    ", Pos: " + juce::String(voice->currentPosition, 0) +
                    "/" + juce::String(rangeLength) +
                    ", Dir: " + (voice->direction >= 0 ? "FWD" : "REV"));
            }
        }

        cleanupFinishedVoices();
    }

    // Setters
    void setADSREnabled(bool enabled)
    {
        adsrEnabled = enabled;
        DBG("ADSR " + juce::String(enabled ? "ENABLED" : "DISABLED"));
    }

    bool isADSREnabled() const { return adsrEnabled; }

    void setPan(float newPan) { pan = juce::jlimit(0.0f, 1.0f, newPan); }
    void setStereoWidth(float newWidth) { stereoWidth = juce::jlimit(0.0f, 2.0f, newWidth); }
    void setInterpolationMode(InterpolationMode mode) { interpolationMode = mode; }
    void setADSRParameters(const juce::ADSR::Parameters& params) { envelopeParams = params; }

    void setSampleRate(double sampleRate)
    {
        currentSampleRate = sampleRate;
        DBG("Sample rate: " + juce::String(sampleRate, 0) + " Hz");

        const juce::ScopedLock sl(lock);
        for (auto& voice : voices)
        {
            voice->envelope.setSampleRate(sampleRate);
        }
    }

    void setEnvelope(const juce::ADSR::Parameters& params)
    {
        envelopeParams = params;
        const juce::ScopedLock sl(lock);
        for (auto& voice : voices)
        {
            voice->envelope.setParameters(params);
        }
    }

    int getActiveVoiceCount() const
    {
        const juce::ScopedLock sl(lock);
        return static_cast<int>(voices.size());
    }

    // ✅ Play position methods for MainPanel
    bool isAnyVoicePlaying() const
    {
        const juce::ScopedLock sl(lock);
        return std::any_of(voices.begin(), voices.end(), 
            [](const std::unique_ptr<Voice>& voice) { return voice->isPlaying; });
    }

    float getCurrentPlayPosition() const
    {
        const juce::ScopedLock sl(lock);
        for (const auto& voice : voices)
        {
            if (voice->isPlaying && !voice->isReleasing)
            {
                return static_cast<float>(voice->currentPosition);
            }
        }
        return 0.0f;
    }

    float getCurrentPitchRatio() const
    {
        const juce::ScopedLock sl(lock);
        for (const auto& voice : voices)
        {
            if (voice->isPlaying && !voice->isReleasing)
            {
                return static_cast<float>(voice->pitchRatio);
            }
        }
        return 1.0f;
    }

    template<typename Func>
    void forEachVoice(Func&& func) const
    {
        const juce::ScopedLock sl(lock);
        for (const auto& voice : voices)
        {
            func(voice.get());
        }
    }

private:
    static constexpr int SINC_POINTS = 8;
    static constexpr int SINC_RESOLUTION = 4096;
    static constexpr int SINC_TABLE_SIZE = SINC_POINTS * SINC_RESOLUTION;

    mutable juce::CriticalSection lock;
    juce::AudioBuffer<float> sampleBuffer;
    std::vector<std::unique_ptr<Voice>> voices;

    int sampleLength = 0;
    int sampleChannels = 0;
    int maxVoices = 128;

    float pan = 0.5f;
    float stereoWidth = 1.0f;
    double currentSampleRate = 44100.0;

    bool hasPlaybackRange = false;
    int playbackStartSample = 0;
    int playbackEndSample = -1;

    bool cutItselfMode = false;
    bool adsrEnabled = false;

    EffectStateManager* effectStateManager = nullptr;

    juce::ADSR::Parameters envelopeParams;
    InterpolationMode interpolationMode = InterpolationMode::Cubic;

    std::vector<float> sincTable;

    void initializeSincTable()
    {
        sincTable.resize(SINC_TABLE_SIZE);

        for (int i = 0; i < SINC_POINTS; ++i)
        {
            float center = (float)(i - SINC_POINTS / 2);
            for (int j = 0; j < SINC_RESOLUTION; ++j)
            {
                float x = center + (float)j / SINC_RESOLUTION;
                float sinc = (x == 0.0f) ? 1.0f : std::sin(juce::MathConstants<float>::pi * x) / (juce::MathConstants<float>::pi * x);

                // Window function (Blackman-Harris)
                float window = 0.35875f - 0.48829f * std::cos(2.0f * juce::MathConstants<float>::pi * j / SINC_TABLE_SIZE) +
                    0.14128f * std::cos(4.0f * juce::MathConstants<float>::pi * j / SINC_TABLE_SIZE) -
                    0.01168f * std::cos(6.0f * juce::MathConstants<float>::pi * j / SINC_TABLE_SIZE);

                sincTable[i * SINC_RESOLUTION + j] = sinc * window;
            }
        }
        DBG("✅ Sinc table initialized: " + juce::String(SINC_TABLE_SIZE) + " points");
    }

    float getCubicInterpolated(int channel, double position) const
    {
        if (position < 0.0 || position >= sampleLength - 1)
            return 0.0f;

        int pos = static_cast<int>(position);
        float fraction = position - pos;

        const float* data = sampleBuffer.getReadPointer(channel);

        // Cubic interpolation using 4 points
        float y0 = data[std::max(0, pos - 1)];
        float y1 = data[pos];
        float y2 = data[std::min(sampleLength - 1, pos + 1)];
        float y3 = data[std::min(sampleLength - 1, pos + 2)];

        float a = y3 - y2 - y0 + y1;
        float b = y0 - y1 - a;
        float c = y2 - y0;
        float d = y1;

        return a * fraction * fraction * fraction + b * fraction * fraction + c * fraction + d;
    }

    void cleanupFinishedVoices()
    {
        voices.erase(
            std::remove_if(voices.begin(), voices.end(),
                [](const std::unique_ptr<Voice>& voice) { return !voice->isPlaying; }),
            voices.end());
    }

    void stealOldestVoice()
    {
        if (!voices.empty())
        {
            voices.front()->isPlaying = false;
            voices.front()->isReleasing = false;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplePlayer)
};