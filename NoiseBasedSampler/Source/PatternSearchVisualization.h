/*
PatternSearchVisualization.h
Визуализация прогресса поиска паттернов в минималистичном стиле
✅ Block-based visualization как на скриншоте
✅ Real-time updates через thread-safe механизм
✅ Цветовая индикация: black (unchecked) → blue shades (checked) → green (pattern)
✅ Минималистичный дизайн
*/

#pragma once
#include <JuceHeader.h>
#include "PatternAnalyzer.h"

class PatternSearchVisualization : public juce::Component,
                                   public juce::Timer
{
public:
    PatternSearchVisualization()
    {
        startTimerHz(30); // 30 FPS updates
        
        // Enable mouse events
        setWantsKeyboardFocus(true);
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    // Thread-safe обновление прогресса
    void updateProgress(const PatternSearchProgress& progress)
    {
        const juce::ScopedLock sl(progressLock);
        
        currentProgress = progress;
        hasData = true;
        
        // Если нашли новый паттерн - добавляем в список
        if (progress.newPatternFound)
        {
            RecentPattern recent;
            recent.patternId = progress.lastFoundPattern.patternId;
            recent.occurrences = progress.lastFoundPattern.occurrenceCount;
            recent.length = static_cast<int>(progress.lastFoundPattern.values.size());
            recent.positions = progress.lastFoundPattern.occurrencePositions;
            recent.fadeAlpha = 1.0f;
            
            recentPatterns.push_back(recent);
            
            // Ограничиваем список последних паттернов
            if (recentPatterns.size() > 5)
                recentPatterns.erase(recentPatterns.begin());
        }
    }

    void reset()
    {
        const juce::ScopedLock sl(progressLock);
        hasData = false;
        currentProgress = PatternSearchProgress();
        recentPatterns.clear();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        
        // Background
        g.fillAll(juce::Colour(0xff0a0a0a));
        
        const juce::ScopedLock sl(progressLock);
        
        if (!hasData)
        {
            g.setColour(juce::Colours::grey);
            g.setFont(12.0f);
            g.drawText("Waiting for analysis...", 
                      area, juce::Justification::centred);
            return;
        }
        
        // ========== VISUALIZATION AREA ==========
        auto vizArea = area.reduced(10).removeFromTop(area.getHeight() - 100);
        
        drawBlockVisualization(g, vizArea);
        
        // ========== INFO AREA ==========
        auto infoArea = area.removeFromBottom(90).reduced(10);
        drawInfoPanel(g, infoArea);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        // Store position under mouse BEFORE zoom
        float dataPosUnderMouse = panOffsetX + (e.x / static_cast<float>(getWidth())) * (1.0f / zoomHorizontal);
        
        // Apply horizontal zoom
        float zoomDelta = wheel.deltaY * 0.5f;
        zoomHorizontal *= (1.0f + zoomDelta);
        constrainZoomAndPan();
        
        // Recalculate pan to keep data position under mouse
        float newViewWidth = 1.0f / zoomHorizontal;
        panOffsetX = dataPosUnderMouse - (e.x / static_cast<float>(getWidth())) * newViewWidth;
        panOffsetX = juce::jlimit(0.0f, 1.0f - newViewWidth, panOffsetX);
        
        repaint();
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.mods.isMiddleButtonDown())
        {
            // Pan with middle mouse button
            float dragDeltaX = (e.getDistanceFromDragStartX() / static_cast<float>(getWidth())) * (1.0f / zoomHorizontal);
            panOffsetX = originalPanX - dragDeltaX;
            constrainZoomAndPan();
            repaint();
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isMiddleButtonDown())
        {
            originalPanX = panOffsetX;
        }
    }

    void timerCallback() override
    {
        // Fade out эффект для недавних паттернов
        {
            const juce::ScopedLock sl(progressLock);
            
            for (auto& recent : recentPatterns)
            {
                recent.fadeAlpha *= 0.95f; // Плавное затухание
            }
        }
        
        repaint();
    }

private:
    // Zoom state
    float zoomHorizontal = 1.0f;
    float panOffsetX = 0.0f;
    float originalPanX = 0.0f;
    static constexpr float minZoom = 1.0f;
    static constexpr float maxZoom = 20.0f;
    
    juce::CriticalSection progressLock;
    PatternSearchProgress currentProgress;
    bool hasData = false;
    
    // Для хранения недавно найденных паттернов
    struct RecentPattern
    {
        int patternId;
        int occurrences;
        int length;
        std::vector<int> positions;
        float fadeAlpha; // Для fade-out эффекта
    };
    
    std::vector<RecentPattern> recentPatterns;
    
    // ==========================================================================
    // ZOOM MANAGEMENT
    // ==========================================================================
    
    void constrainZoomAndPan()
    {
        zoomHorizontal = juce::jlimit(minZoom, maxZoom, zoomHorizontal);
        
        float viewWidth = 1.0f / zoomHorizontal;
        float maxPanX = juce::jmax(0.0f, 1.0f - viewWidth);
        
        panOffsetX = juce::jlimit(0.0f, maxPanX, panOffsetX);
    }
    
    // ==========================================================================
    // BLOCK VISUALIZATION - КЛЮЧЕВАЯ ФУНКЦИЯ
    // ==========================================================================
    
    void drawBlockVisualization(juce::Graphics& g, juce::Rectangle<int> area)
    {
        if (currentProgress.totalDataPoints == 0)
            return;
        
        // Apply zoom and pan - determine visible range
        float viewWidth = 1.0f / zoomHorizontal;
        int startBlock = static_cast<int>(currentProgress.totalDataPoints * panOffsetX);
        int endBlock = static_cast<int>(currentProgress.totalDataPoints * (panOffsetX + viewWidth));
        endBlock = juce::jmin(endBlock, currentProgress.totalDataPoints);
        int visibleBlocks = endBlock - startBlock;
        
        // Вычисляем оптимальное количество блоков для отображения
        int availableWidth = area.getWidth() - 20;
        int blockSize = 4; // Минимальный размер блока
        int blockGap = 1;
        
        int maxBlocksPerRow = availableWidth / (blockSize + blockGap);
        int totalBlocks = visibleBlocks;
        
        // Если блоков слишком много - группируем
        int samplesPerBlock = 1;
        if (totalBlocks > maxBlocksPerRow * 30) // Если больше 30 рядов
        {
            samplesPerBlock = totalBlocks / (maxBlocksPerRow * 30);
            totalBlocks = totalBlocks / samplesPerBlock;
        }
        
        int blocksPerRow = juce::jmin(maxBlocksPerRow, totalBlocks);
        int numRows = (totalBlocks + blocksPerRow - 1) / blocksPerRow;
        
        // Title
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("Index Analysis Progress", 
                  area.removeFromTop(20), 
                  juce::Justification::centredLeft);
        
        area.removeFromTop(5);
        
        // Вычисляем размеры
        int totalWidth = blocksPerRow * (blockSize + blockGap);
        int startX = area.getX() + (area.getWidth() - totalWidth) / 2;
        int startY = area.getY();
        
        // Создаем set позиций найденных паттернов для быстрого поиска
        std::set<int> patternPositions;
        for (const auto& recent : recentPatterns)
        {
            for (int pos : recent.positions)
            {
                // Добавляем все позиции этого паттерна
                for (int i = 0; i < recent.length; ++i)
                {
                    int sampleIdx = (pos + i) / samplesPerBlock;
                    if (sampleIdx >= startBlock && sampleIdx < endBlock)
                    {
                        patternPositions.insert(sampleIdx - startBlock);
                    }
                }
            }
        }
        
        // Отрисовка блоков
        int currentBlock = 0;
        int checkedBlocks = (currentProgress.blocksCheckedSoFar - startBlock) / samplesPerBlock;
        
        for (int row = 0; row < numRows && currentBlock < totalBlocks; ++row)
        {
            for (int col = 0; col < blocksPerRow && currentBlock < totalBlocks; ++col)
            {
                int x = startX + col * (blockSize + blockGap);
                int y = startY + row * (blockSize + blockGap);
                
                juce::Rectangle<int> blockRect(x, y, blockSize, blockSize);
                
                // Определяем цвет блока
                juce::Colour blockColour;
                
                if (patternPositions.count(currentBlock) > 0)
                {
                    // Найден паттерн - зеленый с fade
                    float alpha = 0.3f;
                    for (const auto& recent : recentPatterns)
                    {
                        for (int pos : recent.positions)
                        {
                            int posBlock = pos / samplesPerBlock;
                            if (std::abs(posBlock - currentBlock) < recent.length / samplesPerBlock)
                            {
                                alpha = juce::jmax(alpha, recent.fadeAlpha);
                            }
                        }
                    }
                    blockColour = juce::Colour(0xff10b981).withAlpha(alpha);
                }
                else if (currentBlock < checkedBlocks)
                {
                    // Проверено - синий градиент
                    float checkedRatio = currentBlock / static_cast<float>(checkedBlocks);
                    float intensity = 0.2f + checkedRatio * 0.3f; // 0.2 - 0.5
                    blockColour = juce::Colour(0xff3b82f6).withAlpha(intensity);
                }
                else
                {
                    // Не проверено - черный
                    blockColour = juce::Colour(0xff000000);
                }
                
                g.setColour(blockColour);
                g.fillRect(blockRect);
                
                // Текущая позиция анализа - подсветка
                int currentPosBlock = currentProgress.currentStartPosition / samplesPerBlock;
                if (currentBlock + startBlock == currentPosBlock)
                {
                    g.setColour(juce::Colour(0xfff59e0b)); // Желтый
                    g.drawRect(blockRect.expanded(1), 1);
                }
                
                currentBlock++;
            }
        }
        
        // Legend
        auto legendArea = area.withY(startY + numRows * (blockSize + blockGap) + 15);
        drawLegend(g, legendArea);
    }
    
    void drawLegend(juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setFont(9.0f);
        
        struct LegendItem {
            juce::Colour colour;
            juce::String label;
        };
        
        std::vector<LegendItem> items = {
            { juce::Colour(0xff000000), "Unchecked" },
            { juce::Colour(0xff3b82f6).withAlpha(0.4f), "Checked" },
            { juce::Colour(0xff10b981), "Pattern Found" },
            { juce::Colour(0xfff59e0b), "Current Position" }
        };
        
        int itemWidth = 120;
        int startX = area.getX();
        
        for (size_t i = 0; i < items.size(); ++i)
        {
            int x = startX + i * itemWidth;
            
            // Color box
            g.setColour(items[i].colour);
            g.fillRect(x, area.getY(), 12, 12);
            
            // Label
            g.setColour(juce::Colours::lightgrey);
            g.drawText(items[i].label, 
                      x + 16, area.getY(), 100, 12, 
                      juce::Justification::centredLeft);
        }
    }
    
    // ==========================================================================
    // INFO PANEL
    // ==========================================================================
    
    void drawInfoPanel(juce::Graphics& g, juce::Rectangle<int> area)
    {
        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(area.toFloat(), 6.0f);
        
        area.reduce(10, 10);
        
        // Statistics
        auto statsArea = area.removeFromTop(35);
        
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        
        juce::String statsText = 
            "Progress: " + juce::String(currentProgress.overallProgress * 100.0f, 1) + "% | " +
            "Checked: " + juce::String(currentProgress.blocksCheckedSoFar) + "/" + 
            juce::String(currentProgress.totalBlocksToCheck) + " | " +
            "Patterns Found: " + juce::String(currentProgress.patternsFoundSoFar) + " | " +
            "Current Length: " + juce::String(currentProgress.currentPatternLength);
        
        g.drawText(statsText, statsArea, juce::Justification::centredLeft);
        
        // Recent patterns
        if (!recentPatterns.empty())
        {
            area.removeFromTop(5);
            
            g.setColour(juce::Colour(0xff10b981));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText("Recently Found:", area.removeFromTop(15), 
                      juce::Justification::centredLeft);
            
            for (int i = recentPatterns.size() - 1; i >= 0 && i >= static_cast<int>(recentPatterns.size()) - 3; --i)
            {
                const auto& recent = recentPatterns[i];
                
                float alpha = recent.fadeAlpha * 0.8f;
                g.setColour(juce::Colours::lightgrey.withAlpha(alpha));
                g.setFont(9.0f);
                
                juce::String patternText = 
                    "  • Pattern #" + juce::String(recent.patternId) + 
                    " (length: " + juce::String(recent.length) + 
                    ", " + juce::String(recent.occurrences) + "x)";
                
                g.drawText(patternText, area.removeFromTop(14), 
                          juce::Justification::centredLeft);
            }
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternSearchVisualization)
};