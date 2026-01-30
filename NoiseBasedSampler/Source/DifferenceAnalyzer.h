/*
DifferenceAnalyzer.h
Analyzes the spectral difference between two audio files.
Creates AlgorithmDNA that captures the transformation.

Process:
1. Load original and processed audio
2. Perform STFT on both
3. Calculate magnitude ratios and phase deltas
4. Store as AlgorithmDNA
*/

#pragma once
#include <JuceHeader.h>
#include "AlgorithmDNA.h"
#include <vector>
#include <complex>

// ==========================================================================
// DIFFERENCE ANALYZER
// ==========================================================================

class DifferenceAnalyzer
{
public:
    DifferenceAnalyzer() : fft(fftOrder)
    {
        window.resize(fftSize);
        createHannWindow();
    }
    
    // ==========================================================================
    // MAIN ANALYSIS
    // ==========================================================================
    
    AlgorithmDNA analyze(const juce::AudioBuffer<float>& original,
                        const juce::AudioBuffer<float>& processed,
                        double sampleRate)
    {
        AlgorithmDNA algo;
        
        if (original.getNumSamples() == 0 || processed.getNumSamples() == 0)
        {
            DBG("❌ DifferenceAnalyzer: Empty audio buffers");
            return algo;
        }
        
        DBG("===========================================");
        DBG("🔬 DIFFERENCE ANALYSIS STARTED");
        DBG("===========================================");
        DBG("Original samples: " + juce::String(original.getNumSamples()));
        DBG("Processed samples: " + juce::String(processed.getNumSamples()));
        
        // 1. Perform STFT on both
        auto originalSpectrum = performSTFT(original);
        auto processedSpectrum = performSTFT(processed);
        
        int numFrames = std::min(originalSpectrum.size(), processedSpectrum.size());
        if (numFrames == 0)
        {
            DBG("❌ No frames to analyze");
            return algo;
        }
        
        int numBins = fftSize / 2;
        
        DBG("Frames: " + juce::String(numFrames));
        DBG("Bins: " + juce::String(numBins));
        
        // 2. Calculate ratios and deltas
        algo.transformData.numFrames = numFrames;
        algo.transformData.numBins = numBins;
        algo.transformData.fftSize = fftSize;
        algo.transformData.hopSize = hopSize;
        algo.transformData.originalSampleRate = static_cast<int>(sampleRate);
        
        algo.transformData.magnitudeRatios.resize(numFrames);
        algo.transformData.phaseDeltas.resize(numFrames);
        
        float sumOriginalEnergy = 0.0f;
        float sumProcessedEnergy = 0.0f;
        
        for (int frame = 0; frame < numFrames; ++frame)
        {
            algo.transformData.magnitudeRatios[frame].resize(numBins);
            algo.transformData.phaseDeltas[frame].resize(numBins);
            
            for (int bin = 0; bin < numBins; ++bin)
            {
                float origMag = originalSpectrum[frame][bin].real();
                float origPhase = originalSpectrum[frame][bin].imag();
                
                float procMag = processedSpectrum[frame][bin].real();
                float procPhase = processedSpectrum[frame][bin].imag();
                
                // Magnitude ratio (avoid division by zero)
                const float MIN_MAG = 0.00001f;
                float ratio = procMag / std::max(origMag, MIN_MAG);
                
                // Clamp extreme ratios
                ratio = juce::jlimit(0.01f, 100.0f, ratio);
                
                algo.transformData.magnitudeRatios[frame][bin] = ratio;
                
                // Phase delta (normalized to -π to π)
                float phaseDelta = procPhase - origPhase;
                
                // Wrap to [-π, π]
                while (phaseDelta > juce::MathConstants<float>::pi)
                    phaseDelta -= juce::MathConstants<float>::twoPi;
                while (phaseDelta < -juce::MathConstants<float>::pi)
                    phaseDelta += juce::MathConstants<float>::twoPi;
                
                algo.transformData.phaseDeltas[frame][bin] = phaseDelta;
                
                // Accumulate energy
                sumOriginalEnergy += origMag * origMag;
                sumProcessedEnergy += procMag * procMag;
            }
        }
        
        // 3. Calculate RMS
        float totalPoints = numFrames * numBins;
        algo.transformData.originalRMS = std::sqrt(sumOriginalEnergy / totalPoints);
        algo.transformData.processedRMS = std::sqrt(sumProcessedEnergy / totalPoints);
        
        // 4. Calculate statistics
        auto stats = algo.calculateStatistics();
        
        DBG("-------------------------------------------");
        DBG("✅ ANALYSIS COMPLETE");
        DBG("Average magnitude boost: " + juce::String(stats.averageMagnitudeBoost, 3));
        DBG("Average phase shift: " + juce::String(stats.averagePhaseShift, 3));
        DBG("Original RMS: " + juce::String(algo.transformData.originalRMS, 6));
        DBG("Processed RMS: " + juce::String(algo.transformData.processedRMS, 6));
        DBG("===========================================");
        
        return algo;
    }
    
    // ==========================================================================
    // STFT PROCESSING
    // ==========================================================================
    
private:
    static constexpr int fftOrder = 11;        // 2^11 = 2048
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 4;  // 75% overlap
    
    juce::dsp::FFT fft;
    std::vector<float> window;
    
    void createHannWindow()
    {
        for (int i = 0; i < fftSize; ++i)
        {
            window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
        }
    }
    
    // Perform STFT on audio buffer
    // Returns: [frame][bin] -> complex<float>(magnitude, phase)
    std::vector<std::vector<std::complex<float>>> performSTFT(const juce::AudioBuffer<float>& audio)
    {
        std::vector<std::vector<std::complex<float>>> spectrum;
        
        int numSamples = audio.getNumSamples();
        int numFrames = (numSamples - fftSize) / hopSize + 1;
        
        if (numFrames <= 0)
            return spectrum;
        
        spectrum.reserve(numFrames);
        
        std::vector<float> fftData(fftSize * 2, 0.0f);
        
        // Use first channel (mono or left)
        const auto* audioData = audio.getReadPointer(0);
        
        for (int frame = 0; frame < numFrames; ++frame)
        {
            int startSample = frame * hopSize;
            
            // Fill FFT buffer with windowed data
            for (int i = 0; i < fftSize; ++i)
            {
                int sampleIdx = startSample + i;
                if (sampleIdx < numSamples)
                    fftData[i] = audioData[sampleIdx] * window[i];
                else
                    fftData[i] = 0.0f;
            }
            
            // Clear imaginary part
            for (int i = fftSize; i < fftSize * 2; ++i)
                fftData[i] = 0.0f;
            
            // Perform FFT
            fft.performFrequencyOnlyForwardTransform(fftData.data());
            
            // Extract magnitude and phase for each bin
            std::vector<std::complex<float>> frameSpectrum;
            frameSpectrum.reserve(fftSize / 2);
            
            for (int bin = 0; bin < fftSize / 2; ++bin)
            {
                float real = fftData[bin * 2];
                float imag = fftData[bin * 2 + 1];
                
                float magnitude = std::sqrt(real * real + imag * imag);
                float phase = std::atan2(imag, real);
                
                // Store as complex (magnitude, phase)
                frameSpectrum.emplace_back(magnitude, phase);
            }
            
            spectrum.push_back(frameSpectrum);
        }
        
        return spectrum;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DifferenceAnalyzer)
};