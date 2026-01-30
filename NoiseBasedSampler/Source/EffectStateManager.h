/*
EffectStateManager.h - COMPLETE EFFECT STATE MANAGEMENT
✅ Manages state for all audio effects: Trim, Normalize, Reverse, Boost
✅ Thread-safe state storage with original sample backup
✅ Full effect stack application
✅ Constant behavior for trim and normalize
*/
#pragma once
#include <JuceHeader.h>

class EffectStateManager
{
public:
    EffectStateManager() = default;

    // ========== ORIGINAL SAMPLE ==========
    void setOriginalSample(const juce::AudioBuffer<float>& sample)
    {
        originalSample.makeCopyOf(sample);
    }

    bool hasOriginalSample() const
    {
        return originalSample.getNumSamples() > 0;
    }

    const juce::AudioBuffer<float>& getOriginalSample() const
    {
        return originalSample;
    }

    // ========== TRIM (CONSTANT) ==========
    void setTrimActive(bool active, int start = 0, int end = 0)
    {
        trimActive = active;
        trimStart = start;
        trimEnd = end;
    }

    // Constant trim - always applies when active, regardless of other changes
    void updateTrimPoints(int start = 0, int end = 0)
    {
        if (trimActive)
        {
            trimStart = start;
            trimEnd = end;
        }
    }

    bool isTrimActive() const { return trimActive; }
    int getTrimStart() const { return trimStart; }
    int getTrimEnd() const { return trimEnd; }

    // ========== NORMALIZE (CONSTANT) ==========
    void setNormalizeActive(bool active, float targetDb = 0.0f, float gain = 1.0f)
    {
        normalizeActive = active;
        normalizeTargetDb = targetDb;
        normalizeGain = gain;
    }

    // Constant normalize - always applies to target dB regardless of other changes
    void updateNormalizeGain(float targetDb = 0.0f)
    {
        if (normalizeActive && hasOriginalSample())
        {
            // Recalculate peak from current buffer to maintain target dB
            const int numChannels = originalSample.getNumChannels();
            const int numSamples = originalSample.getNumSamples();

            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* data = originalSample.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    peak = juce::jmax(peak, std::abs(data[i]));
            }

            if (peak > 0.0f)
            {
                const float targetLin = std::pow(10.0f, targetDb / 20.0f);
                normalizeGain = targetLin / peak;
                normalizeTargetDb = targetDb;
            }
        }
    }

    bool isNormalizeActive() const { return normalizeActive; }
    float getNormalizeGain() const { return normalizeGain; }
    float getNormalizeTargetDb() const { return normalizeTargetDb; }

    // ========== REVERSE ==========
    void setReverseActive(bool active)
    {
        reverseActive = active;
    }

    bool isReverseActive() const { return reverseActive; }

    // ========== BOOST ==========
    void setBoostActive(bool active, float db = 6.0f, float gain = 2.0f)
    {
        boostActive = active;
        boostDb = db;
        boostGain = gain;
    }

    bool isBoostActive() const { return boostActive; }
    float getBoostDb() const { return boostDb; }
    float getBoostGain() const { return boostGain; }

    // ========== ADSR CUT ITSELF ==========
    bool isAdsrCutItselfMode() const { return adsrCutItselfMode; }

    void setAdsrCutItselfMode(bool enabled)
    {
        adsrCutItselfMode = enabled;
    }

    // ========== EFFECT STACK ==========
    void applyAllEffects(juce::AudioBuffer<float>& buffer)
    {
        if (!hasOriginalSample())
            return;

        // 1️⃣ Start with TRIM (if active) - CONSTANT EFFECT
        if (trimActive)
        {
            int trimmedLength = trimEnd - trimStart + 1;
            buffer.setSize(originalSample.getNumChannels(), trimmedLength, false, true, false);

            for (int ch = 0; ch < originalSample.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, 0, originalSample, ch, trimStart, trimmedLength);
            }
        }
        else
        {
            // No trim - use full original
            buffer.makeCopyOf(originalSample);
        }

        // 2️⃣ Apply REVERSE (if active)
        if (reverseActive)
        {
            applyReverse(buffer);
        }

        // 3️⃣ Apply BOOST (if active)
        if (boostActive)
        {
            applyBoost(buffer);
        }

        // 4️⃣ Apply NORMALIZE (if active, always last) - CONSTANT EFFECT
        if (normalizeActive)
        {
            // ✅ CONSTANT: Recalculate gain to maintain target dB regardless of changes
            updateNormalizeGain(normalizeTargetDb);
            applyNormalize(buffer);
        }
    }

    // ✅ Re-apply trim with current trim settings (for constant behavior)
    void reapplyTrim(juce::AudioBuffer<float>& buffer)
    {
        if (!trimActive || !hasOriginalSample())
            return;

        int trimmedLength = trimEnd - trimStart + 1;

        // Create temporary buffer for trimmed content
        juce::AudioBuffer<float> tempBuffer;
        tempBuffer.setSize(originalSample.getNumChannels(), trimmedLength, false, true, false);

        // Copy from original sample
        for (int ch = 0; ch < originalSample.getNumChannels(); ++ch)
        {
            tempBuffer.copyFrom(ch, 0, originalSample, ch, trimStart, trimmedLength);
        }

        // Apply other effects to trimmed buffer
        if (reverseActive)
        {
            applyReverse(tempBuffer);
        }

        if (boostActive)
        {
            applyBoost(tempBuffer);
        }

        if (normalizeActive)
        {
            applyNormalize(tempBuffer);
        }

        // Copy final result to output buffer
        buffer.makeCopyOf(tempBuffer);
    }

    // ✅ Re-apply normalize to current buffer (for constant behavior)
    void reapplyNormalize(juce::AudioBuffer<float>& buffer)
    {
        if (!normalizeActive || normalizeGain <= 0.0f)
            return;

        buffer.applyGain(normalizeGain);
    }

    void applyNormalize(juce::AudioBuffer<float>& buffer)
    {
        if (!normalizeActive || normalizeGain <= 0.0f)
            return;

        buffer.applyGain(normalizeGain);
    }

    void applyReverse(juce::AudioBuffer<float>& buffer)
    {
        if (!reverseActive)
            return;

        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();

        if (numSamples == 0)
            return;

        // Reverse each channel
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);

            // Swap samples from both ends
            for (int i = 0; i < numSamples / 2; ++i)
            {
                std::swap(data[i], data[numSamples - 1 - i]);
            }
        }

        DBG("🔄 Reverse applied: " + juce::String(numSamples) + " samples reversed");
    }

    // ✅ Apply BOOST
    void applyBoost(juce::AudioBuffer<float>& buffer)
    {
        if (!boostActive || boostGain <= 0.0f)
            return;

        buffer.applyGain(boostGain);

        DBG("➕ Boost applied: +" + juce::String(boostDb, 1) + " dB (gain: " +
            juce::String(boostGain, 4) + ")");
    }

    // ========== RESET ALL EFFECTS ==========
    void reset()
    {
        trimActive = false;
        normalizeActive = false;
        reverseActive = false;
        boostActive = false;
        adsrCutItselfMode = false;
        trimStart = 0;
        trimEnd = 0;
        normalizeTargetDb = 0.0f;
        normalizeGain = 1.0f;
        boostDb = 6.0f;
        boostGain = 2.0f;
        originalSample.setSize(0, 0);
    }

    // ========== UTILITY ==========
    bool hasAnyActiveEffect() const
    {
        return trimActive || normalizeActive || reverseActive || boostActive;
    }

    juce::String getActiveEffectsString() const
    {
        juce::StringArray effects;
        if (trimActive) effects.add("trim");
        if (normalizeActive) effects.add("normalize");
        if (reverseActive) effects.add("reverse");
        if (boostActive) effects.add("boost");

        if (effects.isEmpty())
            return "none";

        return effects.joinIntoString(", ");
    }

private:
    juce::AudioBuffer<float> originalSample;

    // TRIM state
    bool trimActive = false;
    int trimStart = 0;
    int trimEnd = 0;

    // ADSR state
    bool adsrCutItselfMode = false;

    // NORMALIZE state
    bool normalizeActive = false;
    float normalizeTargetDb = 0.0f;
    float normalizeGain = 1.0f;

    // REVERSE state
    bool reverseActive = false;

    // BOOST state
    bool boostActive = false;
    float boostDb = 6.0f;
    float boostGain = 2.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectStateManager)
};

/*
#pragma once
#include <JuceHeader.h>

class EffectStateManager
{
public:
    EffectStateManager() = default;

    // ========== ORIGINAL SAMPLE ==========
    void setOriginalSample(const juce::AudioBuffer<float>& sample)
    {
        originalSample.makeCopyOf(sample);
    }

    bool hasOriginalSample() const
    {
        return originalSample.getNumSamples() > 0;
    }

    const juce::AudioBuffer<float>& getOriginalSample() const
    {
        return originalSample;
    }

    // ========== TRIM (CONSTANT) ==========
    void setTrimActive(bool active, int start = 0, int end = 0)
    {
        trimActive = active;
        trimStart = start;
        trimEnd = end;
    }
    
    // Constant trim - always applies when active, regardless of other changes
    void updateTrimPoints(int start = 0, int end = 0)
    {
        if (trimActive)
        {
            trimStart = start;
            trimEnd = end;
        }
    }

    bool isTrimActive() const { return trimActive; }
    int getTrimStart() const { return trimStart; }
    int getTrimEnd() const { return trimEnd; }

    // ========== NORMALIZE (CONSTANT) ==========
    void setNormalizeActive(bool active, float targetDb = 0.0f, float gain = 1.0f)
    {
        normalizeActive = active;
        normalizeTargetDb = targetDb;
        normalizeGain = gain;
    }
    
    // Constant normalize - always applies to 0 dB regardless of other changes
    void updateNormalizeGain(float targetDb = 0.0f)
    {
        if (normalizeActive && hasOriginalSample())
        {
            // Recalculate peak from current buffer to maintain 0 dB
            const int numChannels = originalSample.getNumChannels();
            const int numSamples = originalSample.getNumSamples();
            
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* data = originalSample.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    peak = juce::jmax(peak, std::abs(data[i]));
            }
            
            if (peak > 0.0f)
            {
                const float targetLin = std::pow(10.0f, targetDb / 20.0f);
                normalizeGain = targetLin / peak;
                normalizeTargetDb = targetDb;
            }
        }
    }

    bool isNormalizeActive() const { return normalizeActive; }
    float getNormalizeGain() const { return normalizeGain; }
    float getNormalizeTargetDb() const { return normalizeTargetDb; }

    // ========== REVERSE ==========
    void setReverseActive(bool active)
    {
        reverseActive = active;
    }

    bool isReverseActive() const { return reverseActive; }

    // ========== ✅ BOOST (НОВОЕ!) ==========
    void setBoostActive(bool active, float db = 6.0f, float gain = 2.0f)
    {
        boostActive = active;
        boostDb = db;
        boostGain = gain;
    }

    bool isBoostActive() const { return boostActive; }
    float getBoostDb() const { return boostDb; }
    float getBoostGain() const { return boostGain; }

    // ========== ADSR CUT ITSELF ==========
    bool isAdsrCutItselfMode() const { return adsrCutItselfMode; }

    void setAdsrCutItselfMode(bool enabled)
    {
        adsrCutItselfMode = enabled;
    }

    // ========== EFFECT STACK ==========
    void applyAllEffects(juce::AudioBuffer<float>& buffer)
    {
        if (!hasOriginalSample())
            return;

        // 1️⃣ Start with TRIM (if active) - CONSTANT EFFECT
        if (trimActive)
        {
            int trimmedLength = trimEnd - trimStart + 1;
            buffer.setSize(originalSample.getNumChannels(), trimmedLength, false, true, false);

            for (int ch = 0; ch < originalSample.getNumChannels(); ++ch)
            {
                buffer.copyFrom(ch, 0, originalSample, ch, trimStart, trimmedLength);
            }
        }
        else
        {
            // No trim - use full original
            buffer.makeCopyOf(originalSample);
        }

        // 2️⃣ ✅ Apply BOOST (if active) - НОВОЕ!
        if (boostActive)
        {
            applyBoost(buffer);
        }

        // 🔕 REVERSE и NORMALIZE теперь работают на уровне amplitude indices,
        // поэтому они применяются через FeatureData::applyToAudioBuffer()
        // 3️⃣ Apply NORMALIZE (if active, always last) - CONSTANT EFFECT
        // 🔕 ОТКЛЮЧЕНО - теперь работает через amplitude indices в PluginProcessor
    }
    
    // ✅ NEW: Re-apply trim with current trim settings (for constant behavior)
    void reapplyTrim(juce::AudioBuffer<float>& buffer)
    {
        if (!trimActive || !hasOriginalSample())
            return;
            
        int trimmedLength = trimEnd - trimStart + 1;
        
        // Create temporary buffer for trimmed content
        juce::AudioBuffer<float> tempBuffer;
        tempBuffer.setSize(originalSample.getNumChannels(), trimmedLength, false, true, false);
        
        // Copy from original sample
        for (int ch = 0; ch < originalSample.getNumChannels(); ++ch)
        {
            tempBuffer.copyFrom(ch, 0, originalSample, ch, trimStart, trimmedLength);
        }
        
        // Apply other effects to trimmed buffer
        if (reverseActive)
        {
            applyReverse(tempBuffer);
        }
        
        if (boostActive)
        {
            applyBoost(tempBuffer);
        }
        
        if (normalizeActive)
        {
            applyNormalize(tempBuffer);
        }
        
        // Copy final result to output buffer
        buffer.makeCopyOf(tempBuffer);
    }
    
    // ✅ NEW: Re-apply normalize to current buffer (for constant behavior)
    void reapplyNormalize(juce::AudioBuffer<float>& buffer)
    {
        if (!normalizeActive || normalizeGain <= 0.0f)
            return;
            
        buffer.applyGain(normalizeGain);
    }

    void applyNormalize(juce::AudioBuffer<float>& buffer)
    {
        if (!normalizeActive || normalizeGain <= 0.0f)
            return;

        buffer.applyGain(normalizeGain);
    }

    void applyReverse(juce::AudioBuffer<float>& buffer)
    {
        if (!reverseActive)
            return;

        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();

        if (numSamples == 0)
            return;

        // Reverse each channel
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);

            // Swap samples from both ends
            for (int i = 0; i < numSamples / 2; ++i)
            {
                std::swap(data[i], data[numSamples - 1 - i]);
            }
        }

        DBG("🔄 Reverse applied: " + juce::String(numSamples) + " samples reversed");
    }

    // ✅ НОВОЕ: Apply BOOST
    void applyBoost(juce::AudioBuffer<float>& buffer)
    {
        if (!boostActive || boostGain <= 0.0f)
            return;

        buffer.applyGain(boostGain);

        DBG("➕ Boost applied: +" + juce::String(boostDb, 1) + " dB (gain: " +
            juce::String(boostGain, 4) + ")");
    }

private:
    juce::AudioBuffer<float> originalSample;

    // TRIM state
    bool trimActive = false;
    int trimStart = 0;
    int trimEnd = 0;

    // ADSR state
    bool adsrCutItselfMode = false;

    // NORMALIZE state
    bool normalizeActive = false;
    float normalizeTargetDb = 0.0f;
    float normalizeGain = 1.0f;

    // REVERSE state
    bool reverseActive = false;

    // ✅ BOOST state (НОВОЕ!)
    bool boostActive = false;
    float boostDb = 6.0f;
    float boostGain = 2.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffectStateManager)
};
*/