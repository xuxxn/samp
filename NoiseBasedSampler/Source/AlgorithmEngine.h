/*
AlgorithmEngine.h
Applies AlgorithmDNA transformations to new audio.

Process:
1. STFT on input audio
2. Apply magnitude ratios and phase deltas with intensity
3. Adaptive RMS matching (optional)
4. Inverse STFT to reconstruct audio
*/

#pragma once
#include <JuceHeader.h>
#include "AlgorithmDNA.h"
#include <vector>
#include <complex>

// ==========================================================================
// ALGORITHM ENGINE
// ==========================================================================

class AlgorithmEngine
{
public:
    AlgorithmEngine() : fft(fftOrder)
    {
        window.resize(fftSize);
        createHannWindow();
    }
    
    // ==========================================================================
    // MAIN APPLICATION
    // ==========================================================================
    
    void applyAlgorithm(const juce::AudioBuffer<float>& input,
                       juce::AudioBuffer<float>& output,
                       const AlgorithmDNA& algo,
                       float intensityOverride = -1.0f)
    {
        if (!algo.isValid())
        {
            DBG("❌ Invalid algorithm DNA");
            output.makeCopyOf(input);
            return;
        }
        
        float intensity = (intensityOverride >= 0.0f) ? 
            intensityOverride : algo.applicationParams.intensity;
        
        DBG("===========================================");
        DBG("🎨 APPLYING ALGORITHM: " + algo.metadata.name);
        DBG("===========================================");
        DBG("Input samples: " + juce::String(input.getNumSamples()));
        DBG("Intensity: " + juce::String(intensity, 2));
        DBG("Adaptive mode: " + juce::String(algo.applicationParams.adaptiveMode ? "ON" : "OFF"));
        
        // 1. Perform STFT on input
        auto inputSpectrum = performSTFT(input);
        
        if (inputSpectrum.empty())
        {
            DBG("❌ Failed to perform STFT");
            output.makeCopyOf(input);
            return;
        }
        
        // 2. Apply transformation
        applyTransform(inputSpectrum, algo, intensity);
        
        // 3. Inverse STFT
        performISTFT(inputSpectrum, output, input.getNumSamples());
        
        // 4. Adaptive RMS matching
        if (algo.applicationParams.adaptiveMode)
        {
            matchRMS(input, output);
        }
        
        // 5. Ensure stereo
        if (output.getNumChannels() < input.getNumChannels())
        {
            int targetChannels = input.getNumChannels();
            juce::AudioBuffer<float> stereoOutput(targetChannels, output.getNumSamples());
            
            for (int ch = 0; ch < targetChannels; ++ch)
            {
                stereoOutput.copyFrom(ch, 0, output, 0, 0, output.getNumSamples());
            }
            
            output = stereoOutput;
        }
        
        DBG("✅ Algorithm applied successfully");
        DBG("===========================================");
    }
    
private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;
    
    juce::dsp::FFT fft;
    std::vector<float> window;
    
    void createHannWindow()
    {
        for (int i = 0; i < fftSize; ++i)
        {
            window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
        }
    }
    
    // ==========================================================================
    // STFT
    // ==========================================================================
    
    std::vector<std::vector<std::complex<float>>> performSTFT(const juce::AudioBuffer<float>& audio)
    {
        std::vector<std::vector<std::complex<float>>> spectrum;
        
        int numSamples = audio.getNumSamples();
        int numFrames = (numSamples - fftSize) / hopSize + 1;
        
        if (numFrames <= 0)
            return spectrum;
        
        spectrum.reserve(numFrames);
        
        std::vector<float> fftData(fftSize * 2, 0.0f);
        const auto* audioData = audio.getReadPointer(0);
        
        for (int frame = 0; frame < numFrames; ++frame)
        {
            int startSample = frame * hopSize;
            
            for (int i = 0; i < fftSize; ++i)
            {
                int sampleIdx = startSample + i;
                if (sampleIdx < numSamples)
                    fftData[i] = audioData[sampleIdx] * window[i];
                else
                    fftData[i] = 0.0f;
            }
            
            for (int i = fftSize; i < fftSize * 2; ++i)
                fftData[i] = 0.0f;
            
            fft.performFrequencyOnlyForwardTransform(fftData.data());
            
            std::vector<std::complex<float>> frameSpectrum;
            frameSpectrum.reserve(fftSize / 2);
            
            for (int bin = 0; bin < fftSize / 2; ++bin)
            {
                float real = fftData[bin * 2];
                float imag = fftData[bin * 2 + 1];
                
                frameSpectrum.emplace_back(real, imag);
            }
            
            spectrum.push_back(frameSpectrum);
        }
        
        return spectrum;
    }
    
    // ==========================================================================
    // TRANSFORM APPLICATION
    // ==========================================================================
    
    void applyTransform(std::vector<std::vector<std::complex<float>>>& spectrum,
                       const AlgorithmDNA& algo,
                       float intensity)
    {
        int inputFrames = static_cast<int>(spectrum.size());
        int inputBins = static_cast<int>(spectrum[0].size());
        
        int algoFrames = algo.transformData.numFrames;
        int algoBins = algo.transformData.numBins;
        
        for (int frame = 0; frame < inputFrames; ++frame)
        {
            // Temporal interpolation (stretch/shrink algorithm to match input length)
            float algoFramePos = (frame / static_cast<float>(inputFrames)) * algoFrames;
            int algoFrame = static_cast<int>(algoFramePos);
            algoFrame = juce::jlimit(0, algoFrames - 1, algoFrame);
            
            for (int bin = 0; bin < inputBins; ++bin)
            {
                // Frequency interpolation (if different bin counts)
                int algoBin = (bin * algoBins) / inputBins;
                algoBin = juce::jlimit(0, algoBins - 1, algoBin);
                
                // Get transform values
                float magRatio = algo.transformData.magnitudeRatios[algoFrame][algoBin];
                float phaseDelta = algo.transformData.phaseDeltas[algoFrame][algoBin];
                
                // Apply with intensity
                float appliedRatio = 1.0f + (magRatio - 1.0f) * intensity;
                float appliedPhaseDelta = phaseDelta * intensity;
                
                // Transform complex value
                auto& complexValue = spectrum[frame][bin];
                float mag = std::abs(complexValue);
                float phase = std::arg(complexValue);
                
                // Apply transformation
                float newMag = mag * appliedRatio;
                float newPhase = phase + appliedPhaseDelta;
                
                // Reconstruct complex value
                spectrum[frame][bin] = std::polar(newMag, newPhase);
            }
        }
    }
    
    // ==========================================================================
    // INVERSE STFT
    // ==========================================================================
    
    void performISTFT(const std::vector<std::vector<std::complex<float>>>& spectrum,
                     juce::AudioBuffer<float>& output,
                     int targetLength)
    {
        int numFrames = static_cast<int>(spectrum.size());
        
        // Prepare output buffer
        output.setSize(1, targetLength + fftSize, false, true, false);
        output.clear();
        
        std::vector<float> windowAccum(targetLength + fftSize, 0.0f);
        std::vector<float> fftData(fftSize * 2, 0.0f);
        
        for (int frame = 0; frame < numFrames; ++frame)
        {
            int startSample = frame * hopSize;
            
            // Prepare FFT data
            std::fill(fftData.begin(), fftData.end(), 0.0f);
            
            for (int bin = 0; bin < fftSize / 2; ++bin)
            {
                const auto& complexValue = spectrum[frame][bin];
                fftData[bin * 2] = complexValue.real();
                fftData[bin * 2 + 1] = complexValue.imag();
            }
            
            // Inverse FFT
            fft.performRealOnlyInverseTransform(fftData.data());
            
            // Overlap-add with windowing
            auto* outputData = output.getWritePointer(0);
            
            for (int i = 0; i < fftSize; ++i)
            {
                int outputIdx = startSample + i;
                if (outputIdx < targetLength + fftSize)
                {
                    float windowValue = window[i];
                    outputData[outputIdx] += fftData[i] * windowValue;
                    windowAccum[outputIdx] += windowValue * windowValue;
                }
            }
        }
        
        // Normalize by window accumulation
        auto* outputData = output.getWritePointer(0);
        
        for (int i = 0; i < targetLength; ++i)
        {
            if (windowAccum[i] > 0.001f)
                outputData[i] /= windowAccum[i];
        }
        
        // Trim to target length
        if (output.getNumSamples() > targetLength)
        {
            juce::AudioBuffer<float> trimmed(1, targetLength);
            trimmed.copyFrom(0, 0, output, 0, 0, targetLength);
            output = trimmed;
        }
    }
    
    // ==========================================================================
    // RMS MATCHING
    // ==========================================================================
    
    void matchRMS(const juce::AudioBuffer<float>& reference,
                 juce::AudioBuffer<float>& target)
    {
        float refRMS = reference.getRMSLevel(0, 0, reference.getNumSamples());
        float targetRMS = target.getRMSLevel(0, 0, target.getNumSamples());
        
        if (targetRMS < 0.00001f)
            return;
        
        float gain = refRMS / targetRMS;
        gain = juce::jlimit(0.1f, 10.0f, gain);  // Safety limits
        
        target.applyGain(gain);
        
        DBG("RMS matching applied: " + juce::String(gain, 3) + "x");
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmEngine)
};