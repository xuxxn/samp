/*
PluginProcessor.cpp
Ã ÃÂµÃÂ°ÃÂ»ÃÂ¸ÃÂ·ÃÂ°Ã‘â€ ÃÂ¸Ã‘Â ÃÂ°Ã‘Æ’ÃÂ´ÃÂ¸ÃÂ¾ ÃÂ¿Ã‘â‚¬ÃÂ¾Ã‘â€ ÃÂµÃ‘ÂÃ‘ÂÃÂ¾Ã‘â‚¬ÃÂ° Ã‘Â STEREO ÃÂ¸ ADSR ÃÂ¿ÃÂ¾ÃÂ´ÃÂ´ÃÂµÃ‘â‚¬ÃÂ¶ÃÂºÃÂ¾ÃÂ¹
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
NoiseBasedSamplerAudioProcessor::NoiseBasedSamplerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
{
    auto scaleParameter = std::make_unique<juce::AudioParameterFloat>(
        "scale", "Scale",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f), 1.0f);
    auto offsetParameter = std::make_unique<juce::AudioParameterFloat>(
        "offset", "Offset",
        juce::NormalisableRange<float>(-0.5f, 0.5f, 0.0001f), 0.0f);
    auto seedParameter = std::make_unique<juce::AudioParameterFloat>(
        "seed", "Seed",
        juce::NormalisableRange<float>(1.0f, 99999.0f, 1.0f), 12345.0f);
    auto bitDepthParameter = std::make_unique<juce::AudioParameterInt>(
        "bitdepth", "Bit Depth", 1, 16, 16);
    auto attackParameter = std::make_unique<juce::AudioParameterFloat>(
        "attack", "Attack",
        juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.01f);
    auto decayParameter = std::make_unique<juce::AudioParameterFloat>(
        "decay", "Decay",
        juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f, 0.3f), 0.1f);
    auto sustainParameter = std::make_unique<juce::AudioParameterFloat>(
        "sustain", "Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f);
    auto releaseParameter = std::make_unique<juce::AudioParameterFloat>(
        "release", "Release",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f, 0.3f), 0.3f);
    auto panParameter = std::make_unique<juce::AudioParameterFloat>(
        "pan", "Pan",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f);

    // New tool parameters
    auto boostDbParameter = std::make_unique<juce::AudioParameterFloat>(
        "boostDb", "Boost",
        juce::NormalisableRange<float>(-20.0f, 20.0f, 0.1f), 0.0f);
    auto pitchShiftParameter = std::make_unique<juce::AudioParameterFloat>(
        "pitchShift", "Pitch",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f);
    auto timeStretchParameter = std::make_unique<juce::AudioParameterFloat>(
        "timeStretch", "Stretch",
        juce::NormalisableRange<float>(0.25f, 4.0f, 0.01f), 1.0f);
    auto loopActiveParameter = std::make_unique<juce::AudioParameterBool>(
        "loopActive", "Loop", false);

    scaleParam = scaleParameter.get();
    offsetParam = offsetParameter.get();
    seedParam = seedParameter.get();
    bitDepthParam = bitDepthParameter.get();
    attackParam = attackParameter.get();
    decayParam = decayParameter.get();
    sustainParam = sustainParameter.get();
    releaseParam = releaseParameter.get();
    panParam = panParameter.get();

    // New tool parameters
    boostDbParam = boostDbParameter.get();
    pitchShiftParam = pitchShiftParameter.get();
    timeStretchParam = timeStretchParameter.get();
    loopActiveParam = loopActiveParameter.get();
    addParameter(scaleParameter.release());
    addParameter(offsetParameter.release());
    addParameter(seedParameter.release());
    addParameter(bitDepthParameter.release());
    addParameter(attackParameter.release());
    addParameter(decayParameter.release());
    addParameter(sustainParameter.release());
    addParameter(releaseParameter.release());
    addParameter(panParameter.release());

    // Add new tool parameters
    addParameter(boostDbParameter.release());
    addParameter(pitchShiftParameter.release());
    addParameter(timeStretchParameter.release());
    addParameter(loopActiveParameter.release());

    algorithmFileManager = std::make_unique<AlgorithmFileManager>();

    DBG("✅ Processor initialized with async algorithm loading");

    projectManager = std::make_unique<ProjectManager>(*this);
    DBG("✅ ProjectManager initialized");

    PluginVersion::printVersionInfo();
}
NoiseBasedSamplerAudioProcessor::~NoiseBasedSamplerAudioProcessor()
{

    // ✅ ADD AT THE BEGINNING
    if (projectManager && hasSampleLoaded())
    {
        DBG("💾 Auto-saving project before closing...");
        projectManager->saveCurrentProject();
    }

    // algorithmFileManager = std::make_unique<AlgorithmFileManager>(); - so far leave this line alone
    // Сначала останавливаем любые async операции
    if (algorithmFileManager)
    {
        algorithmFileManager.reset();
    }
}
const juce::String NoiseBasedSamplerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}
bool NoiseBasedSamplerAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}
bool NoiseBasedSamplerAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}
bool NoiseBasedSamplerAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}
double NoiseBasedSamplerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}
int NoiseBasedSamplerAudioProcessor::getNumPrograms()
{
    return 1;
}
int NoiseBasedSamplerAudioProcessor::getCurrentProgram()
{
    return 0;
}
void NoiseBasedSamplerAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}
const juce::String NoiseBasedSamplerAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}
void NoiseBasedSamplerAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}
void NoiseBasedSamplerAudioProcessor::prepareToPlay(
    double sampleRate,
    int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    const int numOutputChannels = getTotalNumOutputChannels();

    // ============================
    // SAMPLE RATE CHANGE HANDLING
    // ============================
    if (std::abs(currentSampleRate - sampleRate) > 0.1)
    {
        DBG("===========================================");
        DBG("SAMPLE RATE CHANGED");
        DBG("===========================================");
        DBG("Old: " + juce::String(currentSampleRate, 0) + " Hz");
        DBG("New: " + juce::String(sampleRate, 0) + " Hz");

        currentSampleRate = sampleRate;

        if (sampleLoaded && originalSample.getNumSamples() > 0)
        {
            const juce::ScopedLock sl(sampleLock);

            DBG("Re-analyzing sample for new sample rate...");

            indexDatabase.clearCache();
            featureExtractor.getPhaseVocoder().invalidateCache();
            resetFeaturesModificationFlag();

            featureData = featureExtractor.extractFeatures(
                originalSample,
                currentSampleRate
            );

            processSample();
            analyzeSpectralIndices();

            DBG("Ã¢Å“â€¦ Sample re-analyzed for new sample rate");
            sampleRateChanged = true;
        }
        else
        {
            DBG("No sample loaded - only updating sample rate");
        }
    }
    else
    {
        currentSampleRate = sampleRate;
    }

    // ============================
    // Ã°Å¸â€Â¥ SAMPLE PLAYER PREPARE
    // ============================
    samplePlayer.prepare(
        numOutputChannels,     // Ã°Å¸â€Â¥ ÃÅ¡Ã ÃËœÃÂ¢ÃËœÃÂ§ÃÂÃÅ¾: ÃÂÃâ€¢ 1
        sampleRate,
        samplesPerBlock
    );

    samplePlayer.setSampleRate(sampleRate);

    samplePlayer.setADSRParameters({
        attackParam->get(),
        decayParam->get(),
        sustainParam->get(),
        releaseParam->get()
        });

    samplePlayer.setPan(panParam->get());

    samplePlayer.setInterpolationMode(
        SamplePlayer::InterpolationMode::Cubic
    );
}

void NoiseBasedSamplerAudioProcessor::releaseResources()
{
    const juce::ScopedLock sl(sampleLock);
    samplePlayer.allNotesOff();
    DBG("Audio processor released");
}
#ifndef JucePlugin_PreferredChannelConfigurations
bool NoiseBasedSamplerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
#endif
}
#endif
void NoiseBasedSamplerAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const juce::ScopedLock sl(sampleLock);

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Ã°Å¸â€Â¥ Ãâ€™ÃÂÃâ€“ÃÂÃÅ¾: Ã‘â€¡ÃÂ¸Ã‘ÂÃ‘â€šÃÂ¸ÃÂ¼, ÃÂ½ÃÂ¾ ÃÂ´ÃÂ°ÃÂ»ÃÂµÃÂµ SamplePlayer ÃÂ¾ÃÂ±Ã‘ÂÃÂ·ÃÂ°ÃÂ½ ÃÂ·ÃÂ°ÃÂ¿ÃÂ¾ÃÂ»ÃÂ½ÃÂ¸Ã‘â€šÃ‘Å’ Ãâ€™ÃÂ¡Ãâ€¢ ÃÂºÃÂ°ÃÂ½ÃÂ°ÃÂ»Ã‘â€¹
    buffer.clear();

    // Envelope parameters
    samplePlayer.setADSRParameters({
        attackParam->get(),
        decayParam->get(),
        sustainParam->get(),
        releaseParam->get()
        });

    // Pan parameter for SamplePlayer
    samplePlayer.setPan(panParam->get());

    // MIDI (ÃÂ±ÃÂµÃÂ· ÃÂ¸ÃÂ·ÃÂ¼ÃÂµÃÂ½ÃÂµÃÂ½ÃÂ¸ÃÂ¹)
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            samplePlayer.noteOn(
                message.getNoteNumber(),
                message.getFloatVelocity()
            );
        }
        else if (message.isNoteOff())
        {
            samplePlayer.noteOff(message.getNoteNumber());
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            samplePlayer.allNotesOff();
        }
    }

    // Ã°Å¸â€Â¥ ÃÅ¡Ãâ€ºÃÂ®ÃÂ§Ãâ€¢Ãâ€™ÃÅ¾: SamplePlayer Ã Ãâ€¢ÃÂÃâ€Ãâ€¢Ã ÃËœÃÂ¢ STEREO
    samplePlayer.renderNextBlock(buffer, 0, numSamples);

    // DEBUG: Check if SamplePlayer is producing audio
    static bool audioCheckDone = false;
    if (!audioCheckDone)
    {
        float rms = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            rms += buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
        }
        rms /= buffer.getNumChannels();
        DBG("🎵 SamplePlayer RMS: " + juce::String(rms, 6));
        audioCheckDone = true;
    }

    // 🔊 APPLY REAL-TIME EFFECTS FROM CMD TOOLS
    applyRealtimeEffects(buffer);

    // DEBUG: Check final output
    static bool outputCheckDone = false;
    if (!outputCheckDone)
    {
        float rms = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            rms += buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
        }
        rms /= buffer.getNumChannels();
        DBG("🔊 Final Output RMS: " + juce::String(rms, 6));
        outputCheckDone = true;
    }

    // DEBUG: Log parameter values
    static int debugCounter = 0;
    if (debugCounter++ % 1000 == 0) // Log every 1000th buffer
    {
        DBG("🔊 DEBUG - Boost: " + juce::String(getBoostDb(), 1) + "dB" +
            " | Pitch: " + juce::String(getPitchShift(), 1) + "st" +
            " | Stretch: " + juce::String(getTimeStretch(), 2) + "x" +
            " | Loop: " + juce::String(isLoopActive() ? "ON" : "OFF"));
    }
}

bool NoiseBasedSamplerAudioProcessor::hasEditor() const
{
    return true;
}
juce::AudioProcessorEditor* NoiseBasedSamplerAudioProcessor::createEditor()
{
    return new NoiseBasedSamplerAudioProcessorEditor(*this);
}
void NoiseBasedSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);

    // ========== СУЩЕСТВУЮЩИЕ ПАРАМЕТРЫ (как было) ==========
    stream.writeFloat(scaleParam->get());
    stream.writeFloat(offsetParam->get());
    stream.writeFloat(seedParam->get());
    stream.writeInt(bitDepthParam->get());
    stream.writeFloat(attackParam->get());
    stream.writeFloat(decayParam->get());
    stream.writeFloat(sustainParam->get());
    stream.writeFloat(releaseParam->get());
    stream.writeFloat(panParam->get());

    // ========== СУЩЕСТВУЮЩИЕ ПАТТЕРНЫ (как было) ==========
    const juce::ScopedLock sl(sampleLock);
    stream.writeInt(static_cast<int>(storedPatterns.size()));

    for (const auto& pattern : storedPatterns)
    {
        stream.writeInt(pattern.patternId);
        stream.writeInt(pattern.occurrenceCount);
        stream.writeFloat(pattern.averageValue);
        stream.writeFloat(pattern.variance);

        stream.writeInt(static_cast<int>(pattern.values.size()));
        for (float val : pattern.values)
            stream.writeFloat(val);

        stream.writeInt(static_cast<int>(pattern.occurrencePositions.size()));
        for (int pos : pattern.occurrencePositions)
            stream.writeInt(pos);
    }

    // ========== ✅ НОВОЕ: UI STATE (через XML) ==========
    auto uiStateXml = std::unique_ptr<juce::XmlElement>(uiState.toXml());
    juce::String xmlString = uiStateXml->toString();

    // Сохраняем длину строки и саму строку
    stream.writeInt(xmlString.length());
    stream.writeString(xmlString);

    DBG("💾 State saved: " + juce::String(storedPatterns.size()) + " patterns + UI state + effects");
}
void NoiseBasedSamplerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);

    // ========== СУЩЕСТВУЮЩИЕ ПАРАМЕТРЫ (как было) ==========
    *scaleParam = stream.readFloat();
    *offsetParam = stream.readFloat();
    *seedParam = stream.readFloat();
    *bitDepthParam = stream.readInt();

    if (stream.getPosition() < stream.getTotalLength())
    {
        *attackParam = stream.readFloat();
        *decayParam = stream.readFloat();
        *sustainParam = stream.readFloat();
        *releaseParam = stream.readFloat();
        *panParam = stream.readFloat();
    }

    // ========== СУЩЕСТВУЮЩИЕ ПАТТЕРНЫ (как было) ==========
    if (stream.getPosition() < stream.getTotalLength())
    {
        const juce::ScopedLock sl(sampleLock);
        storedPatterns.clear();

        int numPatterns = stream.readInt();

        for (int i = 0; i < numPatterns; ++i)
        {
            if (stream.isExhausted())
                break;

            IndexPattern pattern;
            pattern.patternId = stream.readInt();
            pattern.occurrenceCount = stream.readInt();
            pattern.averageValue = stream.readFloat();
            pattern.variance = stream.readFloat();

            int numValues = stream.readInt();
            pattern.values.reserve(numValues);
            for (int j = 0; j < numValues; ++j)
                pattern.values.push_back(stream.readFloat());

            int numPositions = stream.readInt();
            pattern.occurrencePositions.reserve(numPositions);
            for (int j = 0; j < numPositions; ++j)
                pattern.occurrencePositions.push_back(stream.readInt());

            storedPatterns.push_back(pattern);
        }

        DBG("📥 State loaded: " + juce::String(storedPatterns.size()) + " patterns");
    }

    // ========== ✅ НОВОЕ: UI STATE ==========
    if (stream.getPosition() < stream.getTotalLength())
    {
        int xmlLength = stream.readInt();

        if (xmlLength > 0 && xmlLength < 1000000) // Санити-чек
        {
            juce::String xmlString = stream.readString();

            auto uiStateXml = juce::XmlDocument::parse(xmlString);

            if (uiStateXml != nullptr)
            {
                uiState.fromXml(uiStateXml.get());
                DBG("✅ UI State restored from save");
            }
        }
    }
}

void NoiseBasedSamplerAudioProcessor::loadSample(const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader != nullptr)
    {
        const juce::ScopedLock sl(sampleLock);

        int numChannels = static_cast<int>(reader->numChannels);
        int numSamples = static_cast<int>(reader->lengthInSamples);

        DBG("===========================================");
        DBG("LOADING SAMPLE (LAZY MODE)");
        DBG("===========================================");
        DBG("File: " + file.getFileName());
        DBG("Channels: " + juce::String(numChannels));
        DBG("Samples: " + juce::String(numSamples));

        // âœ… Ð’ÑÐµÐ³Ð´Ð° Ð·Ð°Ð³Ñ€ÑƒÐ¶Ð°ÐµÐ¼ Ð² STEREO
        juce::AudioBuffer<float> loadedBuffer(2, numSamples);

        if (numChannels == 1)
        {
            juce::AudioBuffer<float> tempMono(1, numSamples);
            reader->read(&tempMono, 0, numSamples, 0, true, false);
            loadedBuffer.copyFrom(0, 0, tempMono, 0, 0, numSamples);
            loadedBuffer.copyFrom(1, 0, tempMono, 0, 0, numSamples);
            DBG("  Converted MONO â†’ STEREO");
        }
        else if (numChannels >= 2)
        {
            reader->read(&loadedBuffer, 0, numSamples, 0, true, true);
            DBG("  Loaded as STEREO");
        }

        // âœ… Ð˜Ð½Ð¸Ñ†Ð¸Ð°Ð»Ð¸Ð·Ð¸Ñ€ÑƒÐµÐ¼ AudioStateManager
        audioState.loadSample(
            loadedBuffer,
            currentSampleRate,
            featureExtractor,
            indexDatabase
        );

        auto groundTruth = audioState.getGroundTruthAudio();
        DBG("  Ground truth channels: " + juce::String(groundTruth.getNumChannels()));

        juce::AudioBuffer<float> monoForAnalysis(1, groundTruth.getNumSamples());
        monoForAnalysis.copyFrom(0, 0, groundTruth, 0, 0, groundTruth.getNumSamples());

        // âœ… ÐšÐ Ð˜Ð¢Ð˜Ð§ÐÐž: Ð˜ÑÐ¿Ð¾Ð»ÑŒÐ·ÑƒÐµÐ¼ Ð‘Ð«Ð¡Ð¢Ð Ð«Ð™ extractAmplitudeOnly Ð²Ð¼ÐµÑÑ‚Ð¾ extractFeatures!
        DBG("ðŸš€ Starting FAST feature extraction (Amplitude only)...");

        auto startTime = juce::Time::getMillisecondCounterHiRes();

        featureData = featureExtractor.extractAmplitudeOnly(monoForAnalysis, currentSampleRate);

        auto endTime = juce::Time::getMillisecondCounterHiRes();
        double elapsed = endTime - startTime;

        DBG("âœ… FAST extraction complete in " + juce::String(elapsed, 2) + " ms");
        DBG("   (Other indices will compute on-demand)");

        // ÐžÐ±Ð½Ð¾Ð²Ð»ÑÐµÐ¼ Ð±ÑƒÑ„ÐµÑ€Ñ‹
        originalSample.makeCopyOf(groundTruth);
        outputBuffer.makeCopyOf(groundTruth);

        sampleLoaded = true;

        // Store original for effect system
        effectStateManager.setOriginalSample(originalSample);

        resetFeaturesModificationFlag();
        featureExtractor.getPhaseVocoder().invalidateCache();

        processSample();

        samplePlayer.setSample(outputBuffer);
        samplePlayer.setEffectStateManager(&effectStateManager);

        // ✅ Reset start/length to default when new sample loaded
        setSampleStartOffset(0.0f);   // 0% = no offset
        setSamplePlaybackLength(1.0f); // 100% = full length

        DBG("===========================================");
        DBG("âœ… SAMPLE LOADED (LAZY MODE - INSTANT!)");
        DBG("===========================================");

        // ✅ AUTO-APPLY ACTIVE EFFECTS TO NEW SAMPLE
        if (effectStateManager.isTrimActive() ||
            effectStateManager.isNormalizeActive() ||
            effectStateManager.isReverseActive() ||
            effectStateManager.isBoostActive())
        {
            DBG("🔄 Auto-applying active effects to new sample...");
            applyEffectStack();
            DBG("✅ Active effects applied!");
        }

    }
    if (projectManager) projectManager->markDirty();
}

void NoiseBasedSamplerAudioProcessor::setFeatureAmplitudeAt(int index, float value) {
    featureData.setAmplitudeAt(index, value);
    if (projectManager) projectManager->markDirty();
}

void NoiseBasedSamplerAudioProcessor::setFeatureFrequencyAt(int index, float value) {
    featureData.setFrequencyAt(index, value);
    if (projectManager) projectManager->markDirty();
}

/*
void applyEffectStack()
{
    if (!effectStateManager.hasOriginalSample())
        return;

    // âœ… Start with ORIGINAL for TRIM, but apply other effects to current state
    juce::AudioBuffer<float> processedBuffer;

    if (effectStateManager.isTrimActive())
    {
        // TRIM needs original sample for boundaries
        effectStateManager.applyAllEffects(processedBuffer);
    }
    else
    {
        // Other effects work with current state
        processedBuffer.makeCopyOf(outputBuffer);

        // Apply REVERSE first (if active)
        if (effectStateManager.isReverseActive())
        {
            effectStateManager.applyReverse(processedBuffer);
        }

        // Then NORMALIZE
        effectStateManager.applyNormalize(processedBuffer);
    }

    if (processedBuffer.getNumSamples() == 0)
        return;

// Update all dependent systems
    outputBuffer.makeCopyOf(processedBuffer);
    originalSample.makeCopyOf(processedBuffer);  // ✅ Also update originalSample for visualization
    effectStateManager.setOriginalSample(originalSample);  // ✅ Keep EffectStateManager in sync
    samplePlayer.setSample(outputBuffer);
    samplePlayer.setEffectStateManager(&effectStateManager);

    // Re-extract features from processed audio
    juce::AudioBuffer<float> monoForAnalysis(1, processedBuffer.getNumSamples());
    monoForAnalysis.copyFrom(0, 0, processedBuffer, 0, 0, processedBuffer.getNumSamples());

    featureData = featureExtractor.extractAmplitudeOnly(monoForAnalysis, currentSampleRate);

    // ✅ Mark features as modified to prevent processSample() overwriting
    featuresModifiedByUser = true;

    DBG("🔍 REVERSE: Updated featureData from processedBuffer, featuresModifiedByUser = true");

    DBG("âœ… Effect stack applied - " + juce::String(processedBuffer.getNumSamples()) + " samples");
}
*/
void NoiseBasedSamplerAudioProcessor::loadSampleFromBuffer(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() == 0)
        return;

    const juce::ScopedLock sl(sampleLock);

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    // Ã¢Å“â€¦ Ãâ€™Ã‘ÂÃÂµÃÂ³ÃÂ´ÃÂ° ÃÂ·ÃÂ°ÃÂ³Ã‘â‚¬Ã‘Æ’ÃÂ¶ÃÂ°ÃÂµÃÂ¼ ÃÂ² stereo
    originalSample.setSize(2, numSamples, false, true, false);

    if (numChannels == 1)
    {
        // Mono Ã¢â€ â€™ copy to both channels
        originalSample.copyFrom(0, 0, buffer, 0, 0, numSamples);
        originalSample.copyFrom(1, 0, buffer, 0, 0, numSamples);
        // Store original for effect system
        effectStateManager.setOriginalSample(originalSample);
    }
    else if (numChannels >= 2)
    {
        // Stereo Ã¢â€ â€™ copy both channels
        originalSample.copyFrom(0, 0, buffer, 0, 0, numSamples);
        originalSample.copyFrom(1, 0, buffer, 1, 0, numSamples);
        // Store original for effect system
        effectStateManager.setOriginalSample(originalSample);
    }

    // ✅ Reset ALL effect states before setting new sample
    effectStateManager.setReverseActive(false);
    effectStateManager.setTrimActive(false);
    effectStateManager.setNormalizeActive(false);

    // ✅ Set original sample in effect manager FIRST
    effectStateManager.setOriginalSample(originalSample);

    // ✅ Create backup for reverse restoration AFTER setting original
    originalSampleBackup.makeCopyOf(originalSample);

    sampleLoaded = true;
    resetFeaturesModificationFlag();
    featureExtractor.getPhaseVocoder().invalidateCache();

    processSample();
    analyzeSpectralIndices();

    DBG("Sample loaded from buffer in STEREO: " + juce::String(numSamples) + " samples");
}

// ============================================================================
// ÃËœÃÂ¡ÃÅ¸Ã ÃÂÃâ€™Ãâ€ºÃâ€¢ÃÂÃËœÃâ€¢ #3: processSample() - ÃÂ¾ÃÂ±Ã‘â‚¬ÃÂ°ÃÂ±ÃÂ¾Ã‘â€šÃÂºÃÂ° ÃÂ¾ÃÂ±ÃÂ¾ÃÂ¸Ã‘â€¦ ÃÂºÃÂ°ÃÂ½ÃÂ°ÃÂ»ÃÂ¾ÃÂ²
// ============================================================================

void NoiseBasedSamplerAudioProcessor::processSample()
{
    if (!sampleLoaded || originalSample.getNumSamples() == 0)
        return;

    int numSamples = originalSample.getNumSamples();
    int numChannels = originalSample.getNumChannels();  // Ã¢Å“â€¦ ÃÂ¢ÃÂµÃÂ¿ÃÂµÃ‘â‚¬Ã‘Å’ = 2!

    DBG("Processing sample: " + juce::String(numChannels) + " channels, " +
        juce::String(numSamples) + " samples");

    // Ã¢Å“â€¦ Feature extraction ÃÂ½ÃÂ° LEFT ÃÂºÃÂ°ÃÂ½ÃÂ°ÃÂ»ÃÂµ (ÃÂ´ÃÂ»Ã‘Â ÃÂ²ÃÂ¸ÃÂ·Ã‘Æ’ÃÂ°ÃÂ»ÃÂ¸ÃÂ·ÃÂ°Ã‘â€ ÃÂ¸ÃÂ¸)
    if (!featuresModifiedByUser)
    {
        DBG("🔍 processSample(): Extracting features from originalSample (featuresModifiedByUser = false)");
        // ÃÂ§ÃÂ¾ÃÂ·ÃÂ´ÃÂ°Ã‘â€˜ÃÂ¼ ÃÂ²Ã‘â‚¬ÃÂµÃÂ¼ÃÂµÃÂ½ÃÂ½Ã‘â€¹ÃÂ¹ ÃÂ¼ÃÂ¾ÃÂ½ÃÂ¾ ÃÂ±Ã‘Æ’Ã‘â€žÃÂµÃ‘â‚¬ ÃÂ¸ÃÂ· ÃÂ»ÃÂµÃÂ²ÃÂ¾ÃÂ³ÃÂ¾ ÃÂºÃÂ°ÃÂ½ÃÂ°ÃÂ»ÃÂ°
        juce::AudioBuffer<float> monoForAnalysis(1, numSamples);
        monoForAnalysis.copyFrom(0, 0, originalSample, 0, 0, numSamples);

        featureData = featureExtractor.extractFeatures(monoForAnalysis, currentSampleRate);

        auto stats = featureData.calculateStatistics();
        DBG("Feature Stats (Left channel):");
        DBG("  Amplitude: " + juce::String(stats.minAmplitude, 3) + " to " +
            juce::String(stats.maxAmplitude, 3));
    }

    // Ã¢Å“â€¦ Noise generation - STEREO
    noiseBuffer.setSize(2, numSamples, false, true, false);  // 2 channels!
    noiseGenerator.setSeed(static_cast<uint64_t>(seedParam->get()));
    noiseGenerator.generateNoise(noiseBuffer);

    // Ã¢Å“â€¦ Difference calculation - STEREO
    differenceEngine.calculateDifference(originalSample, noiseBuffer, differenceBuffer);

    // Ã¢Å“â€¦ Reconstruction - STEREO
    float scale = scaleParam->get();
    float offset = offsetParam->get();
    int bitDepth = bitDepthParam->get();

    differenceEngine.reconstruct(noiseBuffer, differenceBuffer, reconstructedBuffer,
        scale, offset, bitDepth);

    // Ã¢Å“â€¦ Output ÃÂ´ÃÂ»Ã‘Â playback - STEREO
    outputBuffer.setSize(2, numSamples, false, true, false);
    outputBuffer.makeCopyOf(reconstructedBuffer);

    samplePlayer.setSample(outputBuffer);

    DBG("Ã¢Å“â€¦ Sample processed in STEREO");
}
// latest version of applyFeatureChangestoSample ykwim
void NoiseBasedSamplerAudioProcessor::applyFeatureChangesToSample()
{
    if (featureData.getNumSamples() == 0)
        return;

    const juce::ScopedLock sl(sampleLock);

    DBG("===========================================");
    DBG("ðŸŽµ APPLYING FEATURE CHANGES");
    DBG("===========================================");

    // âœ… ÐšÐ›Ð®Ð§Ð•Ð’ÐžÐ•: Ð˜ÑÐ¿Ð¾Ð»ÑŒÐ·ÑƒÐµÐ¼ AudioStateManager ÐºÐ¾Ñ‚Ð¾Ñ€Ñ‹Ð¹ ÑÐ¾Ñ…Ñ€Ð°Ð½Ð¸Ñ‚ ÑÑ‚ÐµÑ€ÐµÐ¾
    audioState.applyFeatureChanges(
        featureData,
        currentSampleRate,
        indexDatabase,
        true
    );

    // ÐŸÐ¾Ð»ÑƒÑ‡Ð°ÐµÐ¼ Ñ€ÐµÐ·ÑƒÐ»ÑŒÑ‚Ð°Ñ‚
    auto groundTruth = audioState.getGroundTruthAudio();

    // âœ… ÐŸÐ ÐžÐ’Ð•Ð ÐšÐ: Ground truth Ð´Ð¾Ð»Ð¶ÐµÐ½ Ð±Ñ‹Ñ‚ÑŒ Ð² STEREO
    if (groundTruth.getNumChannels() < 2)
    {
        DBG("âŒ ERROR: Ground truth is not STEREO!");
        return;
    }

    // ÐžÐ±Ð½Ð¾Ð²Ð»ÑÐµÐ¼ Ð±ÑƒÑ„ÐµÑ€Ñ‹
    outputBuffer.makeCopyOf(groundTruth);
    originalSample.makeCopyOf(groundTruth);

    // ✅ КОНСТАНТНЫЕ ЭФФЕКТЫ: Применяем только TRIM
    if (effectStateManager.isTrimActive())
    {
        DBG("🔧 Applying trim effect...");

        // Устанавливаем оригинальный сэмпл для EffectStateManager
        effectStateManager.setOriginalSample(originalSample);

        // Применяем только trim (без normalize!)
        juce::AudioBuffer<float> processedBuffer;
        effectStateManager.applyAllEffects(processedBuffer);

        // Обновляем все буферы обработанным результатом
        outputBuffer.makeCopyOf(processedBuffer);
        originalSample.makeCopyOf(processedBuffer);

        DBG("✅ Trim applied");
    }

    // ✅ REAL-TIME NORMALIZE: Пересчитываем и применяем ПОСЛЕ всех изменений
    if (effectStateManager.isNormalizeActive())
    {
        // ✅ Пересчитываем gain из CURRENT output buffer
        float peak = 0.0f;
        const int numChannels = outputBuffer.getNumChannels();
        const int numSamples = outputBuffer.getNumSamples();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* data = outputBuffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                peak = juce::jmax(peak, std::abs(data[i]));
        }

        if (peak > 0.0f)
        {
            const float targetLin = std::pow(10.0f, 0.0f / 20.0f); // 0 dB
            const float gain = targetLin / peak;

            // Обновляем gain в EffectStateManager
            effectStateManager.setNormalizeActive(true, 0.0f, gain);

            DBG("🎛️ Real-time normalize: peak=" + juce::String(peak, 4) + " gain=" + juce::String(gain, 4));

            // Применяем gain
            outputBuffer.applyGain(gain);

            DBG("✅ Real-time normalize applied");
        }
    }

    // ✅ Всегда обновляем player
    samplePlayer.allNotesOff();
    samplePlayer.setSample(outputBuffer);

    resetFeaturesModificationFlag();

    // Ð’ÐµÑ€Ð¸Ñ„Ð¸ÐºÐ°Ñ†Ð¸Ñ
    const float* left = outputBuffer.getReadPointer(0);
    const float* right = outputBuffer.getReadPointer(1);

    int stereoSamples = 0;
    for (int i = 0; i < outputBuffer.getNumSamples(); ++i)
    {
        if (std::abs(left[i] - right[i]) > 0.0001f)
            stereoSamples++;
    }

    float stereoPercent = (stereoSamples * 100.0f) / outputBuffer.getNumSamples();

    DBG("âœ… Features applied!");
    DBG("   Channels: " + juce::String(outputBuffer.getNumChannels()));
    DBG("   Stereo content: " + juce::String(stereoPercent, 1) + "%");
    DBG("===========================================");
}

/*
void NoiseBasedSamplerAudioProcessor::applyFeatureChangesToSample()
{
    if (featureData.getNumSamples() == 0)
        return;

    const juce::ScopedLock sl(sampleLock);

    DBG("===========================================");
    DBG("Ã°Å¸Å½Âµ APPLYING FEATURE CHANGES");
    DBG("===========================================");

    int numSamples = featureData.getNumSamples();

    // STEP 1: ÃÂ¡ÃÂ¸ÃÂ½Ã‘â€šÃÂµÃÂ·ÃÂ¸Ã‘â‚¬Ã‘Æ’ÃÂµÃÂ¼ ÃÂ¸ÃÂ· features
    juce::AudioBuffer<float> synthesizedStereo(2, numSamples);
    featureData.applyToAudioBuffer(synthesizedStereo, currentSampleRate);

    DBG("Ã¢Å“â€¦ Step 1: Features Ã¢â€ â€™ Audio synthesis complete");

    // STEP 2: ÃÅ¾ÃÂ±ÃÂ½ÃÂ¾ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼ originalSample (ÃÂ´ÃÂ»Ã‘Â Ã‘ÂÃÂ¾Ã‘â€¦Ã‘â‚¬ÃÂ°ÃÂ½ÃÂµÃÂ½ÃÂ¸Ã‘Â)
    originalSample.setSize(2, numSamples, false, true, false);
    originalSample.copyFrom(0, 0, synthesizedStereo, 0, 0, numSamples);
    originalSample.copyFrom(1, 0, synthesizedStereo, 1, 0, numSamples);

    DBG("Ã¢Å“â€¦ Step 2: Updated originalSample");

    // STEP 3: Ã¢Å“â€¦ ÃÅ¡Ã ÃËœÃÂ¢ÃËœÃÂ§ÃÂÃÅ¾ - ÃÅ¾ÃÂ±ÃÂ½ÃÂ¾ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼ outputBuffer
    outputBuffer.setSize(2, numSamples, false, true, false);
    outputBuffer.makeCopyOf(synthesizedStereo);

    DBG("Ã¢Å“â€¦ Step 3: Updated outputBuffer (ground truth)");

    // STEP 4: ÃÅ¾ÃÂ±ÃÂ½ÃÂ¾ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼ player
    samplePlayer.setSample(outputBuffer);

    DBG("Ã¢Å“â€¦ Step 4: Player updated");

    // STEP 5: Ã¢Å“â€¦ ÃÂÃÅ¾Ãâ€™ÃÅ¾Ãâ€¢ - ÃËœÃÂ½ÃÂ²ÃÂ°ÃÂ»ÃÂ¸ÃÂ´ÃÂ¸Ã‘â‚¬Ã‘Æ’ÃÂµÃÂ¼ spectral cache
    // ÃÂ¢ÃÂµÃÂ¿ÃÂµÃ‘â‚¬Ã‘Å’ spectral indices Ã‘Æ’Ã‘ÂÃ‘â€šÃÂ°Ã‘â‚¬ÃÂµÃÂ»ÃÂ¸, Ã‘â€š.ÃÂº. ÃÂ°Ã‘Æ’ÃÂ´ÃÂ¸ÃÂ¾ ÃÂ¸ÃÂ·ÃÂ¼ÃÂµÃÂ½ÃÂ¸ÃÂ»ÃÂ¾Ã‘ÂÃ‘Å’
    // ÃÅ¸Ã‘â‚¬ÃÂ¸ Ã‘ÂÃÂ»ÃÂµÃÂ´Ã‘Æ’Ã‘Å½Ã‘â€°ÃÂµÃÂ¼ "Analyze Indices" ÃÂ±Ã‘Æ’ÃÂ´ÃÂµÃ‘â€š Ã‘ÂÃÂ¾ÃÂ·ÃÂ´ÃÂ°ÃÂ½ÃÂ° ÃÂ½ÃÂ¾ÃÂ²ÃÂ°Ã‘Â ÃÂ±ÃÂ°ÃÂ·ÃÂ°

    // ÃÂÃâ€¢ Ãâ€™ÃÂ«Ãâ€”ÃÂ«Ãâ€™ÃÂÃâ€¢ÃÅ“ analyzeSpectralIndices() ÃÂ°ÃÂ²Ã‘â€šÃÂ¾ÃÂ¼ÃÂ°Ã‘â€šÃÂ¸Ã‘â€¡ÃÂµÃ‘ÂÃÂºÃÂ¸!
    // ÃÅ¸ÃÂ¾ÃÂ»Ã‘Å’ÃÂ·ÃÂ¾ÃÂ²ÃÂ°Ã‘â€šÃÂµÃÂ»Ã‘Å’ Ã‘ÂÃÂ°ÃÂ¼ Ã‘â‚¬ÃÂµÃ‘Ë†ÃÂ¸Ã‘â€š ÃÂºÃÂ¾ÃÂ³ÃÂ´ÃÂ° ÃÂ¿ÃÂµÃ‘â‚¬ÃÂµ-ÃÂ°ÃÂ½ÃÂ°ÃÂ»ÃÂ¸ÃÂ·ÃÂ¸Ã‘â‚¬ÃÂ¾ÃÂ²ÃÂ°Ã‘â€šÃ‘Å’

    DBG("Ã¢Å¡ Ã¯Â¸Â Spectral indices cache is now STALE");
    DBG("   Click 'Analyze Indices' to re-analyze");

    DBG("===========================================");
    DBG("Ã¢Å“â€¦ FEATURE CHANGES APPLIED!");
    DBG("===========================================");

    resetFeaturesModificationFlag();
}
*/

void NoiseBasedSamplerAudioProcessor::exportModifiedSample(const juce::File& file)
{
    // Ã¢Å“â€¦ ÃËœÃÂ¡ÃÅ¸Ã ÃÂÃâ€™Ãâ€ºÃâ€¢ÃÂÃËœÃâ€¢: ÃÂ­ÃÂºÃ‘ÂÃÂ¿ÃÂ¾Ã‘â‚¬Ã‘â€šÃÂ¸Ã‘â‚¬Ã‘Æ’ÃÂµÃÂ¼ outputBuffer (ÃÂ¸ÃÂ»ÃÂ¸ originalSample)
    if (!sampleLoaded || outputBuffer.getNumSamples() == 0)
        return;

    juce::WavAudioFormat wavFormat;

    if (auto fileStream = file.createOutputStream())
    {
        if (auto writer = wavFormat.createWriterFor(
            fileStream.release(),
            currentSampleRate,
            outputBuffer.getNumChannels(),  // Ã¢Å“â€¦ outputBuffer
            32,
            {},
            0))
        {
            writer->writeFromAudioSampleBuffer(outputBuffer, 0,  // Ã¢Å“â€¦ outputBuffer
                outputBuffer.getNumSamples());
            delete writer;
            DBG("Exported modified sample to: " + file.getFullPathName());
        }
    }
}
void NoiseBasedSamplerAudioProcessor::exportDifferenceData(const juce::File& file)
{
    if (!sampleLoaded || differenceBuffer.getNumSamples() == 0)
        return;
    auto stats = differenceEngine.calculateStatistics(differenceBuffer);
    juce::DynamicObject::Ptr jsonData = new juce::DynamicObject();
    jsonData->setProperty("version", "1.0");
    jsonData->setProperty("seed", static_cast<int>(noiseGenerator.getSeed()));
    jsonData->setProperty("length", differenceBuffer.getNumSamples());
    jsonData->setProperty("sampleRate", currentSampleRate);
    juce::DynamicObject::Ptr params = new juce::DynamicObject();
    params->setProperty("scale", scaleParam->get());
    params->setProperty("offset", offsetParam->get());
    params->setProperty("bitDepth", bitDepthParam->get());
    jsonData->setProperty("parameters", params.get());
    juce::DynamicObject::Ptr statsObj = new juce::DynamicObject();
    statsObj->setProperty("min", stats.min);
    statsObj->setProperty("max", stats.max);
    statsObj->setProperty("mean", stats.mean);
    statsObj->setProperty("rms", stats.rms);
    jsonData->setProperty("statistics", statsObj.get());
    juce::Array<juce::var> dataArray;
    auto* data = differenceBuffer.getReadPointer(0);
    int samplesToExport = std::min(1000, differenceBuffer.getNumSamples());
    for (int i = 0; i < samplesToExport; ++i)
    {
        dataArray.add(data[i]);
    }
    jsonData->setProperty("differenceData", dataArray);
    juce::var jsonVar(jsonData.get());
    juce::String jsonString = juce::JSON::toString(jsonVar, true);
    file.replaceWithText(jsonString);
    DBG("Exported difference data to: " + file.getFullPathName());
}

// ============================================================================
// Sample-level utility operations (trim + normalize)
// ============================================================================

void NoiseBasedSamplerAudioProcessor::trimSilence(float thresholdDb)
{
    const juce::ScopedLock sl(sampleLock);

    if (!sampleLoaded || originalSample.getNumSamples() == 0)
        return;

    const int numChannels = originalSample.getNumChannels();
    const int numSamples = originalSample.getNumSamples();

    const float thresholdLin = std::pow(10.0f, thresholdDb / 20.0f); // e.g. -60 dB -> ~0.001

    int start = 0;
    int end = numSamples - 1;

    // Find first non-silent sample from the left
    bool foundStart = false;
    for (int i = 0; i < numSamples; ++i)
    {
        float maxAtSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            maxAtSample = juce::jmax(maxAtSample,
                std::abs(originalSample.getSample(ch, i)));

        if (maxAtSample >= thresholdLin)
        {
            start = i;
            foundStart = true;
            break;
        }
    }

    // If nothing above threshold, do nothing
    if (!foundStart)
        return;

    // Find first non-silent sample from the right
    for (int i = numSamples - 1; i >= 0; --i)
    {
        float maxAtSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            maxAtSample = juce::jmax(maxAtSample,
                std::abs(originalSample.getSample(ch, i)));

        if (maxAtSample >= thresholdLin)
        {
            end = i;
            break;
        }
    }

    const int trimmedLength = end - start + 1;
    if (trimmedLength <= 0 || trimmedLength == numSamples)
        return;

    juce::AudioBuffer<float> trimmed(numChannels, trimmedLength);

    for (int ch = 0; ch < numChannels; ++ch)
        trimmed.copyFrom(ch, 0, originalSample, ch, start, trimmedLength);

    DBG("✂️ TrimSilence: " + juce::String(numSamples) + " -> " +
        juce::String(trimmedLength) + " samples");

    // Reuse standard loading path to update all dependent buffers/player/state
    loadSampleFromBuffer(trimmed);
}

void NoiseBasedSamplerAudioProcessor::normalizeSample(float targetDb)
{
    const juce::ScopedLock sl(sampleLock);

    if (!sampleLoaded || originalSample.getNumSamples() == 0)
        return;

    const int numChannels = originalSample.getNumChannels();
    const int numSamples = originalSample.getNumSamples();

    // Find peak amplitude
    float peak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = originalSample.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax(peak, std::abs(data[i]));
    }

    if (peak <= 0.0f)
        return;

    const float targetLin = std::pow(10.0f, targetDb / 20.0f); // 0 dB -> 1.0
    const float gain = targetLin / peak;

    juce::AudioBuffer<float> normalized;
    normalized.makeCopyOf(originalSample);

    normalized.applyGain(gain);

    DBG("📈 Normalize: peak=" + juce::String(peak, 4) +
        " -> target=" + juce::String(targetLin, 4) +
        ", gain=" + juce::String(gain, 4));

    loadSampleFromBuffer(normalized);
}

void NoiseBasedSamplerAudioProcessor::analyzeSpectralIndices()
{
    const juce::ScopedLock sl(sampleLock);

    if (!sampleLoaded)
    {
        DBG("Ã¢ÂÅ’ Cannot analyze: no sample loaded");
        return;
    }

    // Ã¢Å“â€¦ ÃËœÃÂ¡ÃÅ¸Ã ÃÂÃâ€™Ãâ€ºÃâ€¢ÃÂÃËœÃâ€¢: ÃËœÃ‘ÂÃÂ¿ÃÂ¾ÃÂ»Ã‘Å’ÃÂ·Ã‘Æ’ÃÂµÃÂ¼ ÃÂ¢Ãâ€¢ÃÅ¡ÃÂ£ÃÂ©ÃËœÃâ„¢ outputBuffer ÃÂµÃ‘ÂÃÂ»ÃÂ¸ ÃÂµÃ‘ÂÃ‘â€šÃ‘Å’
    const juce::AudioBuffer<float>* audioToAnalyze = nullptr;

    if (outputBuffer.getNumSamples() > 0)
    {
        audioToAnalyze = &outputBuffer;
        DBG("Ã¢Å“â€¦ Analyzing CURRENT outputBuffer (includes all edits)");
    }
    else if (originalSample.getNumSamples() > 0)
    {
        audioToAnalyze = &originalSample;
        DBG("Ã¢Å¡ Ã¯Â¸Â Analyzing originalSample (no edits yet)");
    }
    else
    {
        DBG("Ã¢ÂÅ’ No audio to analyze!");
        return;
    }

    DBG("===========================================");
    DBG("ANALYZING SPECTRAL INDICES (CURRENT STATE)");
    DBG("===========================================");
    DBG("Samples: " + juce::String(audioToAnalyze->getNumSamples()));

    // ÃÂÃÂ½ÃÂ°ÃÂ»ÃÂ¸ÃÂ·ÃÂ¸Ã‘â‚¬Ã‘Æ’ÃÂµÃÂ¼ Ã‘â€šÃÂµÃÂºÃ‘Æ’Ã‘â€°ÃÂµÃÂµ Ã‘ÂÃÂ¾Ã‘ÂÃ‘â€šÃÂ¾Ã‘ÂÃÂ½ÃÂ¸ÃÂµ
    indexDatabase.analyzeSample(*audioToAnalyze, currentSampleRate);

    // Ã¢Å“â€¦ ÃÅ¡Ã ÃËœÃÂ¢ÃËœÃÂ§ÃÂÃÅ¾: ÃÂ¡ÃÂ±Ã‘â‚¬ÃÂ°Ã‘ÂÃ‘â€¹ÃÂ²ÃÂ°ÃÂµÃÂ¼ Ã‘â€žÃÂ»ÃÂ°ÃÂ³ ÃÂ¼ÃÂ¾ÃÂ´ÃÂ¸Ã‘â€žÃÂ¸ÃÂºÃÂ°Ã‘â€ ÃÂ¸ÃÂ¹
    // (Ã‘â€š.ÃÂº. Ã‘ÂÃ‘â€šÃÂ¾ ÃÂ½ÃÂ¾ÃÂ²ÃÂ°Ã‘Â ÃÂ±ÃÂ°ÃÂ·ÃÂ° ÃÂ¾Ã‘â€š Ã‘â€šÃÂµÃÂºÃ‘Æ’Ã‘â€°ÃÂµÃÂ³ÃÂ¾ ÃÂ°Ã‘Æ’ÃÂ´ÃÂ¸ÃÂ¾)
    indicesModified = false;

    auto stats = indexDatabase.getStatistics();
    DBG("Ã¢Å“â€¦ Analysis complete:");
    DBG("  Overview indices: " + juce::String(stats.overviewTotalIndices));
    DBG("  Transients: " + juce::String(stats.totalTransients));
    DBG("  Peaks: " + juce::String(stats.totalPeaks));
    DBG("===========================================");
}

void NoiseBasedSamplerAudioProcessor::searchForPatterns()
{
    if (!sampleLoaded || differenceBuffer.getNumSamples() == 0)
    {
        DBG("Ã¢Å¡ Ã¯Â¸Â Cannot search patterns: no sample loaded");
        return;
    }
    DBG("Ã°Å¸â€Â Starting pattern search...");
    DBG("Difference buffer size: " + juce::String(differenceBuffer.getNumSamples()));
    DBG("Sample rate: " + juce::String(currentSampleRate));
    auto* data = differenceBuffer.getReadPointer(0);
    float minVal = *std::min_element(data, data + differenceBuffer.getNumSamples());
    float maxVal = *std::max_element(data, data + differenceBuffer.getNumSamples());
    DBG("Difference range: " + juce::String(minVal, 4) + " to " + juce::String(maxVal, 4));
    patternLibrary.clearPatterns();
    std::vector<Pattern> foundPatterns = patternDetector.detectPatterns(
        differenceBuffer,
        currentSampleRate,
        &indexDatabase
    );
    DBG("Ã¢Å“â€¦ Found " + juce::String(foundPatterns.size()) + " patterns");
    patternLibrary.addPatterns(foundPatterns);
}
void NoiseBasedSamplerAudioProcessor::applyPatternToSample(Pattern& pattern, float intensity)
{
    if (!sampleLoaded || differenceBuffer.getNumSamples() == 0)
        return;
    pattern.applyToBuffer(differenceBuffer, intensity);
    float scale = scaleParam->get();
    float offset = offsetParam->get();
    int bitDepth = bitDepthParam->get();
    differenceEngine.reconstruct(noiseBuffer, differenceBuffer, reconstructedBuffer,
        scale, offset, bitDepth);
    samplePlayer.setSample(reconstructedBuffer);
    DBG("Applied pattern modifications to sample");
}

void NoiseBasedSamplerAudioProcessor::synthesizeFromSpectralIndices(
    const SpectralIndexData& indices,
    juce::AudioBuffer<float>& outputBuffer)
{
    if (indices.getNumFrames() == 0 || outputBuffer.getNumSamples() == 0)
    {
        DBG("Ã¢Å¡ Ã¯Â¸Â Cannot synthesize: empty data");
        return;
    }

    DBG("Ã°Å¸Å½Âµ Ãâ€ºÃÅ¾ÃÅ¡ÃÂÃâ€ºÃÂ¬ÃÂÃÂ«Ãâ„¢ Ã‘â‚¬ÃÂµÃ‘ÂÃÂ¸ÃÂ½Ã‘â€šÃÂµÃÂ· spectral indices...");

    // ÃÅ¸ÃÂ¾ÃÂ»Ã‘Æ’Ã‘â€¡ÃÂ°ÃÂµÃÂ¼ ÃÂ¢ÃÅ¾Ãâ€ºÃÂ¬ÃÅ¡ÃÅ¾ ÃÂ¼ÃÂ¾ÃÂ´ÃÂ¸Ã‘â€žÃÂ¸Ã‘â€ ÃÂ¸Ã‘â‚¬ÃÂ¾ÃÂ²ÃÂ°ÃÂ½ÃÂ½Ã‘â€¹ÃÂµ bins
    auto modifiedBins = indices.getAllModifiedBins();

    if (modifiedBins.empty())
    {
        DBG("  No modifications");
        return;
    }

    DBG("  Frames: " + juce::String(indices.getNumFrames()));
    DBG("  Bins: " + juce::String(indices.getNumBins()));
    DBG("  Modified bins: " + juce::String(modifiedBins.size()));

    // Ãâ€œÃ‘â‚¬Ã‘Æ’ÃÂ¿ÃÂ¿ÃÂ¸Ã‘â‚¬Ã‘Æ’ÃÂµÃÂ¼ ÃÂ¿ÃÂ¾ Ã‘â€žÃ‘â‚¬ÃÂµÃÂ¹ÃÂ¼ÃÂ°ÃÂ¼ ÃÂ´ÃÂ»Ã‘Â Ã‘ÂÃ‘â€žÃ‘â€žÃÂµÃÂºÃ‘â€šÃÂ¸ÃÂ²ÃÂ½ÃÂ¾Ã‘ÂÃ‘â€šÃÂ¸
    std::map<int, std::vector<SpectralIndexData::ModifiedBinInfo>> modsByFrame;

    for (const auto& binInfo : modifiedBins)
    {
        modsByFrame[binInfo.frameIdx].push_back(binInfo);
    }

    DBG("  Modified frames: " + juce::String(modsByFrame.size()));

    // Ã¢Å“â€¦ ÃÂ£ÃÅ“Ãâ€¢ÃÂÃÂ¬ÃÂ¨Ãâ€¢ÃÂÃÂÃÅ¾Ãâ€¢ ÃÂ¾ÃÂºÃÂ½ÃÂ¾ ÃÂ´ÃÂ»Ã‘Â ÃÂ±ÃÂ¾ÃÂ»Ã‘Å’Ã‘Ë†ÃÂµÃÂ¹ ÃÂ»ÃÂ¾ÃÂºÃÂ°ÃÂ»Ã‘Å’ÃÂ½ÃÂ¾Ã‘ÂÃ‘â€šÃÂ¸
    int windowSize = 512;  // Ãâ€˜Ã‘â€¹ÃÂ»ÃÂ¾ 1024, Ã‘â€šÃÂµÃÂ¿ÃÂµÃ‘â‚¬Ã‘Å’ ÃÂµÃ‘â€°Ã‘â€˜ ÃÂ¼ÃÂµÃÂ½Ã‘Å’Ã‘Ë†ÃÂµ
    int halfWindow = windowSize / 2;

    // Hann window
    std::vector<float> window(windowSize);
    for (int i = 0; i < windowSize; ++i)
    {
        window[i] = 0.5f * (1.0f - std::cos(
            2.0f * juce::MathConstants<float>::pi * i / (windowSize - 1)));
    }

    // Ã¢Å“â€¦ ÃÂÃÅ¾Ãâ€™ÃÅ¾Ãâ€¢: ÃÅ¾Ã‘â€šÃ‘ÂÃÂ»ÃÂµÃÂ¶ÃÂ¸ÃÂ²ÃÂ°ÃÂµÃÂ¼ peak contributions ÃÂ´ÃÂ»Ã‘Â soft limiting
    std::vector<float> localPeaks(outputBuffer.getNumSamples(), 0.0f);

    // Ã¢Å“â€¦ ÃÅ¸Ã‘â‚¬ÃÂ¸ÃÂ¼ÃÂµÃÂ½Ã‘ÂÃÂµÃÂ¼ ÃÂ¸ÃÂ·ÃÂ¼ÃÂµÃÂ½ÃÂµÃÂ½ÃÂ¸Ã‘Â Ã‘â€žÃ‘â‚¬ÃÂµÃÂ¹ÃÂ¼ ÃÂ·ÃÂ° Ã‘â€žÃ‘â‚¬ÃÂµÃÂ¹ÃÂ¼ÃÂ¾ÃÂ¼
    for (const auto& [frameIdx, frameMods] : modsByFrame)
    {
        const auto& frame = indices.getFrame(frameIdx);
        float timePosition = frame.timePosition;

        int samplePos = static_cast<int>(timePosition * currentSampleRate);

        if (samplePos < 0 || samplePos >= outputBuffer.getNumSamples())
            continue;

        // Ãâ€ÃÂ»Ã‘Â ÃÂºÃÂ°ÃÂ¶ÃÂ´ÃÂ¾ÃÂ³ÃÂ¾ modified bin ÃÂ² Ã‘ÂÃ‘â€šÃÂ¾ÃÂ¼ Ã‘â€žÃ‘â‚¬ÃÂµÃÂ¹ÃÂ¼ÃÂµ
        for (const auto& binInfo : frameMods)
        {
            auto modifiedIndex = indices.getIndex(frameIdx, binInfo.binIdx);

            // Ã¢Å“â€¦ Ãâ€ºÃÅ¾ÃÅ¡ÃÂÃâ€ºÃÂ¬ÃÂÃÅ¾Ãâ€¢ ÃÂ¸ÃÂ·ÃÂ¼ÃÂµÃÂ½ÃÂµÃÂ½ÃÂ¸ÃÂµ: ÃÂ´ÃÂµÃÂ»Ã‘Å’Ã‘â€šÃÂ° magnitude
            float magnitudeDelta = modifiedIndex.magnitude -
                modifiedIndex.originalMagnitude;

            if (std::abs(magnitudeDelta) < 0.0001f)
                continue;

            float frequency = binInfo.frequency;
            float phase = modifiedIndex.phase;

            // Ã¢Å“â€¦ ÃÂ¡ÃÂ¸ÃÂ½Ã‘â€šÃÂµÃÂ·ÃÂ¸Ã‘â‚¬Ã‘Æ’ÃÂµÃÂ¼ Ã‘Â ÃÅ“Ãâ€¢ÃÂÃÂ¬ÃÂ¨ÃËœÃÅ“ ÃÂ¾ÃÂºÃÂ½ÃÂ¾ÃÂ¼ (ÃÂ±ÃÂ¾ÃÂ»Ã‘Å’Ã‘Ë†ÃÂµ ÃÂ»ÃÂ¾ÃÂºÃÂ°ÃÂ»Ã‘Å’ÃÂ½ÃÂ¾Ã‘ÂÃ‘â€šÃÂ¸)
            for (int i = -halfWindow; i < halfWindow; ++i)
            {
                int targetSample = samplePos + i;
                if (targetSample < 0 || targetSample >= outputBuffer.getNumSamples())
                    continue;

                // Hann window ÃÂ´ÃÂ»Ã‘Â ÃÂ¿ÃÂ»ÃÂ°ÃÂ²ÃÂ½ÃÂ¾Ã‘ÂÃ‘â€šÃÂ¸
                float windowValue = window[i + halfWindow];

                // ÃÂ¡ÃÂ¸ÃÂ½Ã‘Æ’Ã‘Â ÃÂ½ÃÂ° Ã‘â€¡ÃÂ°Ã‘ÂÃ‘â€šÃÂ¾Ã‘â€šÃÂµ bin
                float t = i / static_cast<float>(currentSampleRate);
                float sinValue = std::sin(
                    2.0f * juce::MathConstants<float>::pi * frequency * t + phase);

                // Ã¢Å“â€¦ Ãâ€ºÃÅ¾ÃÅ¡ÃÂÃâ€ºÃÂ¬ÃÂÃÅ¾Ãâ€¢ ÃÂ¸ÃÂ·ÃÂ¼ÃÂµÃÂ½ÃÂµÃÂ½ÃÂ¸ÃÂµ: ÃÂ´ÃÂ¾ÃÂ±ÃÂ°ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼/ÃÂ²Ã‘â€¹Ã‘â€¡ÃÂ¸Ã‘â€šÃÂ°ÃÂµÃÂ¼ ÃÂ´ÃÂµÃÂ»Ã‘Å’Ã‘â€šÃ‘Æ’
                float contribution = magnitudeDelta * sinValue * windowValue;

                // Ã¢Å“â€¦ ÃÂÃÅ¾Ãâ€™ÃÅ¾Ãâ€¢: Ãâ€ºÃÂ¾ÃÂºÃÂ°ÃÂ»Ã‘Å’ÃÂ½Ã‘â€¹ÃÂ¹ soft limiting per-sample
                // Ãâ€™ÃÂ¼ÃÂµÃ‘ÂÃ‘â€šÃÂ¾ ÃÂ³ÃÂ»ÃÂ¾ÃÂ±ÃÂ°ÃÂ»Ã‘Å’ÃÂ½ÃÂ¾ÃÂ¹ ÃÂ½ÃÂ¾Ã‘â‚¬ÃÂ¼ÃÂ°ÃÂ»ÃÂ¸ÃÂ·ÃÂ°Ã‘â€ ÃÂ¸ÃÂ¸ ÃÂ´ÃÂµÃÂ»ÃÂ°ÃÂµÃÂ¼ ÃÂ¼Ã‘ÂÃÂ³ÃÂºÃÂ¾ÃÂµ ÃÂ¾ÃÂ³Ã‘â‚¬ÃÂ°ÃÂ½ÃÂ¸Ã‘â€¡ÃÂµÃÂ½ÃÂ¸ÃÂµ
                float absContribution = std::abs(contribution);

                // Soft saturation ÃÂ´ÃÂ»Ã‘Â ÃÂ±ÃÂ¾ÃÂ»Ã‘Å’Ã‘Ë†ÃÂ¸Ã‘â€¦ ÃÂ²ÃÂºÃÂ»ÃÂ°ÃÂ´ÃÂ¾ÃÂ² (> 0.5)
                if (absContribution > 0.5f)
                {
                    float sign = (contribution > 0.0f) ? 1.0f : -1.0f;
                    // Tanh-like soft saturation
                    contribution = sign * (0.5f + std::tanh((absContribution - 0.5f) * 2.0f) * 0.3f);
                }

                // ÃÅ¸Ã‘â‚¬ÃÂ¸ÃÂ¼ÃÂµÃÂ½Ã‘ÂÃÂµÃÂ¼ ÃÂº ÃÂ¾ÃÂ±ÃÂ¾ÃÂ¸ÃÂ¼ ÃÂºÃÂ°ÃÂ½ÃÂ°ÃÂ»ÃÂ°ÃÂ¼
                for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
                {
                    float* channelData = outputBuffer.getWritePointer(ch);
                    channelData[targetSample] += contribution;

                    // ÃÅ¾Ã‘â€šÃ‘ÂÃÂ»ÃÂµÃÂ¶ÃÂ¸ÃÂ²ÃÂ°ÃÂµÃÂ¼ ÃÂ»ÃÂ¾ÃÂºÃÂ°ÃÂ»Ã‘Å’ÃÂ½Ã‘â€¹ÃÂµ ÃÂ¿ÃÂ¸ÃÂºÃÂ¸
                    localPeaks[targetSample] = juce::jmax(localPeaks[targetSample],
                        std::abs(channelData[targetSample]));
                }
            }
        }
    }

    // Ã¢Å“â€¦ ÃÅ¡Ã ÃËœÃÂ¢ÃËœÃÂ§ÃÂÃÅ¾: ÃÂ£ÃÂ±Ã‘â‚¬ÃÂ°ÃÂ½ÃÂ° Ãâ€œÃâ€ºÃÅ¾Ãâ€˜ÃÂÃâ€ºÃÂ¬ÃÂÃÂÃÂ¯ ÃÂ½ÃÂ¾Ã‘â‚¬ÃÂ¼ÃÂ°ÃÂ»ÃÂ¸ÃÂ·ÃÂ°Ã‘â€ ÃÂ¸Ã‘Â!
    // Ãâ€™ÃÂ¼ÃÂµÃ‘ÂÃ‘â€šÃÂ¾ ÃÂ½ÃÂµÃ‘â€˜ - Ã‘â€šÃÂ¾ÃÂ»Ã‘Å’ÃÂºÃÂ¾ ÃÂ¿Ã‘â‚¬ÃÂ¾ÃÂ²ÃÂµÃ‘â‚¬ÃÂºÃÂ° Ã‘ÂÃÂºÃ‘ÂÃ‘â€šÃ‘â‚¬ÃÂµÃÂ¼ÃÂ°ÃÂ»Ã‘Å’ÃÂ½Ã‘â€¹Ã‘â€¦ ÃÂ¿ÃÂ¸ÃÂºÃÂ¾ÃÂ²

    // ÃÂ¡Ã‘â€¡ÃÂ¸Ã‘â€šÃÂ°ÃÂµÃÂ¼ Ã‘ÂÃ‘â€šÃÂ°Ã‘â€šÃÂ¸Ã‘ÂÃ‘â€šÃÂ¸ÃÂºÃ‘Æ’
    float maxPeak = 0.0f;
    int extremePeaks = 0;

    for (int i = 0; i < outputBuffer.getNumSamples(); ++i)
    {
        if (localPeaks[i] > maxPeak)
            maxPeak = localPeaks[i];

        if (localPeaks[i] > 0.99f)
            extremePeaks++;
    }

    DBG("  Max peak: " + juce::String(maxPeak, 3));
    DBG("  Extreme peaks: " + juce::String(extremePeaks));

    // Ã¢Å“â€¦ ÃÂÃÅ¾Ãâ€™ÃÂÃÂ¯ Ãâ€ºÃÅ¾Ãâ€œÃËœÃÅ¡ÃÂ: ÃÂ¢ÃÂ¾ÃÂ»Ã‘Å’ÃÂºÃÂ¾ ÃÂµÃ‘ÂÃÂ»ÃÂ¸ ÃÅ“ÃÂÃÅ¾Ãâ€œÃÅ¾ Ã‘ÂÃÂºÃ‘ÂÃ‘â€šÃ‘â‚¬ÃÂµÃÂ¼ÃÂ°ÃÂ»Ã‘Å’ÃÂ½Ã‘â€¹Ã‘â€¦ ÃÂ¿ÃÂ¸ÃÂºÃÂ¾ÃÂ² (> 1% Ã‘ÂÃÂµÃÂ¼ÃÂ¿ÃÂ»ÃÂ¾ÃÂ²)
    if (extremePeaks > outputBuffer.getNumSamples() / 100)
    {
        DBG("Ã¢Å¡ Ã¯Â¸Â Applying LOCALIZED soft limiter to extreme peaks only");

        // ÃÅ¸Ã‘â‚¬ÃÂ¸ÃÂ¼ÃÂµÃÂ½Ã‘ÂÃÂµÃÂ¼ soft limiting ÃÂ¢ÃÅ¾Ãâ€ºÃÂ¬ÃÅ¡ÃÅ¾ ÃÂº Ã‘ÂÃ‘ÂÃÂ¼ÃÂ¿ÃÂ»ÃÂ°ÃÂ¼ Ã‘Â ÃÂ¿ÃÂ¸ÃÂºÃÂ°ÃÂ¼ÃÂ¸
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            float* channelData = outputBuffer.getWritePointer(ch);

            for (int i = 0; i < outputBuffer.getNumSamples(); ++i)
            {
                // ÃÅ¸Ã‘â‚¬ÃÂ¸ÃÂ¼ÃÂµÃÂ½Ã‘ÂÃÂµÃÂ¼ Ã‘â€šÃÂ¾ÃÂ»Ã‘Å’ÃÂºÃÂ¾ ÃÂµÃ‘ÂÃÂ»ÃÂ¸ Ã‘ÂÃ‘â€šÃÂ¾Ã‘â€š Ã‘ÂÃ‘ÂÃÂ¼ÃÂ¿ÃÂ» Ã‘â‚¬ÃÂµÃÂ°ÃÂ»Ã‘Å’ÃÂ½ÃÂ¾ ÃÂ³Ã‘â‚¬ÃÂ¾ÃÂ¼ÃÂºÃÂ¸ÃÂ¹
                if (std::abs(channelData[i]) > 0.95f)
                {
                    float sign = (channelData[i] > 0.0f) ? 1.0f : -1.0f;
                    float absVal = std::abs(channelData[i]);

                    // Soft knee compression ÃÂ²Ã‘â€¹Ã‘Ë†ÃÂµ 0.95
                    if (absVal > 0.95f)
                    {
                        float excess = absVal - 0.95f;
                        // ÃÅ“Ã‘ÂÃÂ³ÃÂºÃÂ¾ÃÂµ Ã‘ÂÃÂ¶ÃÂ°Ã‘â€šÃÂ¸ÃÂµ Ã‘Â ÃÂºÃÂ¾Ã‘ÂÃ‘â€žÃ‘â€žÃÂ¸Ã‘â€ ÃÂ¸ÃÂµÃÂ½Ã‘â€šÃÂ¾ÃÂ¼ 0.3
                        float compressed = 0.95f + excess * 0.3f;
                        channelData[i] = sign * juce::jlimit(0.0f, 1.0f, compressed);
                    }
                }
            }
        }

        // ÃÅ¸ÃÂµÃ‘â‚¬ÃÂµÃ‘ÂÃ‘â€¡ÃÂ¸Ã‘â€šÃ‘â€¹ÃÂ²ÃÂ°ÃÂµÃÂ¼ ÃÂ¼ÃÂ°ÃÂºÃ‘ÂÃÂ¸ÃÂ¼Ã‘Æ’ÃÂ¼ ÃÂ¿ÃÂ¾Ã‘ÂÃÂ»ÃÂµ limiting
        maxPeak = 0.0f;
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            const float* channelData = outputBuffer.getReadPointer(ch);
            for (int i = 0; i < outputBuffer.getNumSamples(); ++i)
            {
                maxPeak = juce::jmax(maxPeak, std::abs(channelData[i]));
            }
        }
    }

    DBG("Ã¢Å“â€¦ Ãâ€ºÃÂ¾ÃÂºÃÂ°ÃÂ»Ã‘Å’ÃÂ½Ã‘â€¹ÃÂ¹ Ã‘â‚¬ÃÂµÃ‘ÂÃÂ¸ÃÂ½Ã‘â€šÃÂµÃÂ· ÃÂ·ÃÂ°ÃÂ²ÃÂµÃ‘â‚¬Ã‘Ë†Ã‘â€˜ÃÂ½!");
    DBG("   Final max peak: " + juce::String(maxPeak, 3));
    DBG("   Original audio PRESERVED everywhere except modified regions");
}

void NoiseBasedSamplerAudioProcessor::removeFeatureSamples(int startSample, int endSample)
{
    const juce::ScopedLock sl(sampleLock);

    if (!hasFeatureData() || startSample < 0 || endSample >= featureData.getNumSamples())
    {
        DBG("Ã¢Å¡ Ã¯Â¸Â Cannot remove samples: invalid range or no data");
        return;
    }

    if (startSample > endSample)
    {
        std::swap(startSample, endSample);
    }

    int numToRemove = endSample - startSample + 1;
    int numSamples = featureData.getNumSamples();
    int newNumSamples = numSamples - numToRemove;

    if (newNumSamples <= 0)
    {
        // ÃÂ£ÃÂ´ÃÂ°ÃÂ»Ã‘ÂÃÂµÃÂ¼ Ãâ€™ÃÂ¡ÃÂ
        featureData = FeatureData();
        originalSample.setSize(2, 0);
        originalSampleBackup.setSize(2, 0);
        outputBuffer.setSize(2, 0);
        samplePlayer.setSample(outputBuffer);

        DBG("Ã°Å¸â€”â€˜Ã¯Â¸Â All samples removed - audio is now empty");
        return;
    }

    DBG("===========================================");
    DBG("REMOVING SAMPLES FROM TIMELINE");
    DBG("===========================================");
    DBG("Removing samples: " + juce::String(startSample) + " to " + juce::String(endSample));
    DBG("Total to remove: " + juce::String(numToRemove));
    DBG("New length: " + juce::String(newNumSamples) + " samples");

    // 1Ã¯Â¸ÂÃ¢Æ’Â£ ÃÂ¡ÃÂ¾ÃÂ·ÃÂ´ÃÂ°Ã‘â€˜ÃÂ¼ ÃÂ½ÃÂ¾ÃÂ²Ã‘â€¹ÃÂ¹ FeatureData ÃÂ±ÃÂµÃÂ· Ã‘Æ’ÃÂ´ÃÂ°ÃÂ»Ã‘â€˜ÃÂ½ÃÂ½ÃÂ¾ÃÂ³ÃÂ¾ Ã‘â‚¬ÃÂµÃÂ³ÃÂ¸ÃÂ¾ÃÂ½ÃÂ°
    FeatureData newFeatures;
    newFeatures.setSize(newNumSamples);

    // ÃÅ¡ÃÂ¾ÃÂ¿ÃÂ¸Ã‘â‚¬Ã‘Æ’ÃÂµÃÂ¼ Ã‘ÂÃ‘ÂÃÂ¼ÃÂ¿ÃÂ»Ã‘â€¹ Ãâ€ÃÅ¾ Ã‘Æ’ÃÂ´ÃÂ°ÃÂ»Ã‘ÂÃÂµÃÂ¼ÃÂ¾ÃÂ³ÃÂ¾ Ã‘â‚¬ÃÂµÃÂ³ÃÂ¸ÃÂ¾ÃÂ½ÃÂ°
    for (int i = 0; i < startSample; ++i)
    {
        newFeatures[i] = featureData[i];
    }

    // ÃÅ¡ÃÂ¾ÃÂ¿ÃÂ¸Ã‘â‚¬Ã‘Æ’ÃÂµÃÂ¼ Ã‘ÂÃ‘ÂÃÂ¼ÃÂ¿ÃÂ»Ã‘â€¹ ÃÅ¸ÃÅ¾ÃÂ¡Ãâ€ºÃâ€¢ Ã‘Æ’ÃÂ´ÃÂ°ÃÂ»Ã‘ÂÃÂµÃÂ¼ÃÂ¾ÃÂ³ÃÂ¾ Ã‘â‚¬ÃÂµÃÂ³ÃÂ¸ÃÂ¾ÃÂ½ÃÂ°
    for (int i = endSample + 1; i < numSamples; ++i)
    {
        newFeatures[i - numToRemove] = featureData[i];
    }

    // Ãâ€”ÃÂ°ÃÂ¼ÃÂµÃÂ½Ã‘ÂÃÂµÃÂ¼ Ã‘ÂÃ‘â€šÃÂ°Ã‘â‚¬Ã‘â€¹ÃÂµ ÃÂ´ÃÂ°ÃÂ½ÃÂ½Ã‘â€¹ÃÂµ
    featureData = newFeatures;

    // 2Ã¯Â¸ÂÃ¢Æ’Â£ Ã¢Å“â€¦ ÃÅ¡Ã ÃËœÃÂ¢ÃËœÃÂ§ÃÂÃÅ¾: ÃËœÃ‘ÂÃÂ¿ÃÂ¾ÃÂ»Ã‘Å’ÃÂ·Ã‘Æ’ÃÂµÃÂ¼ AudioStateManager ÃÂ´ÃÂ»Ã‘Â ÃÂ¿Ã‘â‚¬ÃÂ°ÃÂ²ÃÂ¸ÃÂ»Ã‘Å’ÃÂ½ÃÂ¾ÃÂ³ÃÂ¾ Ã‘Æ’ÃÂ´ÃÂ°ÃÂ»ÃÂµÃÂ½ÃÂ¸Ã‘Â
    DBG("Using AudioStateManager to rebuild audio timeline...");

    // Ã¢Å“â€¦ ÃÅ¡Ã ÃËœÃÂ¢ÃËœÃÂ§ÃÂÃÅ¾: ÃÂ¡ÃÂ¾ÃÂ·ÃÂ´ÃÂ°Ã‘â€˜ÃÂ¼ STEREO buffer!
    const int STEREO_CHANNELS = 2;
    juce::AudioBuffer<float> newAudioBuffer(STEREO_CHANNELS, newNumSamples);
    newAudioBuffer.clear();

    // ÃÅ¸Ã‘â‚¬ÃÂ¸ÃÂ¼ÃÂµÃÂ½Ã‘ÂÃÂµÃÂ¼ features (ÃÂºÃÂ¾Ã‘â€šÃÂ¾Ã‘â‚¬Ã‘â€¹ÃÂµ Ã‘ÂÃÂ¾ÃÂ·ÃÂ´ÃÂ°ÃÂ´Ã‘Æ’Ã‘â€š STEREO)
    featureData.applyToAudioBuffer(newAudioBuffer, currentSampleRate);

    // Ã¢Å“â€¦ ÃÅ¸Ã ÃÅ¾Ãâ€™Ãâ€¢Ã ÃÅ¡ÃÂ: ÃÂ£ÃÂ±ÃÂµÃÂ´ÃÂ¸ÃÂ¼Ã‘ÂÃ‘Â Ã‘â€¡Ã‘â€šÃÂ¾ ÃÂ¿ÃÂ¾ÃÂ»Ã‘Æ’Ã‘â€¡ÃÂ¸ÃÂ»ÃÂ¸ STEREO
    if (newAudioBuffer.getNumChannels() < 2)
    {
        DBG("Ã¢ÂÅ’ ERROR: applyToAudioBuffer returned MONO!");
        return;
    }

    // ÃÅ¾ÃÂ±ÃÂ½ÃÂ¾ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼ ÃÂ±Ã‘Æ’Ã‘â€žÃÂµÃ‘â‚¬Ã‘â€¹
    originalSample.makeCopyOf(newAudioBuffer);
    outputBuffer.makeCopyOf(newAudioBuffer);

    // Ã¢Å“â€¦ ÃÅ¡Ã ÃËœÃÂ¢ÃËœÃÂ§ÃÂÃÅ¾: ÃÂÃâ€¢ ÃÂ¸Ã‘ÂÃÂ¿ÃÂ¾ÃÂ»Ã‘Å’ÃÂ·Ã‘Æ’ÃÂµÃÂ¼ loadSample - ÃÂ¾ÃÂ½ ÃÂ¼ÃÂ¾ÃÂ¶ÃÂµÃ‘â€š Ã‘ÂÃÂ±Ã‘â‚¬ÃÂ¾Ã‘ÂÃÂ¸Ã‘â€šÃ‘Å’ STEREO!
    // Ãâ€™ÃÂ¼ÃÂµÃ‘ÂÃ‘â€šÃÂ¾ Ã‘ÂÃ‘â€šÃÂ¾ÃÂ³ÃÂ¾ ÃÂ¾ÃÂ±ÃÂ½ÃÂ¾ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼ Ã‘â€šÃÂ¾ÃÂ»Ã‘Å’ÃÂºÃÂ¾ Ã‘â€šÃÂ¾ Ã‘â€¡Ã‘â€šÃÂ¾ ÃÂ½Ã‘Æ’ÃÂ¶ÃÂ½ÃÂ¾:

    // ÃÅ¾ÃÂ±ÃÂ½ÃÂ¾ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼ player
    samplePlayer.allNotesOff();
    samplePlayer.setSample(outputBuffer);

    // Ã ÃÂµÃ‘ÂÃÂ¸ÃÂ½Ã‘â€¦Ã‘â‚¬ÃÂ¾ÃÂ½ÃÂ¸ÃÂ·ÃÂ°Ã‘â€ ÃÂ¸Ã‘Â ÃÂ¸ÃÂ½ÃÂ´ÃÂµÃÂºÃ‘ÂÃÂ¾ÃÂ² Ãâ€™Ã ÃÂ£ÃÂ§ÃÂÃÂ£ÃÂ®
    DBG("Resynchronizing spectral indices...");
    indexDatabase.clearCache();
    indexDatabase.analyzeSample(outputBuffer, currentSampleRate);

    // Ã¢Å“â€¦ Ãâ€™ÃÂÃâ€“ÃÂÃÅ¾: ÃÅ¾ÃÂ±ÃÂ½ÃÂ¾ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼ groundTruth ÃÂ² AudioStateManager ÃÂ½ÃÂ°ÃÂ¿Ã‘â‚¬Ã‘ÂÃÂ¼Ã‘Æ’Ã‘Å½
    // (ÃÂ±ÃÂµÃÂ· ÃÂ¿ÃÂ¾ÃÂ»ÃÂ½ÃÂ¾ÃÂ¹ ÃÂ¿ÃÂµÃ‘â‚¬ÃÂµÃÂ·ÃÂ°ÃÂ³Ã‘â‚¬Ã‘Æ’ÃÂ·ÃÂºÃÂ¸ Ã‘â€¡ÃÂµÃ‘â‚¬ÃÂµÃÂ· loadSample)
    // ÃÂ­Ã‘â€šÃÂ¾ ÃÂ¼ÃÂ¾ÃÂ¶ÃÂ½ÃÂ¾ Ã‘ÂÃÂ´ÃÂµÃÂ»ÃÂ°Ã‘â€šÃ‘Å’ ÃÂ´ÃÂ¾ÃÂ±ÃÂ°ÃÂ²ÃÂ¸ÃÂ² ÃÂ¼ÃÂµÃ‘â€šÃÂ¾ÃÂ´ ÃÂ² AudioStateManager:
    // audioState.updateGroundTruth(outputBuffer);

    featuresModifiedByUser = false;
    indicesModified = false;

    DBG("Ã¢Å“â€¦ Timeline region removed (STEREO preserved)!");
    DBG("===========================================");
}


bool NoiseBasedSamplerAudioProcessor::areAllIndicesSynced() const
{
    return audioState.isFullySynced();
}

void NoiseBasedSamplerAudioProcessor::forceFullResync()
{
    const juce::ScopedLock sl(sampleLock);

    DBG("Ã°Å¸â€â€ž User requested FULL RESYNC");

    // ÃËœÃ‘ÂÃÂ¿ÃÂ¾ÃÂ»Ã‘Å’ÃÂ·Ã‘Æ’ÃÂµÃÂ¼ AudioStateManager ÃÂ´ÃÂ»Ã‘Â ÃÂ¿ÃÂ¾ÃÂ»ÃÂ½ÃÂ¾ÃÂ¹ Ã‘ÂÃÂ¸ÃÂ½Ã‘â€¦Ã‘â‚¬ÃÂ¾ÃÂ½ÃÂ¸ÃÂ·ÃÂ°Ã‘â€ ÃÂ¸ÃÂ¸
    audioState.forceFullSync(featureExtractor, indexDatabase);

    // ÃËœÃÂ·ÃÂ²ÃÂ»ÃÂµÃÂºÃÂ°ÃÂµÃÂ¼ features ÃÂ¸ÃÂ· ground truth
    auto groundTruth = audioState.getGroundTruthAudio();
    juce::AudioBuffer<float> monoForAnalysis(1, groundTruth.getNumSamples());
    monoForAnalysis.copyFrom(0, 0, groundTruth, 0, 0, groundTruth.getNumSamples());

    featureData = featureExtractor.extractFeatures(monoForAnalysis, currentSampleRate);

    // ÃÅ¾ÃÂ±ÃÂ½ÃÂ¾ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼ ÃÂ±Ã‘Æ’Ã‘â€žÃÂµÃ‘â‚¬Ã‘â€¹
    outputBuffer.makeCopyOf(groundTruth);
    originalSample.makeCopyOf(groundTruth);

    samplePlayer.setSample(outputBuffer);

    featuresModifiedByUser = false;
    indicesModified = false;

    DBG("Ã¢Å“â€¦ FULL RESYNC complete - all indices synchronized");
}

void NoiseBasedSamplerAudioProcessor::synthesizeFromModifiedIndices()
{
    const juce::ScopedLock sl(sampleLock);

    const auto* indices = indexDatabase.getOverviewIndices();
    if (!indices || indices->getNumFrames() == 0)
    {
        DBG("Ã¢ÂÅ’ Cannot synthesize: no indices available");
        return;
    }

    auto modifiedBins = indices->getAllModifiedBins();
    if (modifiedBins.empty())
    {
        DBG("Ã¢Å¡ Ã¯Â¸Â No modifications detected");
        return;
    }

    DBG("===========================================");
    DBG("Ã°Å¸Å½Âµ APPLYING SPECTRAL CHANGES (STEREO)");
    DBG("===========================================");

    // Ã¢Å“â€¦ ÃÂÃÅ¾Ãâ€™ÃÅ¾Ãâ€¢: ÃËœÃ‘ÂÃÂ¿ÃÂ¾ÃÂ»Ã‘Å’ÃÂ·Ã‘Æ’ÃÂµÃÂ¼ AudioStateManager
    audioState.applySpectralChanges(
        *indices,
        featureExtractor,
        true  // auto-sync features
    );

    // Ã¢Å“â€¦ Ãâ€™ÃÂÃâ€“ÃÂÃÅ¾: ÃÅ¸ÃÂµÃ‘â‚¬ÃÂµÃ‘â€¡ÃÂ¸Ã‘â€šÃ‘â€¹ÃÂ²ÃÂ°ÃÂµÃÂ¼ features ÃÂ¸ÃÂ· ÃÂ½ÃÂ¾ÃÂ²ÃÂ¾ÃÂ³ÃÂ¾ audio
    auto groundTruth = audioState.getGroundTruthAudio();

    // Ã¢Å“â€¦ ÃÅ¸Ã ÃÅ¾Ãâ€™Ãâ€¢Ã ÃÅ¡ÃÂ: Ground truth ÃÂ´ÃÂ¾ÃÂ»ÃÂ¶ÃÂµÃÂ½ ÃÂ±Ã‘â€¹Ã‘â€šÃ‘Å’ ÃÂ² STEREO
    DBG("   Ground truth channels: " + juce::String(groundTruth.getNumChannels()));

    juce::AudioBuffer<float> monoForAnalysis(1, groundTruth.getNumSamples());
    monoForAnalysis.copyFrom(0, 0, groundTruth, 0, 0, groundTruth.getNumSamples());

    // ÃËœÃÂ·ÃÂ²ÃÂ»ÃÂµÃÂºÃÂ°ÃÂµÃÂ¼ ÃÂ½ÃÂ¾ÃÂ²Ã‘â€¹ÃÂµ features
    featureData = featureExtractor.extractFeatures(monoForAnalysis, currentSampleRate);

    DBG("Ã¢Å“â€¦ Features auto-extracted from new audio");

    // ÃÅ¾ÃÂ±ÃÂ½ÃÂ¾ÃÂ²ÃÂ»Ã‘ÂÃÂµÃÂ¼ ÃÂ±Ã‘Æ’Ã‘â€žÃÂµÃ‘â‚¬Ã‘â€¹
    outputBuffer.makeCopyOf(groundTruth);
    originalSample.makeCopyOf(groundTruth);

    samplePlayer.setSample(outputBuffer);

    indicesModified = false;
    featuresModifiedByUser = false;

    DBG("Ã¢Å“â€¦ Spectral applied + Features auto-synced (STEREO)!");
    DBG("   Output channels: " + juce::String(outputBuffer.getNumChannels()));
    DBG("===========================================");
}

bool NoiseBasedSamplerAudioProcessor::areSpectralIndicesSynced() const
{
    const juce::ScopedLock sl(sampleLock);

    // ÃÅ¸Ã‘â‚¬ÃÂ¾ÃÂ²ÃÂµÃ‘â‚¬Ã‘ÂÃÂµÃÂ¼ Ã‘â€¡Ã‘â€šÃÂ¾ ÃÂ¸ÃÂ½ÃÂ´ÃÂµÃÂºÃ‘ÂÃ‘â€¹ ÃÂ°ÃÂºÃ‘â€šÃ‘Æ’ÃÂ°ÃÂ»Ã‘Å’ÃÂ½Ã‘â€¹
    const auto* indices = indexDatabase.getOverviewIndices();

    if (!indices)
        return false;

    // ÃËœÃÂ½ÃÂ´ÃÂµÃÂºÃ‘ÂÃ‘â€¹ Ã‘ÂÃ‘â€¡ÃÂ¸Ã‘â€šÃÂ°Ã‘Å½Ã‘â€šÃ‘ÂÃ‘Â synced ÃÂµÃ‘ÂÃÂ»ÃÂ¸:
    // 1. Ãâ€¢Ã‘ÂÃ‘â€šÃ‘Å’ ÃÂ¸ÃÂ½ÃÂ´ÃÂµÃÂºÃ‘ÂÃ‘â€¹
    // 2. ÃÂÃÂµÃ‘â€š pending feature ÃÂ¸ÃÂ·ÃÂ¼ÃÂµÃÂ½ÃÂµÃÂ½ÃÂ¸ÃÂ¹
    // 3. ÃÂÃÂµÃ‘â€š spectral ÃÂ¼ÃÂ¾ÃÂ´ÃÂ¸Ã‘â€žÃÂ¸ÃÂºÃÂ°Ã‘â€ ÃÂ¸ÃÂ¹

    return !featuresModifiedByUser && !indicesModified;
}

NoiseBasedSamplerAudioProcessor::ModificationStatistics
NoiseBasedSamplerAudioProcessor::getModificationStatistics() const
{
    const juce::ScopedLock sl(sampleLock);
    ModificationStatistics stats;

    const auto* indices = indexDatabase.getOverviewIndices();
    if (!indices)
        return stats;

    auto modifiedBins = indices->getAllModifiedBins();
    stats.totalModifiedBins = static_cast<int>(modifiedBins.size());

    if (modifiedBins.empty())
        return stats;

    std::set<int> uniqueFrames;
    for (const auto& binInfo : modifiedBins)
    {
        uniqueFrames.insert(binInfo.frameIdx);
    }
    stats.totalModifiedFrames = static_cast<int>(uniqueFrames.size());

    stats.minModifiedFreq = modifiedBins[0].frequency;
    stats.maxModifiedFreq = modifiedBins[0].frequency;

    for (const auto& binInfo : modifiedBins)
    {
        stats.minModifiedFreq = juce::jmin(stats.minModifiedFreq, binInfo.frequency);
        stats.maxModifiedFreq = juce::jmax(stats.maxModifiedFreq, binInfo.frequency);

        float time = indices->getFrame(binInfo.frameIdx).timePosition;
        if (binInfo.frameIdx == modifiedBins[0].frameIdx)
        {
            stats.minModifiedTime = time;
            stats.maxModifiedTime = time;
        }
        else
        {
            stats.minModifiedTime = juce::jmin(stats.minModifiedTime, time);
            stats.maxModifiedTime = juce::jmax(stats.maxModifiedTime, time);
        }
    }

    return stats;
}

// 🔊 REAL-TIME EFFECTS PROCESSING
void NoiseBasedSamplerAudioProcessor::applyRealtimeEffects(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // DEBUG: Check if function is being called and sample is loaded
    static bool firstCall = true;
    if (firstCall)
    {
        DBG("🔊 applyRealtimeEffects called! Samples: " + juce::String(numSamples) + " Channels: " + juce::String(numChannels));
        DBG("🔊 Sample loaded: " + juce::String(sampleLoaded ? "YES" : "NO"));
        if (sampleLoaded)
        {
            DBG("🔊 Original sample length: " + juce::String(originalSample.getNumSamples()));
        }
        firstCall = false;
    }

    // Only apply effects if we have audio data
    if (!sampleLoaded)
        return;

    // 1. BOOST/GAIN
    float boostDb = getBoostDb();
    if (std::abs(boostDb) > 0.01f)
    {
        float boostGain = juce::Decibels::decibelsToGain(boostDb);
        buffer.applyGain(boostGain);
        DBG("🔊 BOOST APPLIED: " + juce::String(boostDb, 1) + "dB (gain: " + juce::String(boostGain, 2) + ")");
    }

    // 2. PITCH SHIFT
    float pitchShiftSemitones = getPitchShift();
    if (std::abs(pitchShiftSemitones) > 0.01f)
    {
        float pitchRatio = std::pow(2.0f, pitchShiftSemitones / 12.0f);

        // Simple pitch shifting using resampling
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* channelData = buffer.getReadPointer(ch);
            auto* outputData = buffer.getWritePointer(ch);

            // Create temporary buffer for pitch shifted audio
            juce::AudioBuffer<float> pitchBuffer(1, numSamples);
            auto* pitchData = pitchBuffer.getWritePointer(0);

            // Simple resampling pitch shift
            for (int i = 0; i < numSamples; ++i)
            {
                float sourcePos = i / pitchRatio;
                int sourceIndex = static_cast<int>(sourcePos);
                float fraction = sourcePos - sourceIndex;

                if (sourceIndex < numSamples - 1)
                {
                    float sample1 = channelData[sourceIndex];
                    float sample2 = channelData[sourceIndex + 1];
                    pitchData[i] = sample1 + fraction * (sample2 - sample1);
                }
                else
                {
                    pitchData[i] = 0.0f; // Pad with silence
                }
            }

            // Copy back directly to the channel
            std::copy(pitchData, pitchData + numSamples, outputData);
        }
    }

    // 3. TIME STRETCH
    float timeStretchRatio = getTimeStretch();
    if (std::abs(timeStretchRatio - 1.0f) > 0.01f)
    {
        // Simple time stretching using overlap-add
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* channelData = buffer.getReadPointer(ch);
            auto* outputData = buffer.getWritePointer(ch);

            // Create temporary buffer for time stretched audio
            juce::AudioBuffer<float> stretchBuffer(1, numSamples);
            auto* stretchData = stretchBuffer.getWritePointer(0);

            if (timeStretchRatio > 1.0f) // Slow down
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float sourcePos = i / timeStretchRatio;
                    int sourceIndex = static_cast<int>(sourcePos);
                    float fraction = sourcePos - sourceIndex;

                    if (sourceIndex < numSamples - 1)
                    {
                        float sample1 = channelData[sourceIndex];
                        float sample2 = channelData[sourceIndex + 1];
                        stretchData[i] = sample1 + fraction * (sample2 - sample1);
                    }
                    else
                    {
                        stretchData[i] = channelData[std::min(sourceIndex, numSamples - 1)];
                    }
                }
            }
            else // Speed up
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float sourcePos = i * timeStretchRatio;
                    int sourceIndex = static_cast<int>(sourcePos);
                    stretchData[i] = sourceIndex < numSamples ? channelData[sourceIndex] : 0.0f;
                }
            }

            // Copy back directly to the channel
            std::copy(stretchData, stretchData + numSamples, outputData);
        }
    }

    // 4. LOOP HANDLING (basic implementation)
    if (isLoopActive())
    {
        // For now, just log that loop is active
        // Full loop implementation would requireSamplePlayer integration
        DBG("🔄 Loop active (basic implementation)");
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoiseBasedSamplerAudioProcessor();
}