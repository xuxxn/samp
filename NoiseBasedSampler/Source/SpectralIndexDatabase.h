/*
SpectralIndexDatabase.h
База данных спектральных индексов с многоуровневым кэшированием и Adaptive LOD.
Функции:

analyzeSample: анализирует AudioBuffer и создает overview индексы (FFT 512, всегда в памяти)
getOverviewIndices: возвращает overview индексы (быстрый доступ, всегда доступны)
getDetailedIndices: загружает/возвращает детальные индексы для региона (Medium/Maximum разрешение)
Кэширование: LRU cache для детальных индексов (максимум 10 регионов)
exportAllIndicesForML: экспорт overview + всех кэшированных детальных данных для AI
exportRegionForPatternDetection: специализированный экспорт для PatternDetector
getStatistics: memory usage, количество кэшированных регионов, transients/peaks
clearCache: очистка кэша детальных индексов
Thread-safe: CriticalSection для всех операций

Контекст и связи:

Хранится в PluginProcessor как indexDatabase
Использует SpectralIndexAnalyzer для создания индексов
Предоставляет данные для SpectralIndexPanel (визуализация)
Предоставляет данные для PatternDetector (улучшенный поиск)
Будущее: AI анализ через exportAllIndicesForML
УМНАЯ ОПТИМИЗАЦИЯ:

Overview (512 FFT) = ~50KB для 3 сек семпла
Maximum (8192 FFT) для региона = ~500KB
Кэш автоматически удаляет старые регионы (LRU)


Lasso selection workflow: user selects region → getDetailedIndices loads maximum resolution → caches for future use
*/

#pragma once
#include <JuceHeader.h>
#include "SpectralIndexData.h"
#include "SpectralIndexAnalyzer.h"
#include <map>
#include <memory>

// ==========================================================================
// БАЗА ДАННЫХ СПЕКТРАЛЬНЫХ ИНДЕКСОВ
// Управляет многоуровневым кэшем индексов для AI анализа
// ==========================================================================

class SpectralIndexDatabase
{
public:
    SpectralIndexDatabase() = default;
    
    // ==========================================================================
    // ИНИЦИАЛИЗАЦИЯ
    // ==========================================================================
    
    // Анализировать семпл и создать overview индексы (всегда в памяти)
    void analyzeSample(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        const juce::ScopedLock sl(lock);
        
        currentSampleRate = sampleRate;
        originalBuffer = buffer;
        
        // Очищаем предыдущие данные
        overviewIndices.reset();
        mediumCache.clear();
        maximumCache.clear();
        
        DBG("===========================================");
        DBG("CREATING SPECTRAL INDEX DATABASE");
        DBG("===========================================");
        
        // Создаём overview индексы (всегда в памяти)
        overviewIndices = std::make_unique<SpectralIndexData>(
            analyzer.analyzeWithResolution(buffer, sampleRate, IndexResolution::Overview)
        );
        
        auto stats = overviewIndices->calculateStatistics();
        
        DBG("Overview indices created:");
        DBG("  Resolution: " + juce::String(overviewIndices->getNumFrames()) + " frames × " + 
            juce::String(overviewIndices->getNumBins()) + " bins");
        DBG("  Total indices: " + juce::String(stats.totalIndices));
        DBG("  Memory: ~" + juce::String(stats.totalIndices * 4 / 1024) + " KB");
        
        sampleLoaded = true;
    }
    
    // ==========================================================================
    // ДОСТУП К ИНДЕКСАМ
    // ==========================================================================
    
    // Получить overview индексы (всегда доступны)
    const SpectralIndexData* getOverviewIndices() const
    {
        return overviewIndices.get();
    }
    
    // Получить детальные индексы для региона (загружаются по требованию)
    const SpectralIndexData* getDetailedIndices(
        const SpectralIndexData::Region& region,
        IndexResolution resolution = IndexResolution::Maximum)
    {
        const juce::ScopedLock sl(lock);

        if (!sampleLoaded || !originalBuffer.getNumSamples())
            return nullptr;

        // Генерируем ключ для кэша
        juce::String cacheKey = generateCacheKey(region, resolution);

        // Проверяем кэш
        auto& cache = (resolution == IndexResolution::Medium) ? mediumCache : maximumCache;

        auto it = cache.find(cacheKey);
        if (it != cache.end())
        {
            DBG("✅ Cache HIT: " + cacheKey);
            it->second.lastAccessTime = juce::Time::getCurrentTime();
            return &it->second.indices;
        }

        DBG("❌ Cache MISS: " + cacheKey + " - analyzing region...");

        // Анализируем регион
        SpectralIndexData regionIndices = analyzer.analyzeRegion(
            originalBuffer,
            currentSampleRate,
            region,
            resolution
        );

        // Создаём запись кэша
        CachedIndexData cachedEntry;
        cachedEntry.indices = std::move(regionIndices);
        cachedEntry.region = region;
        cachedEntry.lastAccessTime = juce::Time::getCurrentTime();

        // Вставляем в кэш
        auto insertResult = cache.insert({ cacheKey, std::move(cachedEntry) });

        // Если не вставилось (ключ существовал), обновляем
        if (!insertResult.second)
        {
            cache[cacheKey] = std::move(cachedEntry);
        }

        // Очищаем старые записи если кэш переполнен
        cleanupCache(cache, maxCacheSize);

        auto stats = cache[cacheKey].indices.calculateStatistics();
        DBG("Detailed indices cached:");
        DBG("  Total indices: " + juce::String(stats.totalIndices));
        DBG("  Cache size: " + juce::String(cache.size()) + " entries");

        return &cache[cacheKey].indices;
    }
    
    // ==========================================================================
    // AI/ML ЭКСПОРТ
    // ==========================================================================
    
    // Экспортировать ВСЕ доступные индексы для ML анализа
    struct MLIndexExport
    {
        // Overview (всегда доступен)
        SpectralIndexData::MLExportData overviewData;
        
        // Детальные данные (если были загружены)
        std::map<juce::String, SpectralIndexData::MLExportData> detailedRegions;
        
        // Метаинформация
        double sampleRate;
        int totalSamples;
        float duration;
    };
    
    MLIndexExport exportAllIndicesForML() const
    {
        const juce::ScopedLock sl(lock);
        
        MLIndexExport export_data;
        
        if (!sampleLoaded || !overviewIndices)
            return export_data;
        
        export_data.sampleRate = currentSampleRate;
        export_data.totalSamples = originalBuffer.getNumSamples();
        export_data.duration = originalBuffer.getNumSamples() / static_cast<float>(currentSampleRate);
        
        // Экспортируем overview
        export_data.overviewData = overviewIndices->exportForML();
        
        // Экспортируем кэшированные детальные регионы
        for (const auto& [key, cached] : maximumCache)
        {
            export_data.detailedRegions[key] = cached.indices.exportForML();
        }
        
        DBG("Exported indices for ML:");
        DBG("  Overview: " + juce::String(export_data.overviewData.numFrames) + " frames");
        DBG("  Detailed regions: " + juce::String(export_data.detailedRegions.size()));
        
        return export_data;
    }
    
    // Экспортировать индексы конкретного региона для паттерн детектора
    std::vector<SpectralIndexFrame> exportRegionForPatternDetection(
        const SpectralIndexData::Region& region)
    {
        // Пытаемся получить максимальное разрешение
        const SpectralIndexData* indices = getDetailedIndices(region, IndexResolution::Maximum);
        
        if (indices)
        {
            return indices->extractRegionIndices(region);
        }
        
        // Fallback на overview
        if (overviewIndices)
        {
            return overviewIndices->extractRegionIndices(region);
        }
        
        return {};
    }
    
    // ==========================================================================
    // СТАТИСТИКА
    // ==========================================================================
    
    struct DatabaseStatistics
    {
        // Overview
        int overviewFrames = 0;
        int overviewBins = 0;
        int overviewTotalIndices = 0;
        
        // Кэш
        int mediumCacheEntries = 0;
        int maximumCacheEntries = 0;
        
        // Память (примерная)
        float estimatedMemoryMB = 0.0f;
        
        // Индексы
        int totalTransients = 0;
        int totalPeaks = 0;
    };
    
    DatabaseStatistics getStatistics() const
    {
        const juce::ScopedLock sl(lock);
        
        DatabaseStatistics stats;
        
        if (overviewIndices)
        {
            stats.overviewFrames = overviewIndices->getNumFrames();
            stats.overviewBins = overviewIndices->getNumBins();
            stats.overviewTotalIndices = stats.overviewFrames * stats.overviewBins;
            
            auto indexStats = overviewIndices->calculateStatistics();
            stats.totalTransients += indexStats.transientCount;
            stats.totalPeaks += indexStats.peakCount;
            
            // Overview memory (magnitude + phase + extras)
            stats.estimatedMemoryMB = (stats.overviewTotalIndices * sizeof(SpectralIndex)) / (1024.0f * 1024.0f);
        }
        
        stats.mediumCacheEntries = static_cast<int>(mediumCache.size());
        stats.maximumCacheEntries = static_cast<int>(maximumCache.size());
        
        // Добавляем память кэша
        for (const auto& [key, cached] : maximumCache)
        {
            int cacheIndices = cached.indices.getNumFrames() * cached.indices.getNumBins();
            stats.estimatedMemoryMB += (cacheIndices * sizeof(SpectralIndex)) / (1024.0f * 1024.0f);
            
            auto indexStats = cached.indices.calculateStatistics();
            stats.totalTransients += indexStats.transientCount;
            stats.totalPeaks += indexStats.peakCount;
        }
        
        return stats;
    }
    
    // ==========================================================================
    // УПРАВЛЕНИЕ КЭШЕМ
    // ==========================================================================
    
    void clearCache()
    {
        const juce::ScopedLock sl(lock);
        mediumCache.clear();
        maximumCache.clear();
        DBG("Cache cleared");
    }
    
    void setMaxCacheSize(int size)
    {
        maxCacheSize = size;
    }
    
    bool hasSampleLoaded() const { return sampleLoaded; }

private:
    struct CachedIndexData
    {
        SpectralIndexData indices;
        SpectralIndexData::Region region;
        juce::Time lastAccessTime;

        // Конструкторы - ВСЕ ПО УМОЛЧАНИЮ (компилятор сгенерирует)
        CachedIndexData() = default;
        ~CachedIndexData() = default;

        // Move semantics
        CachedIndexData(CachedIndexData&&) noexcept = default;
        CachedIndexData& operator=(CachedIndexData&&) noexcept = default;

        // Copy semantics (компилятор сгенерирует автоматически)
        CachedIndexData(const CachedIndexData&) = default;
        CachedIndexData& operator=(const CachedIndexData&) = default;
    };
    
    // Данные
    std::unique_ptr<SpectralIndexData> overviewIndices;
    std::map<juce::String, CachedIndexData> mediumCache;
    std::map<juce::String, CachedIndexData> maximumCache;
    
    juce::AudioBuffer<float> originalBuffer;
    double currentSampleRate = 44100.0;
    bool sampleLoaded = false;
    
    SpectralIndexAnalyzer analyzer;
    
    int maxCacheSize = 10;  // Максимум 10 регионов в кэше
    
    mutable juce::CriticalSection lock;
    
    // Генерация ключа для кэша
    juce::String generateCacheKey(const SpectralIndexData::Region& region, IndexResolution resolution)
    {
        return juce::String::formatted("%.3f-%.3f_%.0f-%.0f_%d",
                                      region.startTime,
                                      region.endTime,
                                      region.minFreq,
                                      region.maxFreq,
                                      static_cast<int>(resolution));
    }
    
    // Очистка старых записей в кэше (LRU)
    void cleanupCache(std::map<juce::String, CachedIndexData>& cache, int maxSize)
    {
        if (cache.size() <= static_cast<size_t>(maxSize))
            return;
        
        // Находим самую старую запись
        juce::String oldestKey;
        juce::Time oldestTime = juce::Time::getCurrentTime();
        
        for (const auto& [key, data] : cache)
        {
            if (data.lastAccessTime < oldestTime)
            {
                oldestTime = data.lastAccessTime;
                oldestKey = key;
            }
        }
        
        if (!oldestKey.isEmpty())
        {
            DBG("Removing old cache entry: " + oldestKey);
            cache.erase(oldestKey);
        }
    }
    
    // Запрещаем копирование (база данных должна быть одна)
    SpectralIndexDatabase(const SpectralIndexDatabase&) = delete;
    SpectralIndexDatabase& operator=(const SpectralIndexDatabase&) = delete;
};