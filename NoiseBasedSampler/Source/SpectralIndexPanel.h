/*
SpectralIndexPanel.h - ПОЛНЫЙ НАБОР ИНСТРУМЕНТОВ
✅ Paint - создание звука (content-aware)
✅ Amplify - усиление существующего (как в SpectraLayers)
✅ Attenuate - размытие/blend (как в RX)
✅ Remove - полное удаление частот
✅ Line Tool - рисование прямых линий
✅ Noise - добавление/создание шума/рандомизация
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SpectralIndexData.h"

class SpectralIndexPanel : public juce::Component,
    public juce::Timer
{
public:
    SpectralIndexPanel(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        setSize(1200, 700);
        startTimerHz(30);

        // ========== CONTROL BUTTONS ==========
        addAndMakeVisible(analyzeButton);
        analyzeButton.setButtonText("Analyze Indices");
        analyzeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b82f6));
        analyzeButton.onClick = [this] { analyzeIndices(); };

        addAndMakeVisible(applyButton);
        applyButton.setButtonText("Apply Changes");
        applyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981));
        applyButton.setEnabled(false);
        applyButton.onClick = [this] { applyModifications(); };

        addAndMakeVisible(clearCacheButton);
        clearCacheButton.setButtonText("Clear Cache");
        clearCacheButton.onClick = [this] {
            processor.getIndexDatabase().clearCache();
            spectrogramNeedsUpdate = true;
            repaint();
            };

        addAndMakeVisible(clearModificationsButton);
        clearModificationsButton.setButtonText("Clear Edits");
        clearModificationsButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffef4444));
        clearModificationsButton.setEnabled(false);
        clearModificationsButton.onClick = [this] {
            processor.clearAllModifications();
            indicesModified = false;
            applyButton.setEnabled(false);
            clearModificationsButton.setEnabled(false);
            spectrogramNeedsUpdate = true;
            repaint();
            };

        addAndMakeVisible(resetZoomButton);
        resetZoomButton.setButtonText("Reset View");
        resetZoomButton.onClick = [this] { resetZoom(); };

        // ========== TOOL SELECTION ==========
        addAndMakeVisible(toolLabel);
        toolLabel.setText("Tool:", juce::dontSendNotification);
        toolLabel.setFont(juce::Font(12.0f, juce::Font::bold));

        // Row 1
        addAndMakeVisible(paintToolButton);
        paintToolButton.setButtonText("🖌️ Paint");
        paintToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981));
        paintToolButton.onClick = [this] { setTool(Tool::Paint); };

        addAndMakeVisible(amplifyToolButton);
        amplifyToolButton.setButtonText("📈 Amplify");
        amplifyToolButton.onClick = [this] { setTool(Tool::Amplify); };

        addAndMakeVisible(attenuateToolButton);
        attenuateToolButton.setButtonText("🌫️ Attenuate");
        attenuateToolButton.onClick = [this] { setTool(Tool::Attenuate); };

        // Row 2
        addAndMakeVisible(removeToolButton);
        removeToolButton.setButtonText("🗑️ Remove");
        removeToolButton.onClick = [this] { setTool(Tool::Remove); };

        addAndMakeVisible(lineToolButton);
        lineToolButton.setButtonText("📏 Line");
        lineToolButton.onClick = [this] { setTool(Tool::Line); };

        addAndMakeVisible(noiseToolButton);
        noiseToolButton.setButtonText("🎲 Noise");
        noiseToolButton.onClick = [this] { setTool(Tool::Noise); };

        // ========== TOOL PARAMETERS ==========
        addAndMakeVisible(toolSizeLabel);
        toolSizeLabel.setText("Size:", juce::dontSendNotification);
        toolSizeLabel.setFont(juce::Font(11.0f));

        addAndMakeVisible(toolSizeSlider);
        toolSizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        toolSizeSlider.setRange(1.0, 20.0, 1.0);
        toolSizeSlider.setValue(5.0);
        toolSizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);

        addAndMakeVisible(toolIntensityLabel);
        toolIntensityLabel.setText("Intensity:", juce::dontSendNotification);
        toolIntensityLabel.setFont(juce::Font(11.0f));

        addAndMakeVisible(toolIntensitySlider);
        toolIntensitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
        toolIntensitySlider.setRange(0.1, 2.0, 0.1);
        toolIntensitySlider.setValue(1.0);
        toolIntensitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);

        // ========== ZOOM SLIDERS ==========
        addAndMakeVisible(horizontalZoomSlider);
        horizontalZoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        horizontalZoomSlider.setRange(1.0, 20.0, 0.1);
        horizontalZoomSlider.setValue(1.0);
        horizontalZoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        horizontalZoomSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff3b82f6));
        horizontalZoomSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff60a5fa));
        horizontalZoomSlider.onValueChange = [this] {
            zoomHorizontal = static_cast<float>(horizontalZoomSlider.getValue());
            constrainZoomAndPan();
            repaint();
            };

        addAndMakeVisible(verticalZoomSlider);
        verticalZoomSlider.setSliderStyle(juce::Slider::LinearVertical);
        verticalZoomSlider.setRange(1.0, 20.0, 0.1);
        verticalZoomSlider.setValue(1.0);
        verticalZoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        verticalZoomSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff10b981));
        verticalZoomSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff34d399));
        verticalZoomSlider.onValueChange = [this] {
            zoomVertical = static_cast<float>(verticalZoomSlider.getValue());
            constrainZoomAndPan();
            repaint();
            };

        setTool(Tool::Paint);
        setWantsKeyboardFocus(true);
    }

    ~SpectralIndexPanel() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff0a0a0a));

        const auto& indexDB = processor.getIndexDatabase();

        if (!indexDB.hasSampleLoaded())
        {
            g.setColour(juce::Colours::white);
            g.setFont(20.0f);
            g.drawText("Load a sample and click 'Analyze Indices'",
                getLocalBounds().reduced(20).withTrimmedBottom(180),
                juce::Justification::centred);
            return;
        }

        auto bounds = getLocalBounds().reduced(10);
        bounds.removeFromBottom(180); // Больше места для двух рядов инструментов

        int verticalSliderWidth = 30;
        int horizontalSliderHeight = 30;

        auto contentArea = bounds;
        contentArea.removeFromRight(verticalSliderWidth + 5);
        contentArea.removeFromBottom(horizontalSliderHeight + 5);

        // Draw spectrogram
        const auto* overviewIndices = indexDB.getOverviewIndices();
        if (overviewIndices)
        {
            drawSpectrogramWithImageData(g, contentArea, *overviewIndices);
        }

        // Draw line preview
        if (currentTool == Tool::Line && isDrawingLine)
        {
            g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.8f));
            g.drawLine(lineStartPos.x, lineStartPos.y,
                getMouseXYRelative().x, getMouseXYRelative().y, 2.0f);

            g.fillEllipse(lineStartPos.x - 4, lineStartPos.y - 4, 8, 8);
        }

        // Draw cursor
        if (contentArea.contains(getMouseXYRelative()) && !isPanning)
        {
            auto mousePos = getMouseXYRelative().toFloat();
            float toolSize = static_cast<float>(toolSizeSlider.getValue());

            juce::Colour cursorColour;
            switch (currentTool)
            {
            case Tool::Paint:      cursorColour = juce::Colour(0xff10b981); break;
            case Tool::Amplify:    cursorColour = juce::Colour(0xff3b82f6); break;
            case Tool::Attenuate:  cursorColour = juce::Colour(0xfff59e0b); break;
            case Tool::Remove:     cursorColour = juce::Colour(0xffef4444); break;
            case Tool::Line:       cursorColour = juce::Colour(0xfff59e0b); break;
            case Tool::Noise:      cursorColour = juce::Colour(0xff8b5cf6); break;
            }

            cursorColour = cursorColour.withAlpha(0.6f);

            if (currentTool != Tool::Line)
            {
                g.setColour(cursorColour);
                g.drawEllipse(mousePos.x - toolSize, mousePos.y - toolSize,
                    toolSize * 2, toolSize * 2, 2.0f);

                g.drawLine(mousePos.x - toolSize - 5, mousePos.y,
                    mousePos.x - toolSize, mousePos.y, 1.5f);
                g.drawLine(mousePos.x + toolSize, mousePos.y,
                    mousePos.x + toolSize + 5, mousePos.y, 1.5f);
                g.drawLine(mousePos.x, mousePos.y - toolSize - 5,
                    mousePos.x, mousePos.y - toolSize, 1.5f);
                g.drawLine(mousePos.x, mousePos.y + toolSize,
                    mousePos.x, mousePos.y + toolSize + 5, 1.5f);
            }
            else
            {
                // Crosshair for line tool
                g.setColour(cursorColour);
                g.drawLine(mousePos.x - 8, mousePos.y, mousePos.x + 8, mousePos.y, 2.0f);
                g.drawLine(mousePos.x, mousePos.y - 8, mousePos.x, mousePos.y + 8, 2.0f);
            }
        }

        if (isPanning)
        {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }
        else
        {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }

        // Status
        if (indicesModified)
        {
            auto stats = processor.getModificationStatistics();
            g.setColour(juce::Colour(0xff10b981));
            g.setFont(juce::Font(13.0f, juce::Font::bold));

            juce::String statusText = "✏️ " + juce::String(stats.totalModifiedBins) +
                " bins modified - Click 'Apply Changes'";

            g.drawText(statusText,
                contentArea.getX(), contentArea.getY() + 50,
                contentArea.getWidth(), 20,
                juce::Justification::centred);
        }

        // Warning
        if (indexDB.hasSampleLoaded() && processor.areFeaturesModified())
        {
            auto warningArea = contentArea.removeFromTop(40);
            g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.9f));
            g.fillRoundedRectangle(warningArea.toFloat().reduced(5), 6.0f);

            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("⚠️ Spectral indices OUTDATED - click 'Analyze Indices'",
                warningArea.reduced(10), juce::Justification::centredLeft);
        }

        // Info tooltip
        if (contentArea.contains(getMouseXYRelative()) && !isPanning)
        {
            paintMagnitudeInfo(g, contentArea);
        }

        // Zoom info
        if (zoomVertical > 1.01f || zoomHorizontal > 1.01f)
        {
            drawZoomInfo(g, contentArea);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().removeFromBottom(170).reduced(10);

        // Buttons
        auto buttonArea = area.removeFromTop(40);
        analyzeButton.setBounds(buttonArea.removeFromLeft(140).withHeight(35));
        buttonArea.removeFromLeft(10);
        applyButton.setBounds(buttonArea.removeFromLeft(140).withHeight(35));
        buttonArea.removeFromLeft(10);
        clearCacheButton.setBounds(buttonArea.removeFromLeft(120).withHeight(35));
        buttonArea.removeFromLeft(10);
        clearModificationsButton.setBounds(buttonArea.removeFromLeft(120).withHeight(35));
        buttonArea.removeFromLeft(20);
        resetZoomButton.setBounds(buttonArea.removeFromLeft(100).withHeight(35));

        area.removeFromTop(10);

        // Tools - Row 1
        auto toolArea1 = area.removeFromTop(35);
        toolLabel.setBounds(toolArea1.removeFromLeft(50));
        toolArea1.removeFromLeft(5);
        paintToolButton.setBounds(toolArea1.removeFromLeft(120).withHeight(30));
        toolArea1.removeFromLeft(5);
        amplifyToolButton.setBounds(toolArea1.removeFromLeft(120).withHeight(30));
        toolArea1.removeFromLeft(5);
        attenuateToolButton.setBounds(toolArea1.removeFromLeft(130).withHeight(30));

        area.removeFromTop(5);

        // Tools - Row 2
        auto toolArea2 = area.removeFromTop(35);
        toolArea2.removeFromLeft(55); // Align with row 1
        removeToolButton.setBounds(toolArea2.removeFromLeft(120).withHeight(30));
        toolArea2.removeFromLeft(5);
        lineToolButton.setBounds(toolArea2.removeFromLeft(120).withHeight(30));
        toolArea2.removeFromLeft(5);
        noiseToolButton.setBounds(toolArea2.removeFromLeft(120).withHeight(30));

        toolArea2.removeFromLeft(20);

        // Parameters
        toolSizeLabel.setBounds(toolArea2.removeFromLeft(45));
        toolArea2.removeFromLeft(5);
        toolSizeSlider.setBounds(toolArea2.removeFromLeft(120).withHeight(30));
        toolArea2.removeFromLeft(15);
        toolIntensityLabel.setBounds(toolArea2.removeFromLeft(65));
        toolArea2.removeFromLeft(5);
        toolIntensitySlider.setBounds(toolArea2.removeFromLeft(120).withHeight(30));

        // Zoom sliders
        auto mainArea = getLocalBounds().reduced(10);
        mainArea.removeFromBottom(180);

        int verticalSliderWidth = 30;
        int horizontalSliderHeight = 30;

        auto verticalSliderArea = mainArea.removeFromRight(verticalSliderWidth);
        verticalSliderArea.removeFromBottom(horizontalSliderHeight + 5);
        verticalZoomSlider.setBounds(verticalSliderArea);

        auto horizontalSliderArea = mainArea.removeFromBottom(horizontalSliderHeight);
        horizontalZoomSlider.setBounds(horizontalSliderArea);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto contentArea = getSpectrogramArea();

        if (!contentArea.contains(e.position.toInt()))
            return;

        if (e.mods.isMiddleButtonDown())
        {
            isPanning = true;
            panStartPosition = e.position;
            panStartOffsetX = panOffsetX;
            panStartOffsetY = panOffsetY;
            return;
        }

        if (e.mods.isLeftButtonDown())
        {
            if (currentTool == Tool::Line)
            {
                if (!isDrawingLine)
                {
                    // Start line
                    lineStartPos = e.position;
                    isDrawingLine = true;
                }
                else
                {
                    // Finish line
                    drawLine(lineStartPos, e.position);
                    isDrawingLine = false;
                }
            }
            else
            {
                isEditing = true;
                lastEditPos = e.position;
                paintAtPosition(e.position);
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isPanning)
        {
            auto contentArea = getSpectrogramArea();
            float deltaX = (e.position.x - panStartPosition.x) / contentArea.getWidth();
            float deltaY = (e.position.y - panStartPosition.y) / contentArea.getHeight();

            float viewWidth = 1.0f / zoomHorizontal;
            float viewHeight = 1.0f / zoomVertical;

            panOffsetX = panStartOffsetX - deltaX * viewWidth;
            panOffsetY = panStartOffsetY + deltaY * viewHeight;

            constrainZoomAndPan();
            repaint();
        }
        else if (isEditing && currentTool != Tool::Line)
        {
            interpolatePaint(lastEditPos, e.position);
            lastEditPos = e.position;
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (isPanning)
        {
            isPanning = false;
        }

        if (isEditing)
        {
            isEditing = false;

            if (indicesModified)
            {
                applyButton.setEnabled(true);
                clearModificationsButton.setEnabled(true);
            }
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (e.mods.isMiddleButtonDown())
        {
            resetZoom();
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        if (currentTool == Tool::Line && isDrawingLine)
        {
            repaint(); // For line preview
        }
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;

    // UI Components
    juce::TextButton analyzeButton, applyButton, clearCacheButton, clearModificationsButton;
    juce::TextButton resetZoomButton;
    juce::Label toolLabel;
    juce::TextButton paintToolButton, amplifyToolButton, attenuateToolButton;
    juce::TextButton removeToolButton, lineToolButton, noiseToolButton;
    juce::Label toolSizeLabel, toolIntensityLabel;
    juce::Slider toolSizeSlider, toolIntensitySlider;
    juce::Slider horizontalZoomSlider, verticalZoomSlider;

    // Cache
    juce::Image cachedSpectrogram;
    bool spectrogramNeedsUpdate = true;
    float lastCachedZoomH = 1.0f;
    float lastCachedZoomV = 1.0f;
    float lastCachedPanX = 0.0f;
    float lastCachedPanY = 0.0f;

    // Fixed dB range
    float fixedDbFloor = -80.0f;
    float fixedDbCeiling = 0.0f;
    bool useFixedDbRange = true;

    // State
    enum class Tool { Paint, Amplify, Attenuate, Remove, Line, Noise };
    Tool currentTool = Tool::Paint;

    bool isEditing = false;
    juce::Point<float> lastEditPos;
    bool indicesModified = false;

    // Line tool state
    bool isDrawingLine = false;
    juce::Point<float> lineStartPos;

    // Zoom & Pan
    float zoomVertical = 1.0f;
    float zoomHorizontal = 1.0f;
    float panOffsetX = 0.0f;
    float panOffsetY = 0.0f;

    bool isPanning = false;
    juce::Point<float> panStartPosition;
    float panStartOffsetX = 0.0f;
    float panStartOffsetY = 0.0f;

    const float maxZoom = 20.0f;
    const float minZoom = 1.0f;

    // Random generator for noise tool
    juce::Random random;

    // ============================================================================
    // UTILITIES
    // ============================================================================

    juce::Rectangle<int> getSpectrogramArea() const
    {
        auto bounds = getLocalBounds().reduced(10);
        bounds.removeFromBottom(180);
        bounds.removeFromRight(35);
        bounds.removeFromBottom(35);
        return bounds;
    }

    void resetZoom()
    {
        zoomVertical = 1.0f;
        zoomHorizontal = 1.0f;
        panOffsetX = 0.0f;
        panOffsetY = 0.0f;

        horizontalZoomSlider.setValue(1.0, juce::dontSendNotification);
        verticalZoomSlider.setValue(1.0, juce::dontSendNotification);
        repaint();
    }

    void constrainZoomAndPan()
    {
        zoomVertical = juce::jlimit(minZoom, maxZoom, zoomVertical);
        zoomHorizontal = juce::jlimit(minZoom, maxZoom, zoomHorizontal);

        float viewWidth = 1.0f / zoomHorizontal;
        float viewHeight = 1.0f / zoomVertical;

        float maxPanX = juce::jmax(0.0f, 1.0f - viewWidth);
        float maxPanY = juce::jmax(0.0f, 1.0f - viewHeight);

        panOffsetX = juce::jlimit(0.0f, maxPanX, panOffsetX);
        panOffsetY = juce::jlimit(0.0f, maxPanY, panOffsetY);
    }

    void screenToDataCoords(
        juce::Point<float> screenPos,
        juce::Rectangle<int> contentArea,
        float& normalizedX,
        float& normalizedY)
    {
        float localX = (screenPos.x - contentArea.getX()) / (float)contentArea.getWidth();
        float localY = (screenPos.y - contentArea.getY()) / (float)contentArea.getHeight();

        float viewWidth = 1.0f / zoomHorizontal;
        float viewHeight = 1.0f / zoomVertical;

        normalizedX = panOffsetX + localX * viewWidth;
        normalizedY = 1.0f - (panOffsetY + localY * viewHeight);

        normalizedX = juce::jlimit(0.0f, 1.0f, normalizedX);
        normalizedY = juce::jlimit(0.0f, 1.0f, normalizedY);
    }

    void drawZoomInfo(juce::Graphics& g, juce::Rectangle<int> area)
    {
        juce::String zoomText = "🔍 ";

        if (std::abs(zoomHorizontal - zoomVertical) < 0.01f)
        {
            zoomText += juce::String(zoomHorizontal, 1) + "x";
        }
        else
        {
            zoomText += "H:" + juce::String(zoomHorizontal, 1) + "x  V:" +
                juce::String(zoomVertical, 1) + "x";
        }

        g.setColour(juce::Colour(0xff3b82f6).withAlpha(0.9f));
        g.setFont(juce::Font(11.0f, juce::Font::bold));

        int textWidth = 150;
        g.fillRoundedRectangle(area.getRight() - textWidth - 15,
            area.getBottom() - 35,
            textWidth, 25, 4.0f);

        g.setColour(juce::Colours::white);
        g.drawText(zoomText,
            area.getRight() - textWidth - 15,
            area.getBottom() - 35,
            textWidth, 25,
            juce::Justification::centred);
    }

    // ============================================================================
    // SPECTROGRAM DRAWING (same as before)
    // ============================================================================

    void drawSpectrogramWithImageData(
        juce::Graphics& g,
        juce::Rectangle<int> area,
        const SpectralIndexData& indices)
    {
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(area.toFloat(), 8.0f);

        if (indices.getNumFrames() == 0)
            return;

        int width = area.getWidth();
        int height = area.getHeight();

        bool zoomOrPanChanged =
            std::abs(zoomHorizontal - lastCachedZoomH) > 0.001f ||
            std::abs(zoomVertical - lastCachedZoomV) > 0.001f ||
            std::abs(panOffsetX - lastCachedPanX) > 0.001f ||
            std::abs(panOffsetY - lastCachedPanY) > 0.001f;

        if (zoomOrPanChanged)
        {
            spectrogramNeedsUpdate = true;
        }

        if (!spectrogramNeedsUpdate && cachedSpectrogram.isValid() &&
            cachedSpectrogram.getWidth() == width && cachedSpectrogram.getHeight() == height)
        {
            g.drawImageAt(cachedSpectrogram, area.getX(), area.getY());
            drawFrequencyGrid(g, area, static_cast<float>(indices.getParams().sampleRate));
            drawTimeGrid(g, area, indices);
            drawHeader(g, area, indices);
            return;
        }

        cachedSpectrogram = juce::Image(juce::Image::RGB, width, height, true);

        int numFrames = indices.getNumFrames();
        int numBins = indices.getNumBins();
        float sampleRate = static_cast<float>(indices.getParams().sampleRate);
        float nyquist = sampleRate / 2.0f;

        std::vector<std::vector<float>> magnitudeDB(numFrames);

        for (int f = 0; f < numFrames; ++f)
        {
            const auto& frame = indices.getFrame(f);
            magnitudeDB[f].resize(numBins);

            for (int b = 0; b < numBins; ++b)
            {
                float mag = frame.indices[b].magnitude;
                const float MIN_MAG = 1e-10f;
                float magDB = 20.0f * std::log10(juce::jmax(mag, MIN_MAG));
                magnitudeDB[f][b] = magDB;
            }
        }

        float dbFloor = fixedDbFloor;
        float dbCeiling = fixedDbCeiling;
        float dynamicRange = dbCeiling - dbFloor;

        for (int py = 0; py < height; ++py)
        {
            const float minFreq = 20.0f;
            float viewHeight = 1.0f / zoomVertical;
            float screenNormY = static_cast<float>(py) / height;
            float zoomedNormY = panOffsetY + screenNormY * viewHeight;
            zoomedNormY = juce::jlimit(0.0f, 1.0f, zoomedNormY);

            const float freqNorm = 1.0f - zoomedNormY;
            const float logMin = std::log10(minFreq);
            const float logMax = std::log10(nyquist);
            const float freq = std::pow(10.0f, logMin + freqNorm * (logMax - logMin));
            const float binFloat = (freq / nyquist) * numBins;

            for (int px = 0; px < width; ++px)
            {
                float viewWidth = 1.0f / zoomHorizontal;
                float screenNormX = static_cast<float>(px) / width;
                float zoomedNormX = panOffsetX + screenNormX * viewWidth;
                zoomedNormX = juce::jlimit(0.0f, 1.0f, zoomedNormX);

                const float frameFloat = zoomedNormX * numFrames;

                const int f0 = static_cast<int>(frameFloat);
                const int f1 = juce::jmin(f0 + 1, numFrames - 1);
                const int b0 = static_cast<int>(binFloat);
                const int b1 = juce::jmin(b0 + 1, numBins - 1);

                const float fx = frameFloat - f0;
                const float fy = binFloat - b0;

                if (f0 >= 0 && f0 < numFrames && b0 >= 0 && b0 < numBins)
                {
                    const float db00 = magnitudeDB[f0][b0];
                    const float db10 = (f1 < numFrames) ? magnitudeDB[f1][b0] : db00;
                    const float db01 = (b1 < numBins) ? magnitudeDB[f0][b1] : db00;
                    const float db11 = (f1 < numFrames && b1 < numBins) ?
                        magnitudeDB[f1][b1] : db00;

                    const float db0 = db00 * (1.0f - fx) + db10 * fx;
                    const float db1 = db01 * (1.0f - fx) + db11 * fx;
                    const float dbValue = db0 * (1.0f - fy) + db1 * fy;

                    float normalized = (dbValue - dbFloor) / dynamicRange;
                    normalized = juce::jlimit(0.0f, 1.0f, normalized);

                    const juce::Colour color = getHotColor(normalized);
                    cachedSpectrogram.setPixelAt(px, py, color);
                }
            }
        }

        spectrogramNeedsUpdate = false;
        lastCachedZoomH = zoomHorizontal;
        lastCachedZoomV = zoomVertical;
        lastCachedPanX = panOffsetX;
        lastCachedPanY = panOffsetY;

        g.drawImageAt(cachedSpectrogram, area.getX(), area.getY());
        drawFrequencyGrid(g, area, sampleRate);
        drawTimeGrid(g, area, indices);
        drawHeader(g, area, indices);
    }

    juce::Colour getHotColor(float intensity) const
    {
        const float i = juce::jlimit(0.0f, 1.0f, intensity);
        const int r = juce::jmin(255, static_cast<int>(i * 3.0f * 255.0f));
        const int g = juce::jmax(0, juce::jmin(255, static_cast<int>((i * 3.0f - 1.0f) * 255.0f)));
        const int b = juce::jmax(0, juce::jmin(255, static_cast<int>((i * 3.0f - 2.0f) * 255.0f)));
        return juce::Colour(static_cast<juce::uint8>(r),
            static_cast<juce::uint8>(g),
            static_cast<juce::uint8>(b));
    }

    void drawFrequencyGrid(juce::Graphics& g, juce::Rectangle<int> area, float sampleRate)
    {
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.setFont(juce::Font(10.0f));

        const float nyquist = sampleRate / 2.0f;
        const std::vector<int> freqMarkers = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };

        for (int freq : freqMarkers)
        {
            if (freq > nyquist) continue;

            const float minFreq = 20.0f;
            const float logMin = std::log10(minFreq);
            const float logMax = std::log10(nyquist);
            const float logFreq = std::log10(static_cast<float>(freq));
            float freqNorm = (logFreq - logMin) / (logMax - logMin);

            float viewHeight = 1.0f / zoomVertical;

            if (freqNorm < panOffsetY || freqNorm > panOffsetY + viewHeight)
                continue;

            float screenNormY = (freqNorm - panOffsetY) / viewHeight;
            screenNormY = 1.0f - screenNormY;

            const float y = area.getHeight() * screenNormY;

            g.drawLine(static_cast<float>(area.getX()), area.getY() + y,
                static_cast<float>(area.getRight()), area.getY() + y, 1.0f);

            juce::String label = freq >= 1000
                ? juce::String(freq / 1000) + "k"
                : juce::String(freq);
            g.drawText(label + "Hz", area.getX() + 5, static_cast<int>(area.getY() + y - 12),
                60, 12, juce::Justification::centredLeft);
        }
    }

    void drawTimeGrid(juce::Graphics& g, juce::Rectangle<int> area, const SpectralIndexData& indices)
    {
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.setFont(juce::Font(10.0f));

        const float duration = indices.getAllFrames().back().timePosition;
        const int numMarkers = 10;

        for (int i = 0; i <= numMarkers; ++i)
        {
            float timeNorm = static_cast<float>(i) / numMarkers;
            float viewWidth = 1.0f / zoomHorizontal;

            if (timeNorm < panOffsetX || timeNorm > panOffsetX + viewWidth)
                continue;

            float screenNormX = (timeNorm - panOffsetX) / viewWidth;
            const float x = area.getX() + screenNormX * area.getWidth();
            const float time = timeNorm * duration;

            g.drawLine(x, static_cast<float>(area.getY()),
                x, static_cast<float>(area.getBottom()), 1.0f);

            g.drawText(juce::String(time, 2) + "s", static_cast<int>(x + 3),
                area.getY() + 5, 60, 12, juce::Justification::centredLeft);
        }
    }

    void drawHeader(juce::Graphics& g, juce::Rectangle<int> area,
        const SpectralIndexData& indices)
    {
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("📊 SPECTRAL OVERVIEW", area.getX() + 10, area.getY() + 10, 300, 20,
            juce::Justification::centredLeft);

        g.setFont(juce::Font(11.0f));
        int numFrames = indices.getNumFrames();
        int numBins = indices.getNumBins();
        juce::String stats = juce::String(numFrames) + " frames × " +
            juce::String(numBins) + " bins";
        g.drawText(stats, area.getX() + 10, area.getY() + 30, 400, 15,
            juce::Justification::centredLeft);
    }

    void paintMagnitudeInfo(juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto mousePos = getMouseXYRelative();
        if (!area.contains(mousePos.toInt()))
            return;

        const SpectralIndexData* indices = processor.getIndexDatabase().getOverviewIndices();
        if (!indices || indices->getNumFrames() == 0)
            return;

        float normalizedX, normalizedY;
        screenToDataCoords(mousePos.toFloat(), area, normalizedX, normalizedY);

        int frameIdx = static_cast<int>(normalizedX * indices->getNumFrames());

        float sampleRate = static_cast<float>(indices->getParams().sampleRate);
        float nyquist = sampleRate / 2.0f;
        const float minFreq = 20.0f;
        const float logMin = std::log10(minFreq);
        const float logMax = std::log10(nyquist);
        const float freq = std::pow(10.0f, logMin + normalizedY * (logMax - logMin));
        int binIdx = static_cast<int>((freq / nyquist) * indices->getNumBins());

        if (frameIdx >= 0 && frameIdx < indices->getNumFrames() &&
            binIdx >= 0 && binIdx < indices->getNumBins())
        {
            auto index = indices->getIndex(frameIdx, binIdx);
            float time = indices->getFrame(frameIdx).timePosition;

            int infoWidth = 320;
            int infoHeight = 70;
            int infoX = area.getRight() - infoWidth - 10;
            int infoY = area.getY() + 10;

            g.setColour(juce::Colour(0xff1a1a1a).withAlpha(0.95f));
            g.fillRoundedRectangle(infoX, infoY, infoWidth, infoHeight, 6.0f);

            g.setColour(juce::Colour(0xff3b82f6));
            g.drawRoundedRectangle(infoX, infoY, infoWidth, infoHeight, 6.0f, 2.0f);

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(11.0f, juce::Font::bold));

            juce::String info1 = "🎯 " + juce::String(time, 3) + "s  |  " +
                juce::String(freq, 0) + " Hz";
            g.drawText(info1, infoX + 10, infoY + 5, infoWidth - 20, 20,
                juce::Justification::centredLeft);

            g.setFont(juce::Font(10.0f));
            g.drawText("Magnitude: " + juce::String(index.magnitude, 4),
                infoX + 10, infoY + 23, infoWidth - 20, 15,
                juce::Justification::centredLeft);

            float magDB = 20.0f * std::log10(index.magnitude + 0.00001f);
            g.drawText(juce::String(magDB, 1) + " dB",
                infoX + 10, infoY + 38, infoWidth - 20, 15,
                juce::Justification::centredLeft);
        }
    }

    // ============================================================================
    // TOOL MANAGEMENT
    // ============================================================================

    void setTool(Tool tool)
    {
        currentTool = tool;

        // Reset colors
        paintToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
        amplifyToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
        attenuateToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
        removeToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
        lineToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));
        noiseToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff374151));

        // Highlight active tool
        switch (tool)
        {
        case Tool::Paint:
            paintToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981));
            break;
        case Tool::Amplify:
            amplifyToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b82f6));
            break;
        case Tool::Attenuate:
            attenuateToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b));
            break;
        case Tool::Remove:
            removeToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffef4444));
            break;
        case Tool::Line:
            lineToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b));
            break;
        case Tool::Noise:
            noiseToolButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8b5cf6));
            break;
        }

        repaint();
    }

    void interpolatePaint(juce::Point<float> from, juce::Point<float> to)
    {
        float distance = from.getDistanceFrom(to);
        int steps = juce::jmax(1, static_cast<int>(distance / 2.0f));

        for (int i = 0; i <= steps; ++i)
        {
            float t = i / static_cast<float>(steps);
            juce::Point<float> interpolated = from + (to - from) * t;
            paintAtPosition(interpolated);
        }
    }

    // ✅ NEW: Draw line between two points
    void drawLine(juce::Point<float> start, juce::Point<float> end)
    {
        float distance = start.getDistanceFrom(end);
        int steps = juce::jmax(1, static_cast<int>(distance));

        for (int i = 0; i <= steps; ++i)
        {
            float t = i / static_cast<float>(steps);
            juce::Point<float> pos = start + (end - start) * t;
            paintAtPosition(pos);
        }
    }

    float analyzeSurroundingMagnitude(
        const SpectralIndexData* indices,
        int frameIdx, int binIdx, int radius)
    {
        if (!indices)
            return 0.0f;

        float totalMag = 0.0f;
        int count = 0;

        for (int df = -radius; df <= radius; ++df)
        {
            for (int db = -radius; db <= radius; ++db)
            {
                int targetFrame = frameIdx + df;
                int targetBin = binIdx + db;

                if (targetFrame < 0 || targetFrame >= indices->getNumFrames() ||
                    targetBin < 0 || targetBin >= indices->getNumBins())
                    continue;

                if (df == 0 && db == 0)
                    continue;

                auto idx = indices->getIndex(targetFrame, targetBin);
                totalMag += idx.magnitude;
                count++;
            }
        }

        return (count > 0) ? (totalMag / count) : 0.0f;
    }

    // ============================================================================
    // ✅ NEW: Complete paint logic with all tools
    // ============================================================================

    void paintAtPosition(juce::Point<float> pos)
    {
        auto contentArea = getSpectrogramArea();

        if (!contentArea.contains(pos.toInt()))
            return;

        const SpectralIndexData* indices = processor.getIndexDatabase().getOverviewIndices();
        if (!indices)
            return;

        float normalizedX, normalizedY;
        screenToDataCoords(pos, contentArea, normalizedX, normalizedY);

        int frameIdx = static_cast<int>(normalizedX * indices->getNumFrames());

        float sampleRate = static_cast<float>(indices->getParams().sampleRate);
        float nyquist = sampleRate / 2.0f;
        const float minFreq = 20.0f;
        const float logMin = std::log10(minFreq);
        const float logMax = std::log10(nyquist);
        const float freq = std::pow(10.0f, logMin + normalizedY * (logMax - logMin));
        int binIdx = static_cast<int>((freq / nyquist) * indices->getNumBins());

        if (frameIdx < 0 || frameIdx >= indices->getNumFrames() ||
            binIdx < 0 || binIdx >= indices->getNumBins())
            return;

        float toolSize = static_cast<float>(toolSizeSlider.getValue());
        float intensity = static_cast<float>(toolIntensitySlider.getValue());

        int toolRadius = static_cast<int>(std::ceil(toolSize));

        float surroundingMag = analyzeSurroundingMagnitude(indices, frameIdx, binIdx,
            toolRadius * 2);

        for (int df = -toolRadius; df <= toolRadius; ++df)
        {
            for (int db = -toolRadius; db <= toolRadius; ++db)
            {
                int targetFrame = frameIdx + df;
                int targetBin = binIdx + db;

                if (targetFrame < 0 || targetFrame >= indices->getNumFrames() ||
                    targetBin < 0 || targetBin >= indices->getNumBins())
                    continue;

                float distance = std::sqrt(static_cast<float>(df * df + db * db));
                if (distance > toolSize)
                    continue;

                float weight = std::exp(-(distance * distance) /
                    (2.0f * toolSize * toolSize / 9.0f));

                auto currentIndex = indices->getIndex(targetFrame, targetBin);
                float originalMag = currentIndex.originalMagnitude;

                if (originalMag < 0.0001f)
                    originalMag = currentIndex.magnitude;

                float newMagnitude = currentIndex.magnitude;

                switch (currentTool)
                {
                case Tool::Paint:
                {
                    // Create/boost with content-awareness
                    if (currentIndex.magnitude < 0.001f)
                    {
                        if (surroundingMag > 0.001f)
                        {
                            newMagnitude = surroundingMag * intensity * weight * 0.5f;
                        }
                        else
                        {
                            newMagnitude = 0.01f * intensity * weight;
                        }
                    }
                    else
                    {
                        float boostFactor = 1.0f + (intensity * weight * 0.3f);
                        newMagnitude = currentIndex.magnitude * boostFactor;

                        float maxAllowed = juce::jmax(originalMag, currentIndex.magnitude) * 3.0f;
                        newMagnitude = juce::jmin(newMagnitude, maxAllowed);

                        if (newMagnitude > originalMag * 2.0f)
                        {
                            float excess = newMagnitude - originalMag * 2.0f;
                            excess = std::tanh(excess / juce::jmax(originalMag, 0.001f)) * originalMag;
                            newMagnitude = originalMag * 2.0f + excess;
                        }
                    }
                    break;
                }

                case Tool::Amplify:
                {
                    // Amplify existing (like dodge in Photoshop)
                    float amplifyFactor = 1.0f + (intensity * weight * 0.5f);
                    newMagnitude = currentIndex.magnitude * amplifyFactor;

                    // Limit to 4x original
                    float maxAllowed = juce::jmax(originalMag, 0.001f) * 4.0f;
                    newMagnitude = juce::jmin(newMagnitude, maxAllowed);
                    break;
                }

                case Tool::Attenuate:
                {
                    // Blend into surroundings
                    if (surroundingMag > 0.0001f)
                    {
                        float targetMag = surroundingMag;
                        float blendFactor = intensity * weight;
                        newMagnitude = currentIndex.magnitude * (1.0f - blendFactor) +
                            targetMag * blendFactor;
                    }
                    else
                    {
                        float attenuation = 1.0f - (intensity * weight * 0.7f);
                        attenuation = juce::jmax(0.01f, attenuation);
                        newMagnitude = currentIndex.magnitude * attenuation;
                    }

                    float minAllowed = originalMag * 0.01f;
                    newMagnitude = juce::jmax(newMagnitude, minAllowed);
                    break;
                }

                case Tool::Remove:
                {
                    // Complete removal/muting
                    float removeFactor = intensity * weight;
                    newMagnitude = currentIndex.magnitude * (1.0f - removeFactor);

                    // Can go down to near-zero
                    newMagnitude = juce::jmax(newMagnitude, 0.0001f);
                    break;
                }

                case Tool::Line:
                {
                    // Same as Paint for line tool
                    if (currentIndex.magnitude < 0.001f)
                    {
                        newMagnitude = 0.01f * intensity * weight;
                    }
                    else
                    {
                        float boostFactor = 1.0f + (intensity * weight * 0.3f);
                        newMagnitude = currentIndex.magnitude * boostFactor;
                        newMagnitude = juce::jmin(newMagnitude, originalMag * 3.0f);
                    }
                    break;
                }

                case Tool::Noise:
                {
                    // Add randomization/noise
                    float noiseFactor = (random.nextFloat() * 2.0f - 1.0f) * intensity * weight;

                    if (currentIndex.magnitude < 0.001f)
                    {
                        // Create noise from nothing
                        newMagnitude = std::abs(noiseFactor) * 0.01f;
                    }
                    else
                    {
                        // Add variation to existing
                        newMagnitude = currentIndex.magnitude * (1.0f + noiseFactor * 0.3f);
                        newMagnitude = juce::jmax(0.0001f, newMagnitude);

                        // Keep within reasonable bounds
                        float maxAllowed = originalMag * 3.0f;
                        newMagnitude = juce::jmin(newMagnitude, maxAllowed);
                    }
                    break;
                }
                }

                float newPhase = currentIndex.phase;
                processor.modifyIndexAt(targetFrame, targetBin, newMagnitude, newPhase);
            }
        }

        indicesModified = true;
        spectrogramNeedsUpdate = true;
    }

    // Apply/Analyze (same as before...)

    void applyModifications()
    {
        if (!indicesModified)
        {
            DBG("⚠️ No modifications");
            return;
        }

        auto stats = processor.getModificationStatistics();
        if (stats.totalModifiedBins == 0)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "⚠️ No Changes",
                "No modifications detected.",
                "OK"
            );
            return;
        }

        processor.synthesizeFromModifiedIndices();

        indicesModified = false;
        applyButton.setEnabled(false);
        clearModificationsButton.setEnabled(true);
        spectrogramNeedsUpdate = true;
        repaint();

        juce::String message = "✅ Spectral modifications applied!\n\n";
        message += "• " + juce::String(stats.totalModifiedBins) + " frequency bins\n";
        message += "• " + juce::String(stats.totalModifiedFrames) + " time frames\n";
        message += "• Frequency: " + juce::String(stats.minModifiedFreq, 0) + "-" +
            juce::String(stats.maxModifiedFreq, 0) + " Hz\n";
        message += "• Time: " + juce::String(stats.minModifiedTime, 2) + "-" +
            juce::String(stats.maxModifiedTime, 2) + " s";

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "✅ Applied Successfully",
            message,
            "OK"
        );
    }

    void analyzeIndices()
    {
        if (!processor.hasSampleLoaded())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "⚠️ No Sample",
                "Please load a sample first.",
                "OK"
            );
            return;
        }

        processor.analyzeSpectralIndices();

        auto* indices = const_cast<SpectralIndexData*>(
            processor.getIndexDatabase().getOverviewIndices());
        if (indices)
        {
            indices->clearAllModifications();
        }

        indicesModified = false;
        applyButton.setEnabled(false);
        clearModificationsButton.setEnabled(false);
        spectrogramNeedsUpdate = true;
        repaint();

        auto stats = processor.getIndexDatabase().getStatistics();

        juce::String message = "Spectral analysis complete!\n\n";
        message += "• " + juce::String(stats.overviewFrames) + " time frames\n";
        message += "• " + juce::String(stats.overviewBins) + " frequency bins\n\n";
        message += "🖌️ Paint - Create/boost\n";
        message += "📈 Amplify - Boost existing\n";
        message += "🌫️ Attenuate - Blend/blur\n";
        message += "🗑️ Remove - Delete frequencies\n";
        message += "📏 Line - Draw straight lines\n";
        message += "🎲 Noise - Add randomization";

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "✅ Analysis Complete",
            message,
            "OK"
        );
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralIndexPanel)
};