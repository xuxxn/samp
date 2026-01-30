/*
SimpleIndexExporter.h
ОПТИМИЗИРОВАННЫЙ экспорт индексов в текстовый формат с ASYNC поддержкой.
Не блокирует DAW!
*/

#pragma once
#include <JuceHeader.h>
#include "SpectralIndexData.h"
#include "FeatureData.h"

class SimpleIndexExporter
{
public:
    // ==========================================================================
    // ASYNC ЭКСПОРТ (не блокирует DAW!)
    // ==========================================================================

    static void exportAllAsync(
        const juce::File& baseFile,
        const juce::AudioBuffer<float>& originalAudio,
        const FeatureData& features,
        const SpectralIndexData& indices,
        double sampleRate,
        std::function<void(bool success, juce::String message)> callback)
    {
        // Копируем данные для безопасной передачи в поток
        auto audioBufferCopy = std::make_shared<juce::AudioBuffer<float>>(originalAudio);
        auto featuresCopy = std::make_shared<FeatureData>(features);
        auto indicesCopy = std::make_shared<SpectralIndexData>(indices);

        // Запускаем в фоновом потоке
        juce::Thread::launch([baseFile, audioBufferCopy, featuresCopy, indicesCopy, sampleRate, callback]()
            {
                DBG("===========================================");
                DBG("ASYNC EXPORT STARTED");
                DBG("===========================================");

                bool success = exportAllSync(baseFile, *audioBufferCopy, *featuresCopy, *indicesCopy, sampleRate);

                // Вызываем callback в message thread
                juce::MessageManager::callAsync([callback, success, baseFile]()
                    {
                        if (success)
                        {
                            callback(true, "Export completed successfully!");
                        }
                        else
                        {
                            callback(false, "Export failed!");
                        }
                    });
            });
    }

private:
    // ==========================================================================
    // SYNC ЭКСПОРТ (вызывается из фонового потока)
    // ==========================================================================

    static bool exportAllSync(
        const juce::File& baseFile,
        const juce::AudioBuffer<float>& originalAudio,
        const FeatureData& features,
        const SpectralIndexData& indices,
        double sampleRate)
    {
        juce::String baseName = baseFile.getFileNameWithoutExtension();
        juce::File directory = baseFile.getParentDirectory();

        DBG("Base name: " + baseName);
        DBG("Directory: " + directory.getFullPathName());

        bool success = true;

        // 1. Simple features (быстро)
        juce::File simpleFile = directory.getChildFile(baseName + "_simple_features.csv");
        success &= exportSimpleFeaturesOptimized(simpleFile, features, sampleRate);

        // 2. Frame features (очень быстро)
        juce::File frameFile = directory.getChildFile(baseName + "_frame_features.csv");
        success &= exportFrameFeaturesOptimized(frameFile, indices);

        // 3. Spectral indices (ДОЛГО - но оптимизировано!)
        juce::File spectralFile = directory.getChildFile(baseName + "_spectral_indices.csv");
        success &= exportSpectralIndicesOptimized(spectralFile, indices);

        // 4. Metadata JSON (быстро)
        juce::File jsonFile = directory.getChildFile(baseName + "_metadata.json");
        success &= exportMetadataJSON(jsonFile, originalAudio, indices, sampleRate);

        if (success)
        {
            DBG("===========================================");
            DBG("✅ ALL FILES EXPORTED!");
            DBG("===========================================");

            DBG("Files created:");
            DBG("  " + simpleFile.getFileName() + " (" +
                juce::String(simpleFile.getSize() / 1024) + " KB)");
            DBG("  " + frameFile.getFileName() + " (" +
                juce::String(frameFile.getSize() / 1024) + " KB)");
            DBG("  " + spectralFile.getFileName() + " (" +
                juce::String(spectralFile.getSize() / 1024 / 1024) + " MB)");
            DBG("  " + jsonFile.getFileName() + " (" +
                juce::String(jsonFile.getSize() / 1024) + " KB)");
        }

        return success;
    }

    // ==========================================================================
    // ОПТИМИЗИРОВАННЫЕ МЕТОДЫ ЭКСПОРТА
    // ==========================================================================

    static bool exportSimpleFeaturesOptimized(
        const juce::File& outputFile,
        const FeatureData& features,
        double sampleRate)
    {
        DBG("Exporting simple features...");

        juce::MemoryOutputStream stream;

        // Header
        stream << "sample,time_sec,amplitude,frequency_hz,phase_rad\n";

        int numSamples = features.getNumSamples();

        // Pre-allocate buffer для строки
        juce::String line;
        line.preallocateBytes(100);

        for (int i = 0; i < numSamples; ++i)
        {
            float time = i / static_cast<float>(sampleRate);

            line.clear();
            line << i << ","
                << juce::String(time, 6) << ","
                << juce::String(features[i].amplitude, 8) << ","
                << juce::String(features[i].frequency, 2) << ","
                << juce::String(features[i].phase, 8) << "\n";

            stream << line;

            // Progress
            if (i % 20000 == 0 && i > 0)
            {
                DBG("  Simple features: " + juce::String(i * 100 / numSamples) + "%");
            }
        }

        bool success = outputFile.replaceWithData(stream.getData(), stream.getDataSize());

        if (success)
        {
            DBG("✅ Simple features exported (" +
                juce::String(outputFile.getSize() / 1024) + " KB)");
        }

        return success;
    }

    static bool exportFrameFeaturesOptimized(
        const juce::File& outputFile,
        const SpectralIndexData& indices)
    {
        DBG("Exporting frame features...");

        juce::MemoryOutputStream stream;

        stream << "frame,time_sec,rms_energy,spectral_centroid_hz,spectral_spread_hz,zero_crossing_rate\n";

        int numFrames = indices.getNumFrames();
        juce::String line;
        line.preallocateBytes(150);

        for (int f = 0; f < numFrames; ++f)
        {
            const auto& frame = indices.getFrame(f);

            line.clear();
            line << f << ","
                << juce::String(frame.timePosition, 6) << ","
                << juce::String(frame.rmsEnergy, 8) << ","
                << juce::String(frame.spectralCentroid, 2) << ","
                << juce::String(frame.spectralSpread, 2) << ","
                << juce::String(frame.zeroCrossingRate, 8) << "\n";

            stream << line;
        }

        bool success = outputFile.replaceWithData(stream.getData(), stream.getDataSize());

        if (success)
        {
            DBG("✅ Frame features exported (" +
                juce::String(outputFile.getSize() / 1024) + " KB)");
        }

        return success;
    }

    static bool exportSpectralIndicesOptimized(
        const juce::File& outputFile,
        const SpectralIndexData& indices)
    {
        DBG("Exporting spectral indices (this may take a while)...");

        int numFrames = indices.getNumFrames();
        int numBins = indices.getNumBins();

        DBG("  Frames: " + juce::String(numFrames));
        DBG("  Bins per frame: " + juce::String(numBins));
        DBG("  Total indices: " + juce::String(numFrames * numBins));

        // Используем FileOutputStream для прямой записи на диск
        // (без загрузки всего в память)
        std::unique_ptr<juce::FileOutputStream> fileStream(outputFile.createOutputStream());

        if (!fileStream)
        {
            DBG("❌ Failed to create output stream");
            return false;
        }

        juce::MemoryOutputStream buffer;
        // buffer.setSize(1024 * 1024); // удалить эту строку

        // Header
        buffer << "frame,time_sec,bin,frequency_hz,magnitude,phase_rad,transient,peak\n";

        juce::String line;
        line.preallocateBytes(150);

        int progressInterval = juce::jmax(1, numFrames / 20); // 5% шаги

        for (int f = 0; f < numFrames; ++f)
        {
            const auto& frame = indices.getFrame(f);

            for (int b = 0; b < numBins; ++b)
            {
                const auto& index = frame.indices[b];
                float freq = indices.getBinFrequency(b);

                line.clear();
                line << f << ","
                    << juce::String(frame.timePosition, 6) << ","
                    << b << ","
                    << juce::String(freq, 2) << ","
                    << juce::String(index.magnitude, 8) << ","
                    << juce::String(index.phase, 8) << ","
                    << (index.isTransient ? "1" : "0") << ","
                    << (index.isPeak ? "1" : "0") << "\n";

                buffer << line;

                // Flush buffer каждые 1MB
                if (buffer.getDataSize() > 1024 * 1024)
                {
                    fileStream->write(buffer.getData(), buffer.getDataSize());
                    buffer.reset();
                }
            }

            // Progress каждые 5%
            if (f % progressInterval == 0 && f > 0)
            {
                int percent = (f * 100) / numFrames;
                DBG("  Spectral indices: " + juce::String(percent) + "%");
            }
        }

        // Flush остаток
        if (buffer.getDataSize() > 0)
        {
            fileStream->write(buffer.getData(), buffer.getDataSize());
        }

        fileStream->flush();
        fileStream.reset();

        DBG("✅ Spectral indices exported (" +
            juce::String(outputFile.getSize() / 1024 / 1024) + " MB)");

        return true;
    }

    static bool exportMetadataJSON(
        const juce::File& outputFile,
        const juce::AudioBuffer<float>& audio,
        const SpectralIndexData& indices,
        double sampleRate)
    {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();

        // Metadata
        juce::DynamicObject::Ptr meta = new juce::DynamicObject();
        meta->setProperty("sample_rate", sampleRate);
        meta->setProperty("duration_sec", audio.getNumSamples() / sampleRate);
        meta->setProperty("num_samples", audio.getNumSamples());
        meta->setProperty("fft_size", indices.getParams().fftSize);
        meta->setProperty("hop_size", indices.getParams().hopSize);
        meta->setProperty("num_frames", indices.getNumFrames());
        meta->setProperty("num_bins", indices.getNumBins());
        meta->setProperty("bin_width_hz", indices.getBinWidth());
        root->setProperty("metadata", meta.get());

        // Statistics
        auto stats = indices.calculateStatistics();
        juce::DynamicObject::Ptr statsObj = new juce::DynamicObject();
        statsObj->setProperty("max_magnitude", stats.maxMagnitude);
        statsObj->setProperty("avg_magnitude", stats.avgMagnitude);
        statsObj->setProperty("total_indices", stats.totalIndices);
        statsObj->setProperty("transient_count", stats.transientCount);
        statsObj->setProperty("peak_count", stats.peakCount);
        root->setProperty("statistics", statsObj.get());

        // Files
        root->setProperty("csv_files", juce::var(juce::Array<juce::var>{
            "simple_features.csv",
                "spectral_indices.csv",
                "frame_features.csv"
        }));

        // Save
        juce::var jsonVar(root.get());
        juce::String jsonString = juce::JSON::toString(jsonVar, true);

        return outputFile.replaceWithText(jsonString);
    }
};