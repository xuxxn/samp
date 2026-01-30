/*
TreeMapVisualization.h - REAL-TIME TREEMAP WITH PROGRESS
✅ Squarified treemap algorithm for optimal aspect ratios
✅ Real-time pattern addition as they're discovered
✅ Vertical progress bar when no selection
✅ Smooth animations for new patterns
✅ Sortable by ID, occurrences, length, avg value
*/

#pragma once
#include <JuceHeader.h>
#include "PatternAnalyzer.h"
#include <vector>
#include <map>

// ==========================================================================
// TREEMAP NODE STRUCTURE
// ==========================================================================

struct TreeMapNode
{
    int patternId = 0;
    int occurrences = 0;
    int length = 0;
    float avgValue = 0.0f;
    float normalizedSize = 0.0f;  // 0.0 to 1.0
    juce::Rectangle<float> bounds;
    std::vector<float> waveform;
    std::vector<int> occurrencePositions;

    bool isSelected = false;
    bool isAnimatingIn = false;
    float animationProgress = 0.0f;  // 0.0 to 1.0

    TreeMapNode() = default;

    TreeMapNode(const IndexPattern& pattern)
        : patternId(pattern.patternId)
        , occurrences(pattern.occurrenceCount)
        , length(static_cast<int>(pattern.values.size()))
        , avgValue(pattern.averageValue)
        , waveform(pattern.values)
        , occurrencePositions(pattern.occurrencePositions)
    {
    }
};

// ==========================================================================
// TREEMAP VISUALIZATION COMPONENT
// ==========================================================================

class TreeMapVisualization : public juce::Component,
    public juce::Timer
{
public:
    enum class SortMode
    {
        ByOccurrences,  // Default - largest first
        ById,
        ByLength,
        ByAvgValue
    };

    TreeMapVisualization()
    {
        startTimerHz(30);  // 30 FPS for smooth animations
        
        // Enable mouse events
        setWantsKeyboardFocus(true);
        setInterceptsMouseClicks(true, true);
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    ~TreeMapVisualization() override
    {
        stopTimer();
    }

    // ==========================================================================
    // PUBLIC API
    // ==========================================================================

    void addPattern(const IndexPattern& pattern)
    {
        TreeMapNode node(pattern);
        node.isAnimatingIn = true;
        node.animationProgress = 0.0f;

        nodes.push_back(node);

        // Recalculate layout
        sortAndLayout();

        DBG("TreeMap: Added pattern #" + juce::String(pattern.patternId));
    }

    void setPatterns(const std::vector<IndexPattern>& patterns)
    {
        nodes.clear();
        selectedNodeIndex = -1;

        for (const auto& pattern : patterns)
        {
            nodes.push_back(TreeMapNode(pattern));
        }

        sortAndLayout();
        repaint();
    }

    void clearPatterns()
    {
        nodes.clear();
        selectedNodeIndex = -1;
        repaint();
    }

    void setProgress(float progress01)
    {
        analysisProgress = juce::jlimit(0.0f, 1.0f, progress01);
        repaint();
    }

    void setSortMode(SortMode mode)
    {
        if (sortMode != mode)
        {
            sortMode = mode;
            sortAndLayout();
            repaint();
        }
    }

    int getSelectedPatternId() const
    {
        if (selectedNodeIndex >= 0 && selectedNodeIndex < nodes.size())
        {
            return nodes[selectedNodeIndex].patternId;
        }
        return -1;
    }

    const TreeMapNode* getSelectedNode() const
    {
        if (selectedNodeIndex >= 0 && selectedNodeIndex < nodes.size())
        {
            return &nodes[selectedNodeIndex];
        }
        return nullptr;
    }

    // ==========================================================================
    // CALLBACKS
    // ==========================================================================

    std::function<void(const TreeMapNode&)> onPatternSelected;
    std::function<void()> onPatternDeselected;

    // ==========================================================================
    // COMPONENT OVERRIDES
    // ==========================================================================

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0a0a0a));

        if (nodes.empty())
        {
            // Show vertical progress bar in center
            paintProgressBar(g);
        }
        else
        {
            // Paint all treemap nodes
            for (size_t i = 0; i < nodes.size(); ++i)
            {
                paintNode(g, nodes[i], i == selectedNodeIndex);
            }
        }
    }

    void resized() override
    {
        sortAndLayout();
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
        
        // Recalculate layout with zoom
        sortAndLayout();
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
            
            // Recalculate layout
            sortAndLayout();
            repaint();
        }
        else
        {
            // Normal selection behavior
            handleSelection(e.position);
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isMiddleButtonDown())
        {
            originalPanX = panOffsetX;
        }
        else
        {
            handleSelection(e.position);
        }
    }
    
    void handleSelection(juce::Point<float> pos)
    {
        int clickedIndex = findNodeAtPosition(pos);

        if (clickedIndex != selectedNodeIndex)
        {
            // Deselect previous
            if (selectedNodeIndex >= 0 && selectedNodeIndex < nodes.size())
            {
                nodes[selectedNodeIndex].isSelected = false;
            }

            selectedNodeIndex = clickedIndex;

            // Select new
            if (selectedNodeIndex >= 0)
            {
                nodes[selectedNodeIndex].isSelected = true;

                if (onPatternSelected)
                {
                    onPatternSelected(nodes[selectedNodeIndex]);
                }
            }
            else
            {
                if (onPatternDeselected)
                {
                    onPatternDeselected();
                }
            }

            repaint();
        }
    }

    void timerCallback() override
    {
        bool needsRepaint = false;

        // Animate new patterns
        for (auto& node : nodes)
        {
            if (node.isAnimatingIn)
            {
                node.animationProgress += 0.08f;  // Speed up animation

                if (node.animationProgress >= 1.0f)
                {
                    node.animationProgress = 1.0f;
                    node.isAnimatingIn = false;
                }

                needsRepaint = true;
            }
        }

        if (needsRepaint)
        {
            repaint();
        }
    }

private:
    // Zoom state
    float zoomHorizontal = 1.0f;
    float panOffsetX = 0.0f;
    float originalPanX = 0.0f;
    static constexpr float minZoom = 1.0f;
    static constexpr float maxZoom = 10.0f;
    
    std::vector<TreeMapNode> nodes;
    int selectedNodeIndex = -1;
    float analysisProgress = 0.0f;
    SortMode sortMode = SortMode::ByOccurrences;

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
    // SORTING & LAYOUT
    // ==========================================================================

    void sortAndLayout()
    {
        if (nodes.empty())
            return;

        // Sort nodes based on mode
        sortNodes();

        // Calculate normalized sizes
        calculateNormalizedSizes();

        // Layout using squarified treemap algorithm with zoom applied
        auto area = getLocalBounds().reduced(5).toFloat();
        
        // Apply zoom: scale horizontally
        float scaledWidth = area.getWidth() * zoomHorizontal;
        area = area.withWidth(scaledWidth);
        
        // Apply pan: shift horizontally
        float panPixels = panOffsetX * area.getWidth();
        area = area.withX(area.getX() - panPixels);
        
        layoutSquarified(nodes, area);
    }

    void sortNodes()
    {
        switch (sortMode)
        {
        case SortMode::ByOccurrences:
            std::sort(nodes.begin(), nodes.end(),
                [](const TreeMapNode& a, const TreeMapNode& b) {
                    return a.occurrences > b.occurrences;
                });
            break;

        case SortMode::ById:
            std::sort(nodes.begin(), nodes.end(),
                [](const TreeMapNode& a, const TreeMapNode& b) {
                    return a.patternId < b.patternId;
                });
            break;

        case SortMode::ByLength:
            std::sort(nodes.begin(), nodes.end(),
                [](const TreeMapNode& a, const TreeMapNode& b) {
                    return a.length > b.length;
                });
            break;

        case SortMode::ByAvgValue:
            std::sort(nodes.begin(), nodes.end(),
                [](const TreeMapNode& a, const TreeMapNode& b) {
                    return std::abs(a.avgValue) > std::abs(b.avgValue);
                });
            break;
        }
    }

    void calculateNormalizedSizes()
    {
        // Size based on occurrences (most important metric)
        int totalOccurrences = 0;
        for (const auto& node : nodes)
        {
            totalOccurrences += node.occurrences;
        }

        if (totalOccurrences > 0)
        {
            for (auto& node : nodes)
            {
                node.normalizedSize = node.occurrences / static_cast<float>(totalOccurrences);
            }
        }
    }

    // ==========================================================================
    // SQUARIFIED TREEMAP ALGORITHM
    // ==========================================================================

    void layoutSquarified(std::vector<TreeMapNode>& items, juce::Rectangle<float> area)
    {
        if (items.empty() || area.isEmpty())
            return;

        std::vector<TreeMapNode*> sorted;
        for (auto& item : items)
        {
            sorted.push_back(&item);
        }

        squarify(sorted, area);
    }

    void squarify(std::vector<TreeMapNode*>& items, juce::Rectangle<float> area)
    {
        if (items.empty())
            return;

        if (items.size() == 1)
        {
            items[0]->bounds = area;
            return;
        }

        bool useVertical = area.getWidth() >= area.getHeight();

        // Calculate total size
        float totalSize = 0.0f;
        for (auto* item : items)
        {
            totalSize += item->normalizedSize;
        }

        // Split into rows
        std::vector<TreeMapNode*> row;
        std::vector<TreeMapNode*> remaining = items;

        float rowSize = 0.0f;
        float bestAspectRatio = std::numeric_limits<float>::max();

        while (!remaining.empty())
        {
            auto* current = remaining[0];
            row.push_back(current);
            rowSize += current->normalizedSize;

            float aspectRatio = calculateWorstAspectRatio(row, rowSize / totalSize, area, useVertical);

            if (aspectRatio > bestAspectRatio)
            {
                // Adding this item made it worse - layout previous row
                row.pop_back();
                rowSize -= current->normalizedSize;

                layoutRow(row, rowSize / totalSize, area, useVertical);

                // Update area for remaining items
                if (useVertical)
                {
                    float width = area.getWidth() * (rowSize / totalSize);
                    area = area.withTrimmedLeft(width);
                }
                else
                {
                    float height = area.getHeight() * (rowSize / totalSize);
                    area = area.withTrimmedTop(height);
                }

                row.clear();
                rowSize = 0.0f;
                bestAspectRatio = std::numeric_limits<float>::max();
                totalSize -= rowSize;
            }
            else
            {
                bestAspectRatio = aspectRatio;
                remaining.erase(remaining.begin());
            }
        }

        // Layout final row
        if (!row.empty())
        {
            layoutRow(row, rowSize / totalSize, area, useVertical);
        }
    }

    float calculateWorstAspectRatio(const std::vector<TreeMapNode*>& row, float rowSize,
        const juce::Rectangle<float>& area, bool useVertical)
    {
        float width = useVertical ? area.getWidth() * rowSize : area.getWidth();
        float height = useVertical ? area.getHeight() : area.getHeight() * rowSize;

        if (width == 0.0f || height == 0.0f)
            return std::numeric_limits<float>::max();

        float worstRatio = 0.0f;

        for (auto* item : row)
        {
            float itemWidth = useVertical ? width : width * (item->normalizedSize / rowSize);
            float itemHeight = useVertical ? height * (item->normalizedSize / rowSize) : height;

            float ratio = std::max(itemWidth / itemHeight, itemHeight / itemWidth);
            worstRatio = std::max(worstRatio, ratio);
        }

        return worstRatio;
    }

    void layoutRow(const std::vector<TreeMapNode*>& row, float rowSize,
        const juce::Rectangle<float>& area, bool useVertical)
    {
        float totalRowSize = 0.0f;
        for (auto* item : row)
        {
            totalRowSize += item->normalizedSize;
        }

        float offset = 0.0f;

        for (auto* item : row)
        {
            float itemFraction = item->normalizedSize / totalRowSize;

            if (useVertical)
            {
                float width = area.getWidth() * rowSize;
                float height = area.getHeight() * itemFraction;
                item->bounds = juce::Rectangle<float>(
                    area.getX(), area.getY() + offset, width, height
                ).reduced(2.0f);  // Padding
                offset += height;
            }
            else
            {
                float width = area.getWidth() * itemFraction;
                float height = area.getHeight() * rowSize;
                item->bounds = juce::Rectangle<float>(
                    area.getX() + offset, area.getY(), width, height
                ).reduced(2.0f);  // Padding
                offset += width;
            }
        }
    }

    // ==========================================================================
    // PAINTING
    // ==========================================================================

    void paintNode(juce::Graphics& g, const TreeMapNode& node, bool isSelected)
    {
        auto bounds = node.bounds;

        // Animation: scale from center
        if (node.isAnimatingIn)
        {
            auto center = bounds.getCentre();
            float scale = node.animationProgress;
            bounds = bounds.withSizeKeepingCentre(
                bounds.getWidth() * scale,
                bounds.getHeight() * scale
            );
        }

        // Color based on occurrences (gradient from light to dark green)
        float intensity = juce::jlimit(0.3f, 1.0f,
            node.occurrences / 100.0f);  // Normalize to 0.3-1.0

        juce::Colour baseColor = juce::Colour(0xff10b981);  // Green
        juce::Colour nodeColor = baseColor.withBrightness(intensity);

        if (isSelected)
        {
            // Red for selected
            nodeColor = juce::Colour(0xffef4444);
        }

        // Fill
        g.setColour(nodeColor);
        g.fillRect(bounds);

        // Border
        g.setColour(juce::Colour(0xff0a0a0a));
        g.drawRect(bounds, 1.0f);

        // Draw pattern info if large enough
        if (bounds.getWidth() > 60 && bounds.getHeight() > 40)
        {
            // Draw occurrences number (centered)
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(bounds.getHeight() * 0.3f, juce::Font::bold));
            g.drawText(juce::String(node.occurrences),
                bounds, juce::Justification::centred);

            // Draw ID in corner (if space)
            if (bounds.getWidth() > 100 && bounds.getHeight() > 60)
            {
                g.setFont(10.0f);
                g.setColour(juce::Colours::white.withAlpha(0.7f));
                g.drawText("#" + juce::String(node.patternId),
                    bounds.reduced(5), juce::Justification::topLeft);
            }
        }
    }

    void paintProgressBar(juce::Graphics& g)
    {
        auto area = getLocalBounds().reduced(20);

        // Center vertical bar
        int barWidth = 40;
        int barHeight = area.getHeight();

        auto barBounds = juce::Rectangle<int>(
            area.getCentreX() - barWidth / 2,
            area.getY(),
            barWidth,
            barHeight
        );

        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRect(barBounds);

        // Progress (fill from bottom to top)
        int fillHeight = static_cast<int>(barHeight * analysisProgress);
        auto fillBounds = barBounds.withTop(barBounds.getBottom() - fillHeight);

        g.setColour(juce::Colour(0xff10b981));
        g.fillRect(fillBounds);

        // Border
        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawRect(barBounds, 2.0f);

        // Percentage text
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(16.0f, juce::Font::bold));

        juce::String progressText = juce::String(static_cast<int>(analysisProgress * 100)) + "%";
        g.drawText(progressText,
            barBounds.withY(barBounds.getBottom() + 10).withHeight(30),
            juce::Justification::centred);
    }

    // ==========================================================================
    // UTILITIES
    // ==========================================================================

    int findNodeAtPosition(juce::Point<float> pos)
    {
        // Apply inverse zoom and pan to convert screen coordinates to data coordinates
        float dataX = pos.x / zoomHorizontal + panOffsetX * zoomHorizontal;
        juce::Point<float> dataPos(dataX, pos.y);
        
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            if (nodes[i].bounds.contains(dataPos))
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TreeMapVisualization)
};