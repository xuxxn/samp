/*
PluginProcessor.h - FIXED: Removed duplicate function definition
Ã¢Å“â€¦ deletePatternRemoveSamples properly declared once
Ã¢Å“â€¦ exportIndicesAsync properly closed
*/
#pragma once
#include <JuceHeader.h>
#include "NoiseGenerator.h"
#include "DifferenceEngine.h"
#include "SamplePlayer.h"
#include "Pattern.h"
#include "PatternDetector.h"
#include "PatternLibrary.h"
#include "MLEvolutionEngine.h"
#include "FeatureData.h"
#include "FeatureExtractor.h"
#include "SimpleIndexExporter.h"
#include "SpectralIndexData.h"
#include "SpectralIndexAnalyzer.h"
#include "SpectralIndexDatabase.h"
#include "AudioStateManager.h"
#include "PatternAnalyzer.h"
#include "UIStateManager.h"
#include "AlgorithmEngine.h"
#include "AlgorithmFileManager.h"
#include "EffectStateManager.h"
#include "ProjectManager.h"
#include "Core/VersionInfo.h"
#include "ProjectData.h"

class NoiseBasedSamplerAudioProcessor : public juce::AudioProcessor
{
public:
    NoiseBasedSamplerAudioProcessor();
    ~NoiseBasedSamplerAudioProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    const juce::CriticalSection& getSampleLock() const { return sampleLock; }

    void setFeatureVolumeAt(int index, float value)
    {
        featureData.setVolumeAt(index, value);
    }

    void setFeaturePanAt(int index, float value)
    {
        featureData.setPanAt(index, value);
    }

    ProjectManager* getProjectManager() {
        return projectManager.get();
    }

    juce::String getCurrentSampleName() const {
        return currentSampleName;
    }

    void setCurrentSampleName(const juce::String& name) {
        currentSampleName = name;
        if (projectManager)
            projectManager->markDirty();
    }

    // For ProjectManager to access processor state
    const juce::AudioBuffer<float>& getOriginalSample() const {
        return originalSample;
    }

    void setOriginalSample(const juce::AudioBuffer<float>& buffer) {
        originalSample.makeCopyOf(buffer);
        outputBuffer.makeCopyOf(buffer);
        effectStateManager.setOriginalSample(buffer);
        sampleLoaded = true;
        if (projectManager)
            projectManager->markDirty();
    }

    void setFeatureData(const FeatureData& features) {
        featureData = features;
        if (projectManager)
            projectManager->markDirty();
    }

    void setPreviewAudio(const juce::AudioBuffer<float>& buffer) {
        // Preview only - no dirty flag
        samplePlayer.setSample(buffer);
    }

    void triggerSample() {
        samplePlayer.noteOn(60, 1.0f);  // Middle C, full velocity
    }

    void setSampleForPlayback(const juce::AudioBuffer<float>& buffer) {
        samplePlayer.setSample(buffer);
    }

    double getCurrentSampleRate() const {
        return currentSampleRate;
    }

    const EffectStateManager& getEffectStateManager() const {
        return effectStateManager;
    }

    EffectStateManager& getEffectStateManager() {
        return effectStateManager;
    }

    AudioStateManager& getAudioStateManager() {
        return audioState;
    }

    UIStateManager& getUIStateManager() {
        return uiState;
    }

    // New tool getters
    float getBoostDb() const { return boostDbParam ? boostDbParam->get() : 0.0f; }
    float getPitchShift() const { return pitchShiftParam ? pitchShiftParam->get() : 0.0f; }
    float getTimeStretch() const { return timeStretchParam ? timeStretchParam->get() : 1.0f; }
    bool isLoopActive() const { return loopActiveParam ? loopActiveParam->get() : false; }

    // New tool setters for CMDTerminalToolsSection
    void setBoostDb(float value) { if (boostDbParam) *boostDbParam = value; }
    void setPitchShift(float value) { if (pitchShiftParam) *pitchShiftParam = value; }
    void setTimeStretch(float value) { if (timeStretchParam) *timeStretchParam = value; }
    void setLoopActive(bool active) { if (loopActiveParam) *loopActiveParam = active; }

    // Real-time audio effects processing
    void applyRealtimeEffects(juce::AudioBuffer<float>& buffer);

    bool areSpectralIndicesSynced() const;

    void beginSampleRangePreview()
    {
        const juce::ScopedLock sl(sampleLock);

        // Store original state for preview
        previewFeatureData = featureData;
        isPreviewingRange = true;
    }

    void previewSampleStart(float startPercent)
    {
        if (!isPreviewingRange) return;

        const juce::ScopedLock sl(sampleLock);

        currentStartPercent = startPercent;

        // âœ… Don't modify featureData - just update the offset
        setSampleStartOffset(startPercent);

        // Keep original featureData intact for preview
        if (previewFeatureData.getNumSamples() > 0)
        {
            featureData = previewFeatureData;
        }
    }

    void previewSampleLength(float lengthPercent)
    {
        if (!isPreviewingRange) return;

        const juce::ScopedLock sl(sampleLock);

        currentLengthPercent = lengthPercent;

        // âœ… Don't modify featureData - just update the length
        setSamplePlaybackLength(lengthPercent);

        // Keep original featureData intact for preview
        if (previewFeatureData.getNumSamples() > 0)
        {
            featureData = previewFeatureData;
        }
    }

    void applySampleStart(float startPercent)
    {
        const juce::ScopedLock sl(sampleLock);

        isPreviewingRange = false;
        currentStartPercent = startPercent;

        // âœ… Use the proper offset system instead of modifying featureData
        setSampleStartOffset(startPercent);

        // Restore original featureData if it was modified
        if (previewFeatureData.getNumSamples() > 0)
        {
            featureData = previewFeatureData;
        }

        DBG("âœ… Sample START applied: " + juce::String(startPercent * 100, 1) + "%");
    }

    void applySampleLength(float lengthPercent)
    {
        const juce::ScopedLock sl(sampleLock);

        isPreviewingRange = false;
        currentLengthPercent = lengthPercent;

        // âœ… Use the proper length system instead of modifying featureData
        setSamplePlaybackLength(lengthPercent);

        // Restore original featureData if it was modified
        if (previewFeatureData.getNumSamples() > 0)
        {
            featureData = previewFeatureData;
        }

        DBG("âœ… Sample LENGTH applied: " + juce::String(lengthPercent * 100, 1) + "%");
    }

    float getSampleStartPercent() const { return currentStartPercent; }
    float getSampleLengthPercent() const { return currentLengthPercent; }

    bool isAdsrCutItselfMode() const
    {
        return effectStateManager.isAdsrCutItselfMode();
    }

    void toggleAdsrCutItselfMode()
    {
        bool newState = !effectStateManager.isAdsrCutItselfMode();
        effectStateManager.setAdsrCutItselfMode(newState);

        // Apply to SamplePlayer
        samplePlayer.setCutItselfMode(newState);

        DBG(newState ? "âœ… ADSR Cut Itself: ON" : "âšª ADSR Cut Itself: OFF");
    }

    // ========== TRIM (REVERSIBLE) ==========

    bool isTrimActive() const
    {
        return effectStateManager.isTrimActive();
    }

    void toggleTrim(float thresholdDb = -60.0f)
    {
        const juce::ScopedLock sl(sampleLock);

        if (!sampleLoaded || originalSample.getNumSamples() == 0)
            return;

        bool currentState = effectStateManager.isTrimActive();

        if (currentState)
        {
            // Disable trim - restore original
            DBG("âšª TRIM: OFF - Restoring original");
            effectStateManager.setTrimActive(false);
            applyEffectStack();
        }
        else
        {
            // Enable trim - calculate trim points FROM CURRENT OUTPUT (for constant behavior)
            DBG("âœ… TRIM: ON - Calculating trim points...");

            // Use current output buffer (with all modifications) for trim calculation
            const int numChannels = outputBuffer.getNumChannels();
            const int numSamples = outputBuffer.getNumSamples();
            const float thresholdLin = std::pow(10.0f, thresholdDb / 20.0f);

            int start = 0;
            int end = numSamples - 1;

            // Find first non-silent sample in CURRENT buffer
            bool foundStart = false;
            for (int i = 0; i < numSamples; ++i)
            {
                float maxAtSample = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    maxAtSample = juce::jmax(maxAtSample,
                        std::abs(outputBuffer.getSample(ch, i)));

                if (maxAtSample >= thresholdLin)
                {
                    start = i;
                    foundStart = true;
                    break;
                }
            }

            if (!foundStart)
            {
                DBG("âš ï¸ Nothing above threshold - trim cancelled");
                return;
            }

            // Find last non-silent sample in CURRENT buffer
            for (int i = numSamples - 1; i >= 0; --i)
            {
                float maxAtSample = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    maxAtSample = juce::jmax(maxAtSample,
                        std::abs(outputBuffer.getSample(ch, i)));

                if (maxAtSample >= thresholdLin)
                {
                    end = i;
                    break;
                }
            }

            int trimmedLength = end - start + 1;

            DBG("  Start: " + juce::String(start) +
                " | End: " + juce::String(end) +
                " | Length: " + juce::String(trimmedLength));

            // Set trim active and immediately apply to create constant behavior
            effectStateManager.setTrimActive(true, start, end);
            applyEffectStack();

            // ✅ CONSTANT: Store original sample before trimming for future modifications
            effectStateManager.setOriginalSample(outputBuffer);
        }
    }

    // ========== NORMALIZE (REVERSIBLE) ==========

    bool isNormalizeActive() const
    {
        return effectStateManager.isNormalizeActive();
    }

    bool isReverseActive() const
    {
        return effectStateManager.isReverseActive();
    }

    void toggleReverse()
    {
        const juce::ScopedLock sl(sampleLock);

        if (!sampleLoaded || originalSample.getNumSamples() == 0)
            return;

        DBG("🔄 REVERSE - Audio buffer first, then sync features");

        // ✅ ШАГ 1: РЕВЕРСИРУЕМ АУДИО БУФЕР (основная операция)
        for (int ch = 0; ch < originalSample.getNumChannels(); ++ch)
        {
            float* channelData = originalSample.getWritePointer(ch);
            int numSamples = originalSample.getNumSamples();

            // Reverse in-place
            std::reverse(channelData, channelData + numSamples);
        }

        // ✅ ШАГ 2: СИНХРОНИЗИРУЕМ FEATURES С НОВЫМ АУДИО
        // Извлекаем features из перевернутого audio buffer
        juce::AudioBuffer<float> monoForAnalysis(1, originalSample.getNumSamples());
        monoForAnalysis.copyFrom(0, 0, originalSample, 0, 0, originalSample.getNumSamples());

        featureData = featureExtractor.extractAmplitudeOnly(monoForAnalysis, currentSampleRate);

        // ✅ ШАГ 3: Обновляем все зависимые буферы
        outputBuffer.makeCopyOf(originalSample);
        samplePlayer.setSample(outputBuffer);

        featuresModifiedByUser = true;

        DBG("✅ REVERSE: Audio reversed, features synced - clean sound");
    }

    bool isBoostActive() const
    {
        return effectStateManager.isBoostActive();
    }

    void toggleBoost(float boostDb = 6.0f)
    {
        const juce::ScopedLock sl(sampleLock);

        if (!sampleLoaded || originalSample.getNumSamples() == 0)
            return;

        bool currentState = effectStateManager.isBoostActive();

        if (currentState)
        {
            // Disable boost - restore original
            DBG("⬇️ BOOST: OFF - Restoring original");
            effectStateManager.setBoostActive(false);
            applyEffectStack();
        }
        else
        {
            // Enable boost - calculate gain
            DBG("⬆️ BOOST: ON - Applying +" + juce::String(boostDb, 1) + " dB");

            float boostGain = std::pow(10.0f, boostDb / 20.0f);

            DBG("  Boost gain: " + juce::String(boostGain, 4));

            effectStateManager.setBoostActive(true, boostDb, boostGain);
            applyEffectStack();
        }
    }

    void setBoostLevel(float boostDb)
    {
        const juce::ScopedLock sl(sampleLock);

        if (!sampleLoaded || originalSample.getNumSamples() == 0)
            return;

        if (boostDb == 0.0f)
        {
            // Turn off boost
            DBG("⬇️ BOOST: OFF - Setting to 0dB");
            effectStateManager.setBoostActive(false);
            applyEffectStack();
        }
        else
        {
            // Set boost level
            DBG("🎛️ BOOST: Setting to " + juce::String(boostDb, 1) + " dB");

            float boostGain = std::pow(10.0f, boostDb / 20.0f);

            DBG("  Boost gain: " + juce::String(boostGain, 4));

            effectStateManager.setBoostActive(true, boostDb, boostGain);
            applyEffectStack();
        }
    }

    void toggleNormalize(float targetDb = 0.0f)
    {
        const juce::ScopedLock sl(sampleLock);

        if (!sampleLoaded || outputBuffer.getNumSamples() == 0)
            return;

        bool currentState = effectStateManager.isNormalizeActive();

        if (currentState)
        {
            // Disable normalize - just turn it off
            DBG("âšª NORMALIZE: OFF");
            effectStateManager.setNormalizeActive(false);
        }
        else
        {
            // Enable normalize - работаем с CURRENT OUTPUT BUFFER для real-time
            DBG("âœ… NORMALIZE: ON - Real-time mode...");

            // Find peak in CURRENT output buffer (после всех изменений)
            float peak = 0.0f;
            const int numChannels = outputBuffer.getNumChannels();
            const int numSamples = outputBuffer.getNumSamples();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float* data = outputBuffer.getReadPointer(ch);
                for (int i = 0; i < numSamples; ++i)
                    peak = juce::jmax(peak, std::abs(data[i]));
            }

            if (peak <= 0.0f)
            {
                DBG("âš ï¸ Silent audio - normalize cancelled");
                return;
            }

            const float targetLin = std::pow(10.0f, targetDb / 20.0f);
            const float gain = targetLin / peak;

            DBG("  Peak: " + juce::String(peak, 4) +
                " | Target: " + juce::String(targetLin, 4) +
                " | Gain: " + juce::String(gain, 4));

            // ✅ НЕ МОДИФИЦИРУЕМ amplitude indices!
            // Просто сохраняем gain для real-time применения
            effectStateManager.setNormalizeActive(true, targetDb, gain);

            // ✅ ПРИМЕНЯЕМ НЕМЕДЛЕННО к output buffer
            outputBuffer.applyGain(gain);
            samplePlayer.setSample(outputBuffer);

            DBG("✅ NORMALIZE: Real-time gain applied - no feature modification");
        }
    }

    // ========== EFFECT STACK APPLICATION ==========

    void applyEffectStack()
    {
        if (!effectStateManager.hasOriginalSample())
            return;

        // ✅ Apply ALL effects to data, including reverse
        juce::AudioBuffer<float> processedBuffer;
        effectStateManager.applyAllEffects(processedBuffer);

        if (processedBuffer.getNumSamples() == 0)
            return;

        // Update all dependent systems
        outputBuffer.makeCopyOf(processedBuffer);
        samplePlayer.setSample(outputBuffer);
        samplePlayer.setEffectStateManager(&effectStateManager);

        // Re-extract features from processed audio
        juce::AudioBuffer<float> monoForAnalysis(1, processedBuffer.getNumSamples());
        monoForAnalysis.copyFrom(0, 0, processedBuffer, 0, 0, processedBuffer.getNumSamples());

        featureData = featureExtractor.extractAmplitudeOnly(monoForAnalysis, currentSampleRate);

        DBG("✅ Effect stack applied - " + juce::String(processedBuffer.getNumSamples()) + " samples");
    }

    // getEffectStateManager() defined earlier (line 105)

    void exportIndicesAsync(
        const juce::File& baseFile,
        std::function<void(bool success, juce::String message)> callback)
    {
        juce::AudioBuffer<float> audioCopy;
        FeatureData featuresCopy;
        const SpectralIndexData* overviewIndices = nullptr;

        {
            const juce::ScopedLock sl(sampleLock);

            if (!sampleLoaded || originalSample.getNumSamples() == 0)
            {
                juce::MessageManager::callAsync([callback]()
                    {
                        callback(false, "No sample loaded");
                    });
                return;
            }

            overviewIndices = indexDatabase.getOverviewIndices();
            if (!overviewIndices)
            {
                juce::MessageManager::callAsync([callback]()
                    {
                        callback(false, "No indices available - click 'Analyze Indices' first");
                    });
                return;
            }

            audioCopy.makeCopyOf(originalSample);
            featuresCopy = featureData;

            lastExportedFeaturesHash = calculateFeaturesHash(featureData);
        }

        auto indicesCopy = std::make_shared<SpectralIndexData>(*overviewIndices);
        auto audioPtr = std::make_shared<juce::AudioBuffer<float>>(audioCopy);
        auto featuresPtr = std::make_shared<FeatureData>(featuresCopy);

        // âœ… FIX: Removed the misplaced function declarations from here

        SimpleIndexExporter::exportAllAsync(
            baseFile,
            *audioPtr,
            *featuresPtr,
            *indicesCopy,
            currentSampleRate,
            callback
        );
    }

    bool needsReexport() const
    {
        const juce::ScopedLock sl(sampleLock);

        if (!sampleLoaded || featureData.getNumSamples() == 0)
            return false;

        size_t currentHash = calculateFeaturesHash(featureData);
        return currentHash != lastExportedFeaturesHash;
    }

    UIStateManager& getUIState() { return uiState; }
    const UIStateManager& getUIState() const { return uiState; }

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;


    void loadSample(const juce::File& file);
    void loadSampleFromBuffer(const juce::AudioBuffer<float>& buffer);
    void processSample();
    void applyFeatureChangesToSample();

    void exportModifiedSample(const juce::File& file);
    void exportDifferenceData(const juce::File& file);
    void searchForPatterns();
    void applyPatternToSample(Pattern& pattern, float intensity = 1.0f);

    bool hasSampleLoaded() const { return sampleLoaded; }
    // getOriginalSample() defined earlier (line 65)
    const juce::AudioBuffer<float>& getReconstructedSample() const { return reconstructedBuffer; }
    const juce::AudioBuffer<float>& getDifferenceBuffer() const { return differenceBuffer; }

    SamplePlayer& getSamplePlayer() { return samplePlayer; }
    PatternLibrary& getPatternLibrary() { return patternLibrary; }
    MLEvolutionEngine& getMLEngine() { return mlEngine; }

    const FeatureData& getFeatureData() const { return featureData; }
    FeatureData& getFeatureDataMutable() { return featureData; }
    bool hasFeatureData() const { return featureData.getNumSamples() > 0; }

    // Ã¢Å“â€¦ ÃÂÃÅ¾Ãâ€™ÃÅ¾Ãâ€¢: Lazy Loading ÃÂ¼ÃÂµÃ‘â€šÃÂ¾ÃÂ´Ã‘â€¹
    bool areFrequenciesComputed() const
    {
        return featureData.areFrequenciesComputed();
    }

    bool arePhasesComputed() const
    {
        return featureData.arePhasesComputed();
    }

    bool areVolumesComputed() const
    {
        return featureData.areVolumesComputed();
    }

    bool arePansComputed() const
    {
        return featureData.arePansComputed();
    }

    void computeFrequencies()
    {
        const juce::ScopedLock sl(sampleLock);
        featureExtractor.computeFrequencies(featureData);
    }

    void computePhases()
    {
        const juce::ScopedLock sl(sampleLock);
        featureExtractor.computePhases(featureData);
    }

    void computeVolumes()
    {
        const juce::ScopedLock sl(sampleLock);
        featureExtractor.computeVolumes(featureData);
    }

    void computePans()
    {
        const juce::ScopedLock sl(sampleLock);
        featureExtractor.computePans(featureData);
    }

    SpectralIndexDatabase& getIndexDatabase() { return indexDatabase; }
    const SpectralIndexDatabase& getIndexDatabase() const { return indexDatabase; }

    void analyzeSpectralIndices();

    const SpectralIndexData* getDetailedIndicesForRegion(
        const SpectralIndexData::Region& region,
        IndexResolution resolution = IndexResolution::Maximum)
    {
        return indexDatabase.getDetailedIndices(region, resolution);
    }

    SpectralIndexDatabase::MLIndexExport exportIndicesForML() const
    {
        return indexDatabase.exportAllIndicesForML();
    }

    void modifyIndexAt(int frameIdx, int binIdx, float newMagnitude, float newPhase)
    {
        const juce::ScopedLock sl(sampleLock);
        auto* overviewIndices = const_cast<SpectralIndexData*>(
            indexDatabase.getOverviewIndices());

        if (overviewIndices &&
            frameIdx >= 0 && frameIdx < overviewIndices->getNumFrames() &&
            binIdx >= 0 && binIdx < overviewIndices->getNumBins())
        {
            overviewIndices->modifyIndex(frameIdx, binIdx, newMagnitude, newPhase);
            indicesModified = true;
        }
    }

    void clearModificationsInRegion(const SpectralIndexData::Region& region)
    {
        const juce::ScopedLock sl(sampleLock);
        auto* overviewIndices = const_cast<SpectralIndexData*>(
            indexDatabase.getOverviewIndices());

        if (overviewIndices)
        {
            overviewIndices->clearModificationsInRegion(region);
            indicesModified = false;
        }
    }

    void clearAllModifications()
    {
        const juce::ScopedLock sl(sampleLock);
        auto* overviewIndices = const_cast<SpectralIndexData*>(
            indexDatabase.getOverviewIndices());

        if (overviewIndices)
        {
            overviewIndices->clearAllModifications();
            indicesModified = false;
            outputBuffer.makeCopyOf(originalSample);
            samplePlayer.setSample(outputBuffer);
        }
    }

    struct ModificationStatistics
    {
        int totalModifiedBins = 0;
        int totalModifiedFrames = 0;
        float minModifiedFreq = 0.0f;
        float maxModifiedFreq = 0.0f;
        float minModifiedTime = 0.0f;
        float maxModifiedTime = 0.0f;
    };

    ModificationStatistics getModificationStatistics() const;
    void synthesizeFromModifiedIndices();


    /*
    void applyAlgorithmToSample(const AlgorithmDNA& algo, float intensity = -1.0f)
    {
        const juce::ScopedLock sl(sampleLock);
        if (!sampleLoaded || outputBuffer.getNumSamples() == 0)
            return;

        DBG("ðŸŽ¨ Applying algorithm: " + algo.metadata.name);

        juce::AudioBuffer<float> input;
        input.makeCopyOf(outputBuffer);

        juce::AudioBuffer<float> output;

        // Ð˜ÑÐ¿Ð¾Ð»ÑŒÐ·ÑƒÐµÐ¼ AlgorithmEngine
        AlgorithmEngine engine;
        engine.applyAlgorithm(input, output, algo, intensity);

        // ÐžÐ±Ð½Ð¾Ð²Ð»ÑÐµÐ¼ ÑÑÐ¼Ð¿Ð»
        loadSampleFromBuffer(output);

        DBG("âœ… Algorithm applied successfully");
    }
    */


    bool areIndicesModified() const { return indicesModified; }


    void setFeatureAmplitudeAt(int index, float value);

    void setFeatureFrequencyAt(int index, float value);

    void setFeaturePhaseAt(int index, float value)
    {
        featureData.setPhaseAt(index, value);
    }

    // Sample-level utility operations (used by MainPanel tools)
    void trimSilence(float thresholdDb = -60.0f);
    void normalizeSample(float targetDb = 0.0f);

    void removeFeatureSamples(int startSample, int endSample);

    void markFeaturesAsModified()
    {
        featuresModifiedByUser = true;
    }

    void resetFeaturesModificationFlag()
    {
        featuresModifiedByUser = false;
    }

    bool areFeaturesModified() const
    {
        return featuresModifiedByUser;
    }

    bool areAllIndicesSynced() const;
    void forceFullResync();

    // ========== PATTERN STORAGE ==========

    void storeFoundPatterns(const std::vector<IndexPattern>& patterns)
    {
        const juce::ScopedLock sl(sampleLock);
        storedPatterns = patterns;
        DBG("Ã¢Å“â€¦ Stored " + juce::String(patterns.size()) + " patterns in processor");
    }

    const std::vector<IndexPattern>& getStoredPatterns() const
    {
        return storedPatterns;
    }

    bool hasStoredPatterns() const
    {
        return !storedPatterns.empty();
    }

    void clearStoredPatterns()
    {
        const juce::ScopedLock sl(sampleLock);
        storedPatterns.clear();
    }

    // ========== PARAMETERS ==========

    juce::AudioParameterFloat* scaleParam = nullptr;
    juce::AudioParameterFloat* offsetParam = nullptr;
    juce::AudioParameterFloat* seedParam = nullptr;
    juce::AudioParameterInt* bitDepthParam = nullptr;
    juce::AudioParameterFloat* attackParam = nullptr;
    juce::AudioParameterFloat* decayParam = nullptr;
    juce::AudioParameterFloat* sustainParam = nullptr;
    juce::AudioParameterFloat* releaseParam = nullptr;

    // New tool parameters
    juce::AudioParameterFloat* boostDbParam = nullptr;
    juce::AudioParameterFloat* pitchShiftParam = nullptr;
    juce::AudioParameterFloat* timeStretchParam = nullptr;
    juce::AudioParameterBool* loopActiveParam = nullptr;
    juce::AudioParameterFloat* panParam = nullptr;

    void setPhaseVocoderEnabled(bool enabled)
    {
        usePhaseVocoderSynthesis = enabled;
    }

    bool isPhaseVocoderEnabled() const
    {
        return usePhaseVocoderSynthesis;
    }

    double getSampleRate() const { return currentSampleRate; }
    bool hasSampleRateChanged() const { return sampleRateChanged; }
    void acknowledgeSampleRateChange() { sampleRateChanged = false; }

    // ==========================================================================
    // Ã¢Å“â€¦ PATTERN DELETION WITH AUTO-RESYNC
    // ==========================================================================

    float getSampleStartOffset() const { return sampleStartOffset; }
    float getSamplePlaybackLength() const { return samplePlaybackLength; }

    void setSampleStartOffset(float offset)
    {
        sampleStartOffset = juce::jlimit(0.0f, 1.0f, offset);
        updateSamplePlayerRange();
    }

    void setSamplePlaybackLength(float length)
    {
        samplePlaybackLength = juce::jlimit(0.0f, 1.0f, length);
        updateSamplePlayerRange();
    }

    // âœ… Get actual sample indices for visualization
    int getPlaybackStartSample() const
    {
        if (!sampleLoaded || originalSample.getNumSamples() == 0)
            return 0;
        return static_cast<int>(sampleStartOffset * originalSample.getNumSamples());
    }

    int getPlaybackEndSample() const
    {
        if (!sampleLoaded || originalSample.getNumSamples() == 0)
            return originalSample.getNumSamples();

        int startSample = getPlaybackStartSample();
        int totalSamples = originalSample.getNumSamples();

        // FL Studio style: length is relative to start position
        int availableSamples = totalSamples - startSample;
        int lengthSamples = static_cast<int>(samplePlaybackLength * availableSamples);

        return startSample + juce::jmax(1, lengthSamples); // Ensure at least 1 sample
    }


    bool deletePatternRemoveSamples(int patternId, int indexType)
    {
        const juce::ScopedLock sl(sampleLock);

        // 1Ã¯Â¸ÂÃ¢Æ’Â£ Find the pattern
        auto it = std::find_if(storedPatterns.begin(), storedPatterns.end(),
            [patternId](const IndexPattern& p) { return p.patternId == patternId; });

        if (it == storedPatterns.end())
        {
            DBG("Ã¢ÂÅ’ Pattern #" + juce::String(patternId) + " not found");
            return false;
        }

        const auto& pattern = *it;
        int patternLength = static_cast<int>(pattern.values.size());

        DBG("===========================================");
        DBG("Ã°Å¸â€”â€˜Ã¯Â¸Â DELETING PATTERN (WITH AUTO-RESYNC) #" + juce::String(patternId));
        DBG("===========================================");
        DBG("Occurrences: " + juce::String(pattern.occurrenceCount));
        DBG("Pattern length: " + juce::String(patternLength));
        DBG("Index type: " + juce::String(indexType));

        // 2Ã¯Â¸ÂÃ¢Æ’Â£ Sort positions in DESCENDING order (critical!)
        std::vector<int> sortedPositions = pattern.occurrencePositions;
        std::sort(sortedPositions.begin(), sortedPositions.end(), std::greater<int>());

        DBG("Sorted positions (descending): ");
        for (int pos : sortedPositions)
        {
            DBG("  - " + juce::String(pos));
        }

        // 3Ã¯Â¸ÂÃ¢Æ’Â£ Remove each occurrence (from end to start)
        int successfulRemovals = 0;
        int totalSamplesRemoved = 0;

        for (int position : sortedPositions)
        {
            int startSample = position;
            int endSample = position + patternLength - 1;

            // Validate range
            if (startSample < 0 || endSample >= featureData.getNumSamples())
            {
                DBG("Ã¢Å¡ Ã¯Â¸Â Skipping invalid range: " + juce::String(startSample) +
                    " to " + juce::String(endSample));
                continue;
            }

            DBG("Removing samples " + juce::String(startSample) +
                " to " + juce::String(endSample));

            // Actually remove the samples
            removeFeatureSamples(startSample, endSample);

            successfulRemovals++;
            totalSamplesRemoved += (endSample - startSample + 1);

            DBG("  Ã¢Å“â€¦ Removed " + juce::String(endSample - startSample + 1) + " samples");
            DBG("  New total samples: " + juce::String(featureData.getNumSamples()));
        }

        DBG("-------------------------------------------");
        DBG("Ã¢Å“â€¦ Successfully removed " + juce::String(successfulRemovals) +
            " occurrences");
        DBG("Total samples removed: " + juce::String(totalSamplesRemoved));
        DBG("Final sample count: " + juce::String(featureData.getNumSamples()));

        // 4Ã¯Â¸ÂÃ¢Æ’Â£ Remove pattern from list
        storedPatterns.erase(it);

        // 5Ã¯Â¸ÂÃ¢Æ’Â£ Apply feature changes to audio
        featuresModifiedByUser = true;
        applyFeatureChangesToSample();

        // 6Ã¯Â¸ÂÃ¢Æ’Â£ Auto-resync spectral indices
        DBG("Ã°Å¸â€â€ž Auto-resyncing spectral indices...");
        audioState.forceFullSync(featureExtractor, indexDatabase);

        featuresModifiedByUser = false;
        indicesModified = false;

        DBG("Ã¢Å“â€¦ Spectral indices auto-resynced!");
        DBG("===========================================");
        DBG("Ã¢Å“â€¦ PATTERN DELETED - TIMELINE SHORTENED - ALL INDICES SYNCED");
        DBG("===========================================");

        return true;
    }

    // âœ… ÐÐžÐ’ÐžÐ•: Ð”Ð¾ÑÑ‚ÑƒÐ¿ Ðº AlgorithmFileManager Ð´Ð»Ñ Settings
    AlgorithmFileManager& getAlgorithmFileManager()
    {
        return *algorithmFileManager;
    }

    // âœ… ÐÐžÐ’ÐžÐ•: ÐŸÑ€Ð¸Ð¼ÐµÐ½ÐµÐ½Ð¸Ðµ Ð°Ð»Ð³Ð¾Ñ€Ð¸Ñ‚Ð¼Ð° Ð¸Ð· MainPanel
    void applyAlgorithmToSample(const AlgorithmDNA& algo)
    {
        if (!hasSampleLoaded() || !algo.isValid())
        {
            DBG("âŒ Cannot apply algorithm: no sample or invalid algorithm");
            return;
        }

        const juce::ScopedLock sl(sampleLock);

        DBG("===========================================");
        DBG("ðŸŽ¨ APPLYING ALGORITHM TO SAMPLE");
        DBG("===========================================");
        DBG("Algorithm: " + algo.metadata.name);

        // ÐŸÐ¾Ð»ÑƒÑ‡Ð°ÐµÐ¼ Ñ‚ÐµÐºÑƒÑ‰Ð¸Ð¹ audio
        juce::AudioBuffer<float> input;
        input.makeCopyOf(originalSample);

        // ÐŸÑ€Ð¸Ð¼ÐµÐ½ÑÐµÐ¼ Ð°Ð»Ð³Ð¾Ñ€Ð¸Ñ‚Ð¼
        juce::AudioBuffer<float> output;
        AlgorithmEngine engine;
        engine.applyAlgorithm(input, output, algo);

        // ÐžÐ±Ð½Ð¾Ð²Ð»ÑÐµÐ¼ ÑÑÐ¼Ð¿Ð»
        originalSample.makeCopyOf(output);
        outputBuffer.makeCopyOf(output);
        samplePlayer.setSample(outputBuffer);

        // ÐŸÐµÑ€ÐµÑÑ‡Ð¸Ñ‚Ñ‹Ð²Ð°ÐµÐ¼ features Ð´Ð»Ñ Ð½Ð¾Ð²Ð¾Ð³Ð¾ audio
        resetFeaturesModificationFlag();
        featureExtractor.getPhaseVocoder().invalidateCache();

        juce::AudioBuffer<float> monoForAnalysis(1, output.getNumSamples());
        monoForAnalysis.copyFrom(0, 0, output, 0, 0, output.getNumSamples());

        featureData = featureExtractor.extractAmplitudeOnly(monoForAnalysis, currentSampleRate);

        DBG("âœ… Algorithm applied + features recalculated");
        DBG("===========================================");
    }

private:
    juce::AudioBuffer<float> originalSample;
    juce::AudioBuffer<float> originalSampleBackup;  // ✅ Backup for restoring reverse
    juce::AudioBuffer<float> noiseBuffer;
    juce::AudioBuffer<float> differenceBuffer;
    juce::AudioBuffer<float> reconstructedBuffer;
    juce::AudioBuffer<float> outputBuffer;
    juce::CriticalSection sampleLock;

    std::unique_ptr<AlgorithmFileManager> algorithmFileManager;

    bool sampleRateChanged = false;
    bool indicesModified = false;

    FeatureData previewFeatureData;
    bool isPreviewingRange = false;
    float currentStartPercent = 0.0f;
    float currentLengthPercent = 100.0f;

    NoiseGenerator noiseGenerator;
    DifferenceEngine differenceEngine;
    SamplePlayer samplePlayer;
    PatternDetector patternDetector;
    PatternLibrary patternLibrary;
    MLEvolutionEngine mlEngine;
    FeatureExtractor featureExtractor;
    FeatureData featureData;
    SpectralIndexDatabase indexDatabase;
    AudioStateManager audioState;
    UIStateManager uiState;
    EffectStateManager effectStateManager;

    std::vector<IndexPattern> storedPatterns;
    mutable size_t lastExportedFeaturesHash = 0;

    std::unique_ptr<ProjectManager> projectManager;
    juce::String currentSampleName;

    bool usePhaseVocoderSynthesis = true;
    bool sampleLoaded = false;
    double currentSampleRate = 44100.0;
    bool featuresModifiedByUser = false;

    float sampleStartOffset = 0.0f;      // 0.0 to 1.0
    float samplePlaybackLength = 1.0f;   // 0.0 to 1.0

    void updateSamplePlayerRange()
    {
        if (!sampleLoaded || outputBuffer.getNumSamples() == 0)
            return;

        int startSample = getPlaybackStartSample();
        int endSample = getPlaybackEndSample();
        int totalSamples = originalSample.getNumSamples();

        DBG("ðŸŽ›ï¸ Range update: Start=" + juce::String(startSample) +
            " (" + juce::String(sampleStartOffset * 100, 1) + "%), " +
            "End=" + juce::String(endSample) +
            " (" + juce::String(samplePlaybackLength * 100, 1) + "%), " +
            "Total=" + juce::String(totalSamples));

        // âœ… Tell SamplePlayer to only play this range
        samplePlayer.setPlaybackRange(startSample, endSample);
    }

    size_t calculateFeaturesHash(const FeatureData& features) const
    {
        if (features.getNumSamples() == 0)
            return 0;

        size_t hash = 0;

        for (int i = 0; i < features.getNumSamples(); i += 100)
        {
            hash ^= std::hash<float>{}(features[i].amplitude);
            hash ^= std::hash<float>{}(features[i].frequency);
            hash ^= std::hash<float>{}(features[i].phase);
        }

        return hash;
    }

    void synthesizeFromSpectralIndices(
        const SpectralIndexData& indices,
        juce::AudioBuffer<float>& outputBuffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseBasedSamplerAudioProcessor)
};