/*
AlgorithmDNA.h
Core data structure for storing transformation algorithms.
Captures spectral "DNA" of audio transformations.

Format:
- Metadata: name, description, author, type
- Transform data: magnitude ratios, phase deltas
- Application parameters: intensity, adaptive mode
*/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <complex>

// ==========================================================================
// ALGORITHM DNA - CORE DATA STRUCTURE
// ==========================================================================

class AlgorithmDNA
{
public:
    // ==========================================================================
    // METADATA
    // ==========================================================================
    
    struct Metadata
    {
        juce::String name = "Untitled Algorithm";
        juce::String description;
        juce::String author = "User";
        juce::String algorithmType = "difference";  // "difference", "morph", etc
        juce::Time creationDate;
        
        // Original files info (for reference)
        juce::String originalFileName;
        juce::String processedFileName;
    };
    
    // ==========================================================================
    // TRANSFORM DATA
    // ==========================================================================
    
    struct TransformData
    {
        // Spectral transform maps [frame][bin]
        std::vector<std::vector<float>> magnitudeRatios;  // processed/original
        std::vector<std::vector<float>> phaseDeltas;      // processed - original
        
        // Dimensions
        int numFrames = 0;
        int numBins = 0;
        int fftSize = 2048;
        int hopSize = 512;
        
        // Normalization data
        float originalRMS = 1.0f;
        float processedRMS = 1.0f;
        int originalSampleRate = 44100;
        
        // Frequency weighting (optional)
        std::vector<float> frequencyWeights;
        
        void clear()
        {
            magnitudeRatios.clear();
            phaseDeltas.clear();
            frequencyWeights.clear();
            numFrames = 0;
            numBins = 0;
        }
        
        bool isValid() const
        {
            return numFrames > 0 && 
                   numBins > 0 && 
                   !magnitudeRatios.empty() && 
                   !phaseDeltas.empty();
        }
    };
    
    // ==========================================================================
    // APPLICATION PARAMETERS
    // ==========================================================================
    
    struct ApplicationParams
    {
        float intensity = 1.0f;         // 0.0 - 2.0 (can overdrive!)
        bool adaptiveMode = true;       // adapt to target audio's loudness
        bool preserveDynamics = false;  // preserve original dynamics
        
        // Frequency range limiting (Hz)
        float minFrequency = 20.0f;
        float maxFrequency = 20000.0f;
        
        // Blend mode
        enum BlendMode
        {
            Replace,    // full replacement
            Add,        // additive
            Multiply,   // multiplicative
            Screen      // screen blend (like Photoshop)
        };
        
        BlendMode blendMode = Replace;
        float blendAmount = 1.0f;  // 0.0 - 1.0
    };
    
    // ==========================================================================
    // PUBLIC API
    // ==========================================================================
    
    AlgorithmDNA() 
    {
        metadata.creationDate = juce::Time::getCurrentTime();
    }
    
    // Data access
    Metadata metadata;
    TransformData transformData;
    ApplicationParams applicationParams;
    
    // Validation
    bool isValid() const
    {
        return transformData.isValid();
    }
    
    // Statistics
    struct Statistics
    {
        float averageMagnitudeBoost = 0.0f;
        float averagePhaseShift = 0.0f;
        float maxMagnitudeRatio = 0.0f;
        float minMagnitudeRatio = 0.0f;
        int totalDataPoints = 0;
    };
    
    Statistics calculateStatistics() const
    {
        Statistics stats;
        
        if (!isValid())
            return stats;
        
        stats.totalDataPoints = transformData.numFrames * transformData.numBins;
        
        float sumMagRatio = 0.0f;
        float sumPhaseShift = 0.0f;
        stats.maxMagnitudeRatio = 0.0f;
        stats.minMagnitudeRatio = std::numeric_limits<float>::max();
        
        for (int frame = 0; frame < transformData.numFrames; ++frame)
        {
            for (int bin = 0; bin < transformData.numBins; ++bin)
            {
                float magRatio = transformData.magnitudeRatios[frame][bin];
                float phaseShift = transformData.phaseDeltas[frame][bin];
                
                sumMagRatio += magRatio;
                sumPhaseShift += std::abs(phaseShift);
                
                stats.maxMagnitudeRatio = std::max(stats.maxMagnitudeRatio, magRatio);
                stats.minMagnitudeRatio = std::min(stats.minMagnitudeRatio, magRatio);
            }
        }
        
        stats.averageMagnitudeBoost = sumMagRatio / stats.totalDataPoints;
        stats.averagePhaseShift = sumPhaseShift / stats.totalDataPoints;
        
        return stats;
    }
    
    // Serialization
    juce::var toJSON() const
    {
        auto* json = new juce::DynamicObject();
        
        // Metadata
        auto* metaObj = new juce::DynamicObject();
        metaObj->setProperty("name", metadata.name);
        metaObj->setProperty("description", metadata.description);
        metaObj->setProperty("author", metadata.author);
        metaObj->setProperty("algorithmType", metadata.algorithmType);
        metaObj->setProperty("creationDate", metadata.creationDate.toISO8601(true));
        metaObj->setProperty("originalFileName", metadata.originalFileName);
        metaObj->setProperty("processedFileName", metadata.processedFileName);
        json->setProperty("metadata", metaObj);
        
        // Transform data dimensions
        auto* dataObj = new juce::DynamicObject();
        dataObj->setProperty("numFrames", transformData.numFrames);
        dataObj->setProperty("numBins", transformData.numBins);
        dataObj->setProperty("fftSize", transformData.fftSize);
        dataObj->setProperty("hopSize", transformData.hopSize);
        dataObj->setProperty("originalRMS", transformData.originalRMS);
        dataObj->setProperty("processedRMS", transformData.processedRMS);
        dataObj->setProperty("originalSampleRate", transformData.originalSampleRate);
        json->setProperty("transformData", dataObj);
        
        // Application params
        auto* paramsObj = new juce::DynamicObject();
        paramsObj->setProperty("intensity", applicationParams.intensity);
        paramsObj->setProperty("adaptiveMode", applicationParams.adaptiveMode);
        paramsObj->setProperty("preserveDynamics", applicationParams.preserveDynamics);
        paramsObj->setProperty("minFrequency", applicationParams.minFrequency);
        paramsObj->setProperty("maxFrequency", applicationParams.maxFrequency);
        paramsObj->setProperty("blendMode", static_cast<int>(applicationParams.blendMode));
        paramsObj->setProperty("blendAmount", applicationParams.blendAmount);
        json->setProperty("applicationParams", paramsObj);
        
        return juce::var(json);
    }
    
    void fromJSON(const juce::var& json)
    {
        if (!json.isObject())
            return;
        
        // Metadata
        auto metaVar = json.getProperty("metadata", juce::var());
        if (metaVar.isObject())
        {
            metadata.name = metaVar.getProperty("name", "Untitled");
            metadata.description = metaVar.getProperty("description", "");
            metadata.author = metaVar.getProperty("author", "User");
            metadata.algorithmType = metaVar.getProperty("algorithmType", "difference");
            metadata.originalFileName = metaVar.getProperty("originalFileName", "");
            metadata.processedFileName = metaVar.getProperty("processedFileName", "");
            
            juce::String dateStr = metaVar.getProperty("creationDate", "");
            if (dateStr.isNotEmpty())
                metadata.creationDate = juce::Time::fromISO8601(dateStr);
        }
        
        // Transform data dimensions
        auto dataVar = json.getProperty("transformData", juce::var());
        if (dataVar.isObject())
        {
            transformData.numFrames = dataVar.getProperty("numFrames", 0);
            transformData.numBins = dataVar.getProperty("numBins", 0);
            transformData.fftSize = dataVar.getProperty("fftSize", 2048);
            transformData.hopSize = dataVar.getProperty("hopSize", 512);
            transformData.originalRMS = dataVar.getProperty("originalRMS", 1.0f);
            transformData.processedRMS = dataVar.getProperty("processedRMS", 1.0f);
            transformData.originalSampleRate = dataVar.getProperty("originalSampleRate", 44100);
        }
        
        // Application params
        auto paramsVar = json.getProperty("applicationParams", juce::var());
        if (paramsVar.isObject())
        {
            applicationParams.intensity = paramsVar.getProperty("intensity", 1.0f);
            applicationParams.adaptiveMode = paramsVar.getProperty("adaptiveMode", true);
            applicationParams.preserveDynamics = paramsVar.getProperty("preserveDynamics", false);
            applicationParams.minFrequency = paramsVar.getProperty("minFrequency", 20.0f);
            applicationParams.maxFrequency = paramsVar.getProperty("maxFrequency", 20000.0f);
            applicationParams.blendMode = static_cast<ApplicationParams::BlendMode>(
                static_cast<int>(paramsVar.getProperty("blendMode", 0)));
            applicationParams.blendAmount = paramsVar.getProperty("blendAmount", 1.0f);
        }
    }
    
    // Binary data save/load (magnitude ratios and phase deltas)
    bool saveBinaryData(const juce::File& file) const
    {
        if (!isValid())
            return false;
        
        juce::FileOutputStream stream(file);
        if (!stream.openedOk())
            return false;
        
        // Write dimensions
        stream.writeInt(transformData.numFrames);
        stream.writeInt(transformData.numBins);
        
        // Write magnitude ratios
        for (const auto& frame : transformData.magnitudeRatios)
        {
            for (float value : frame)
            {
                stream.writeFloat(value);
            }
        }
        
        // Write phase deltas
        for (const auto& frame : transformData.phaseDeltas)
        {
            for (float value : frame)
            {
                stream.writeFloat(value);
            }
        }
        
        return true;
    }
    
    bool loadBinaryData(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;
        
        juce::FileInputStream stream(file);
        if (!stream.openedOk())
            return false;
        
        // Read dimensions
        int numFrames = stream.readInt();
        int numBins = stream.readInt();
        
        if (numFrames <= 0 || numBins <= 0)
            return false;
        
        // Prepare storage
        transformData.numFrames = numFrames;
        transformData.numBins = numBins;
        transformData.magnitudeRatios.resize(numFrames);
        transformData.phaseDeltas.resize(numFrames);
        
        // Read magnitude ratios
        for (int frame = 0; frame < numFrames; ++frame)
        {
            transformData.magnitudeRatios[frame].resize(numBins);
            for (int bin = 0; bin < numBins; ++bin)
            {
                transformData.magnitudeRatios[frame][bin] = stream.readFloat();
            }
        }
        
        // Read phase deltas
        for (int frame = 0; frame < numFrames; ++frame)
        {
            transformData.phaseDeltas[frame].resize(numBins);
            for (int bin = 0; bin < numBins; ++bin)
            {
                transformData.phaseDeltas[frame][bin] = stream.readFloat();
            }
        }
        
        return true;
    }
    
private:
    JUCE_LEAK_DETECTOR(AlgorithmDNA)
};