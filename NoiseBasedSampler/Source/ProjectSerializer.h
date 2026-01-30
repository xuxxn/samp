/*
ProjectSerializer.h - Binary Project Serialization
Fast binary format (.nbsp) for saving/loading complete project state.

File Format Structure:
[Header]
  - Magic Number: "NBSP" (4 bytes)
  - Format Version: 1 (2 bytes)
  - Save Timestamp (8 bytes)
[Metadata]
  - Project name (string)
  - Sample rate, num samples, channels
  - Duration, timestamps
[Audio Data]
  - Original audio buffer (float samples, all channels)
[Feature Data]
  - All indices: amplitude, frequency, phase, volume, pan
  - Modification flags
[Effect State]
  - Trim, normalize, reverse, boost states
[Audio State XML]
  - Spectral indices state
[UI State XML]
  - UI configuration
[Thumbnail]
  - Downsampled waveform for preview
*/

#pragma once
#include <JuceHeader.h>
#include "ProjectData.h"

class ProjectSerializer
{
public:
    // File format constants
    static const juce::uint32 MAGIC_NUMBER = 0x4E425350;  // "NBSP" in hex
    static const juce::uint16 FORMAT_VERSION = 1;

    // ========== SAVE PROJECT ==========

    static bool saveProject(const ProjectData& project, const juce::File& file)
    {
        if (!project.isValid())
        {
            DBG("❌ Cannot save: project is invalid");
            return false;
        }

        try
        {
            juce::MemoryOutputStream stream;

            // Write all sections
            writeHeader(stream);
            writeMetadata(stream, project.getMetadata());
            writeAudioData(stream, project.getOriginalAudio());
            writeFeatureData(stream, project.getFeatureData());
            writeEffectState(stream, project.getEffectState());
            writeXmlSection(stream, project.getAudioStateXml());
            writeXmlSection(stream, project.getUIStateXml());
            writeThumbnail(stream, project.getThumbnailData());

            // Write to file
            if (file.replaceWithData(stream.getData(), stream.getDataSize()))
            {
                DBG("✅ Project saved: " + file.getFullPathName());
                return true;
            }
            else
            {
                DBG("❌ Failed to write file: " + file.getFullPathName());
                return false;
            }
        }
        catch (const std::exception& e)
        {
            DBG("❌ Exception during save: " + juce::String(e.what()));
            return false;
        }
    }

    // ========== LOAD PROJECT ==========

    static bool loadProject(ProjectData& project, const juce::File& file)
    {
        if (!file.existsAsFile())
        {
            DBG("❌ File does not exist: " + file.getFullPathName());
            return false;
        }

        try
        {
            juce::MemoryBlock fileData;
            if (!file.loadFileAsData(fileData))
            {
                DBG("❌ Failed to load file data");
                return false;
            }

            juce::MemoryInputStream stream(fileData, false);

            // Read header and validate
            if (!readAndValidateHeader(stream))
            {
                DBG("❌ Invalid file header");
                return false;
            }

            // Read all sections
            ProjectMetadata metadata;
            if (!readMetadata(stream, metadata))
                return false;

            juce::AudioBuffer<float> audio;
            if (!readAudioData(stream, audio))
                return false;

            FeatureData features;
            if (!readFeatureData(stream, features))
                return false;

            EffectStateSnapshot effectState;
            if (!readEffectState(stream, effectState))
                return false;

            std::unique_ptr<juce::XmlElement> audioXml;
            if (!readXmlSection(stream, audioXml))
                return false;

            std::unique_ptr<juce::XmlElement> uiXml;
            if (!readXmlSection(stream, uiXml))
                return false;

            std::vector<float> thumbnail;
            if (!readThumbnail(stream, thumbnail))
                return false;

            // Set all data to project
            project.getMetadata() = metadata;
            project.setOriginalAudio(audio);
            project.setFeatureData(features);
            project.setEffectState(effectState);
            project.setAudioStateXml(std::move(audioXml));
            project.setUIStateXml(std::move(uiXml));
            project.setFilePath(file.getFullPathName());

            DBG("✅ Project loaded: " + file.getFullPathName());
            return true;
        }
        catch (const std::exception& e)
        {
            DBG("❌ Exception during load: " + juce::String(e.what()));
            return false;
        }
    }

    // ========== QUICK METADATA LOAD ==========

    static bool loadMetadataOnly(ProjectMetadata& metadata, const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;

        try
        {
            juce::MemoryBlock fileData;
            if (!file.loadFileAsData(fileData))
                return false;

            juce::MemoryInputStream stream(fileData, false);

            // Validate header
            if (!readAndValidateHeader(stream))
                return false;

            // Read only metadata section
            return readMetadata(stream, metadata);
        }
        catch (...)
        {
            return false;
        }
    }

private:
    // ========== WRITE METHODS ==========

    static void writeHeader(juce::MemoryOutputStream& stream)
    {
        stream.writeInt(MAGIC_NUMBER);
        stream.writeShort(FORMAT_VERSION);
        stream.writeInt64(juce::Time::currentTimeMillis());
    }

    static void writeMetadata(juce::MemoryOutputStream& stream, const ProjectMetadata& metadata)
    {
        stream.writeString(metadata.projectName);
        stream.writeString(metadata.projectId);
        stream.writeInt64(metadata.creationTime);
        stream.writeInt64(metadata.lastModifiedTime);
        stream.writeDouble(metadata.sampleRate);
        stream.writeInt(metadata.numSamples);
        stream.writeInt(metadata.numChannels);
        stream.writeFloat(metadata.durationSeconds);
    }

    static void writeAudioData(juce::MemoryOutputStream& stream,
        const juce::AudioBuffer<float>& audio)
    {
        int numChannels = audio.getNumChannels();
        int numSamples = audio.getNumSamples();

        stream.writeInt(numChannels);
        stream.writeInt(numSamples);

        // Write channel by channel
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* data = audio.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                stream.writeFloat(data[i]);
            }
        }
    }

    static void writeFeatureData(juce::MemoryOutputStream& stream,
        const FeatureData& features)
    {
        int numSamples = features.getNumSamples();
        stream.writeInt(numSamples);

        // Write each feature sample
        for (int i = 0; i < numSamples; ++i)
        {
            const auto& sample = features[i];

            stream.writeFloat(sample.amplitude);
            stream.writeFloat(sample.frequency);
            stream.writeFloat(sample.phase);
            stream.writeFloat(sample.volume);
            stream.writeFloat(sample.pan);
            stream.writeBool(sample.wasModified);

            // Write computation flags
            stream.writeBool(sample.frequencyComputed);
            stream.writeBool(sample.phaseComputed);
            stream.writeBool(sample.volumeComputed);
            stream.writeBool(sample.panComputed);
        }
    }

    static void writeEffectState(juce::MemoryOutputStream& stream,
        const EffectStateSnapshot& state)
    {
        stream.writeBool(state.trimActive);
        stream.writeInt(state.trimStart);
        stream.writeInt(state.trimEnd);

        stream.writeBool(state.normalizeActive);
        stream.writeFloat(state.normalizeTargetDb);
        stream.writeFloat(state.normalizeGain);

        stream.writeBool(state.reverseActive);

        stream.writeBool(state.boostActive);
        stream.writeFloat(state.boostDb);
        stream.writeFloat(state.boostGain);

        stream.writeBool(state.adsrCutItselfMode);
    }

    static void writeXmlSection(juce::MemoryOutputStream& stream,
        const juce::XmlElement* xml)
    {
        if (xml == nullptr)
        {
            stream.writeInt(0);  // No XML data
            return;
        }

        juce::String xmlString = xml->toString();
        stream.writeInt(xmlString.length());
        stream.writeString(xmlString);
    }

    static void writeThumbnail(juce::MemoryOutputStream& stream,
        const std::vector<float>& thumbnail)
    {
        stream.writeInt(static_cast<int>(thumbnail.size()));

        for (float value : thumbnail)
        {
            stream.writeFloat(value);
        }
    }

    // ========== READ METHODS ==========

    static bool readAndValidateHeader(juce::MemoryInputStream& stream)
    {
        juce::uint32 magic = stream.readInt();
        if (magic != MAGIC_NUMBER)
        {
            DBG("❌ Invalid magic number");
            return false;
        }

        juce::uint16 version = stream.readShort();
        if (version != FORMAT_VERSION)
        {
            DBG("❌ Unsupported format version: " + juce::String(version));
            return false;
        }

        juce::int64 timestamp = stream.readInt64();
        DBG("File saved at: " + juce::Time(timestamp).formatted("%Y-%m-%d %H:%M:%S"));

        return true;
    }

    static bool readMetadata(juce::MemoryInputStream& stream, ProjectMetadata& metadata)
    {
        metadata.projectName = stream.readString();
        metadata.projectId = stream.readString();
        metadata.creationTime = stream.readInt64();
        metadata.lastModifiedTime = stream.readInt64();
        metadata.sampleRate = stream.readDouble();
        metadata.numSamples = stream.readInt();
        metadata.numChannels = stream.readInt();
        metadata.durationSeconds = stream.readFloat();
        metadata.isValid = true;

        return !stream.isExhausted();
    }

    static bool readAudioData(juce::MemoryInputStream& stream,
        juce::AudioBuffer<float>& audio)
    {
        int numChannels = stream.readInt();
        int numSamples = stream.readInt();

        audio.setSize(numChannels, numSamples, false, true, false);

        // Read channel by channel
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = audio.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                data[i] = stream.readFloat();
            }
        }

        return !stream.isExhausted();
    }

    static bool readFeatureData(juce::MemoryInputStream& stream, FeatureData& features)
    {
        int numSamples = stream.readInt();
        features.setSize(numSamples);

        // Read each feature sample
        for (int i = 0; i < numSamples; ++i)
        {
            float amplitude = stream.readFloat();
            float frequency = stream.readFloat();
            float phase = stream.readFloat();
            float volume = stream.readFloat();
            float pan = stream.readFloat();
            bool wasModified = stream.readBool();

            // Read computation flags
            bool freqComputed = stream.readBool();
            bool phaseComputed = stream.readBool();
            bool volComputed = stream.readBool();
            bool panComputed = stream.readBool();

            // Set feature sample data
            features[i].amplitude = amplitude;
            features[i].frequency = frequency;
            features[i].phase = phase;
            features[i].volume = volume;
            features[i].pan = pan;
            features[i].wasModified = wasModified;
            features[i].frequencyComputed = freqComputed;
            features[i].phaseComputed = phaseComputed;
            features[i].volumeComputed = volComputed;
            features[i].panComputed = panComputed;
        }

        return !stream.isExhausted();
    }

    static bool readEffectState(juce::MemoryInputStream& stream,
        EffectStateSnapshot& state)
    {
        state.trimActive = stream.readBool();
        state.trimStart = stream.readInt();
        state.trimEnd = stream.readInt();

        state.normalizeActive = stream.readBool();
        state.normalizeTargetDb = stream.readFloat();
        state.normalizeGain = stream.readFloat();

        state.reverseActive = stream.readBool();

        state.boostActive = stream.readBool();
        state.boostDb = stream.readFloat();
        state.boostGain = stream.readFloat();

        state.adsrCutItselfMode = stream.readBool();

        return !stream.isExhausted();
    }

    static bool readXmlSection(juce::MemoryInputStream& stream,
        std::unique_ptr<juce::XmlElement>& xml)
    {
        int length = stream.readInt();

        if (length == 0)
        {
            xml.reset();
            return true;
        }

        juce::String xmlString = stream.readString();
        xml = juce::parseXML(xmlString);

        return !stream.isExhausted();
    }

    static bool readThumbnail(juce::MemoryInputStream& stream,
        std::vector<float>& thumbnail)
    {
        int numPoints = stream.readInt();
        thumbnail.reserve(numPoints);

        for (int i = 0; i < numPoints; ++i)
        {
            thumbnail.push_back(stream.readFloat());
        }

        return true;  // End of file expected here
    }
};