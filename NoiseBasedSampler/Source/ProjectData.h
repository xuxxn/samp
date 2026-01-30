/*
ProjectData.h - Project Data Structure
Complete snapshot of project state including:
- Original audio buffer (stereo)
- Feature data (all indices: amplitude, frequency, phase, volume, pan)
- Effect states (trim, normalize, reverse, boost)
- Audio state (spectral indices)
- UI state (zoom, pan, selection, etc.)
- Metadata (name, timestamp, duration)
- Waveform thumbnail for preview
*/

#pragma once
#include <JuceHeader.h>
#include "FeatureData.h"
#include <vector>

// ==========================================================================
// PROJECT METADATA
// ==========================================================================

struct ProjectMetadata
{
    juce::String projectName;        // Sample name
    juce::String projectId;          // Unique ID (timestamp-based)
    juce::String filePath;           // Full path to .nbsp file
    // int64 creationTime;              // When created (milliseconds)
    // int64 lastModifiedTime;          // Last edited (milliseconds)

    int creationTime;              // When created (milliseconds)
    int lastModifiedTime;   

    double sampleRate;
    int numSamples;
    int numChannels;
    float durationSeconds;
    bool isValid;

    ProjectMetadata()
        : projectName("Untitled")
        , projectId("")
        , filePath("")
        , creationTime(0)
        , lastModifiedTime(0)
        , sampleRate(44100.0)
        , numSamples(0)
        , numChannels(0)
        , durationSeconds(0.0f)
        , isValid(false)
    {
    }

    juce::String getFormattedDate() const
    {
        juce::Time time(lastModifiedTime);
        return time.formatted("%Y-%m-%d %H:%M:%S");
    }

    juce::String getFormattedDuration() const
    {
        int minutes = static_cast<int>(durationSeconds) / 60;
        int seconds = static_cast<int>(durationSeconds) % 60;
        return juce::String(minutes) + ":" + juce::String(seconds).paddedLeft('0', 2);
    }
};

// ==========================================================================
// EFFECT STATE SNAPSHOT
// ==========================================================================

struct EffectStateSnapshot
{
    bool trimActive = false;
    int trimStart = 0;
    int trimEnd = 0;

    bool normalizeActive = false;
    float normalizeTargetDb = 0.0f;
    float normalizeGain = 1.0f;

    bool reverseActive = false;

    bool boostActive = false;
    float boostDb = 0.0f;
    float boostGain = 1.0f;

    bool adsrCutItselfMode = false;

    EffectStateSnapshot() = default;
};

// ==========================================================================
// PROJECT DATA
// ==========================================================================

class ProjectData
{
public:
    ProjectData()
        : metadata()
    {
    }

    // ========== METADATA ==========

    void setMetadata(const juce::String& name, double sr, int samples, int channels)
    {
        metadata.projectName = name;
        metadata.sampleRate = sr;
        metadata.numSamples = samples;
        metadata.numChannels = channels;
        metadata.durationSeconds = samples / static_cast<float>(sr);
        metadata.isValid = true;
        metadata.creationTime = juce::Time::currentTimeMillis();
        metadata.lastModifiedTime = metadata.creationTime;
        
        // Generate unique ID from timestamp
        metadata.projectId = juce::String(metadata.creationTime);
    }

    void updateModificationTime()
    {
        metadata.lastModifiedTime = juce::Time::currentTimeMillis();
    }

    void setFilePath(const juce::String& path)
    {
        metadata.filePath = path;
    }

    const ProjectMetadata& getMetadata() const { return metadata; }
    ProjectMetadata& getMetadata() { return metadata; }

    // ========== AUDIO DATA ==========

    void setOriginalAudio(const juce::AudioBuffer<float>& audio)
    {
        originalAudio.makeCopyOf(audio);
        
        // Update metadata
        metadata.numSamples = audio.getNumSamples();
        metadata.numChannels = audio.getNumChannels();
        
        // Generate thumbnail for UI preview
        generateThumbnail();
    }

    const juce::AudioBuffer<float>& getOriginalAudio() const
    {
        return originalAudio;
    }

    juce::AudioBuffer<float>& getOriginalAudio()
    {
        return originalAudio;
    }

    // ========== FEATURE DATA ==========

    void setFeatureData(const FeatureData& features)
    {
        featureData = features;
    }

    const FeatureData& getFeatureData() const
    {
        return featureData;
    }

    FeatureData& getFeatureData()
    {
        return featureData;
    }

    // ========== EFFECT STATE ==========

    void setEffectState(const EffectStateSnapshot& state)
    {
        effectState = state;
    }

    const EffectStateSnapshot& getEffectState() const
    {
        return effectState;
    }

    EffectStateSnapshot& getEffectState()
    {
        return effectState;
    }

    // ========== STATE XML ==========

    void setAudioStateXml(std::unique_ptr<juce::XmlElement> xml)
    {
        audioStateXml = std::move(xml);
    }

    void setUIStateXml(std::unique_ptr<juce::XmlElement> xml)
    {
        uiStateXml = std::move(xml);
    }

    const juce::XmlElement* getAudioStateXml() const
    {
        return audioStateXml.get();
    }

    const juce::XmlElement* getUIStateXml() const
    {
        return uiStateXml.get();
    }

    std::unique_ptr<juce::XmlElement> takeAudioStateXml()
    {
        return std::move(audioStateXml);
    }

    std::unique_ptr<juce::XmlElement> takeUIStateXml()
    {
        return std::move(uiStateXml);
    }

    // ========== THUMBNAIL ==========

    void generateThumbnail(int targetPoints = 500)
    {
        thumbnailData.clear();

        if (originalAudio.getNumSamples() == 0)
            return;

        int numSamples = originalAudio.getNumSamples();
        int samplesPerPoint = std::max(1, numSamples / targetPoints);

        // Generate downsampled waveform for quick UI display
        for (int i = 0; i < numSamples; i += samplesPerPoint)
        {
            float maxValue = 0.0f;

            // Get max amplitude across channels in this segment
            for (int ch = 0; ch < originalAudio.getNumChannels(); ++ch)
            {
                const auto* data = originalAudio.getReadPointer(ch);
                
                for (int s = 0; s < samplesPerPoint && (i + s) < numSamples; ++s)
                {
                    maxValue = juce::jmax(maxValue, std::abs(data[i + s]));
                }
            }

            thumbnailData.push_back(maxValue);
        }

        DBG("Generated thumbnail: " + juce::String(thumbnailData.size()) + " points");
    }

    const std::vector<float>& getThumbnailData() const
    {
        return thumbnailData;
    }

    // ========== VALIDATION ==========

    bool isValid() const
    {
        return metadata.isValid &&
               originalAudio.getNumSamples() > 0 &&
               featureData.getNumSamples() > 0;
    }

    // ========== DEBUG INFO ==========

    void printInfo() const
    {
        DBG("===========================================");
        DBG("PROJECT INFO:");
        DBG("  Name: " + metadata.projectName);
        DBG("  ID: " + metadata.projectId);
        DBG("  Duration: " + metadata.getFormattedDuration());
        DBG("  Sample Rate: " + juce::String(metadata.sampleRate, 0) + " Hz");
        DBG("  Samples: " + juce::String(metadata.numSamples));
        DBG("  Channels: " + juce::String(metadata.numChannels));
        DBG("  Features: " + juce::String(featureData.getNumSamples()));
        DBG("  Thumbnail: " + juce::String(thumbnailData.size()) + " points");
        DBG("  Valid: " + juce::String(isValid() ? "YES" : "NO"));
        
        // Effect state info
        DBG("  Effects:");
        DBG("    - Trim: " + juce::String(effectState.trimActive ? "ON" : "OFF"));
        DBG("    - Normalize: " + juce::String(effectState.normalizeActive ? "ON" : "OFF"));
        DBG("    - Reverse: " + juce::String(effectState.reverseActive ? "ON" : "OFF"));
        DBG("    - Boost: " + juce::String(effectState.boostActive ? "ON" : "OFF"));
        
        DBG("===========================================");
    }

private:
    ProjectMetadata metadata;
    juce::AudioBuffer<float> originalAudio;
    FeatureData featureData;
    EffectStateSnapshot effectState;
    
    std::unique_ptr<juce::XmlElement> audioStateXml;
    std::unique_ptr<juce::XmlElement> uiStateXml;
    
    std::vector<float> thumbnailData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectData)
};