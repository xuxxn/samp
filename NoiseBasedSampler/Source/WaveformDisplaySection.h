/*
WaveformDisplaySection.h - CENTER WAVEFORM/INDEX DISPLAY SECTION
âœ… Chart type tabs (Amplitude/Frequency/Phase/Volume/Pan/Spectral)
âœ… Spectral control buttons (Analyze/Apply/Clear/Reset)
âœ… Waveform visualization
âœ… Editing tools
âœ… Zoom/pan/mouse handlers
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class WaveformDisplaySection : public juce::Component,
    public juce::Timer,
    public juce::FileDragAndDropTarget,
    public juce::KeyListener
{
public:
    enum class ChartType
    {
        Amplitude,
        Frequency,
        Phase,
        Volume,
        Pan,
        Spectral
    };

enum class EditTool
    {
        Brush,
        Line,
        RegionSelect,
        VerticalScale,
        Paint,
        Amplify,
        Attenuate,
        Remove,
        Noise
    };

    enum class BrushMode
    {
        Relief,
        Straight,
        Triangle,
        Square,
        Noise
    };

    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (auto& file : files)
        {
            if (file.endsWithIgnoreCase(".wav") || file.endsWithIgnoreCase(".mp3") ||
                file.endsWithIgnoreCase(".aif") || file.endsWithIgnoreCase(".aiff"))
                return true;
        }
        return false;
    }

    void setEditTool(EditTool tool)
    {
        currentEditTool = tool;
        DBG("✅ Edit tool changed to: " + getEditToolName(tool));
        repaint();
    }

EditTool getCurrentEditTool() const
    {
        return currentEditTool;
    }

    void setBrushMode(BrushMode mode)
    {
        currentBrushMode = mode;
        DBG("✅ Brush mode changed to: " + getBrushModeName(mode));
        repaint();
    }

    BrushMode getCurrentBrushMode() const
    {
        return currentBrushMode;
    }

    ChartType getCurrentChartType() const
    {
        return currentChartType;
    }

    void analyzeSpectralIndices()
    {
        if (analyzeIndicesButton.isVisible() && analyzeIndicesButton.isEnabled())
        {
            // Trigger analyze button click
            analyzeIndicesButton.triggerClick();
        }
    }

    void applySpectralModifications()
    {
        if (applySpectralButton.isVisible() && applySpectralButton.isEnabled())
        {
            // Trigger apply button click
            applySpectralButton.triggerClick();
        }
    }

    void clearSpectralEdits()
    {
        if (clearSpectralButton.isVisible() && clearSpectralButton.isEnabled())
        {
            // Trigger clear button click
            clearSpectralButton.triggerClick();
        }
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        if (files.size() > 0)
        {
            juce::File file(files[0]);
            processor.loadSample(file);
            resetZoom();
        }
    }

bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        // ESC key - hide brush mode menu
        if (key == juce::KeyPress::escapeKey && brushModeMenuVisible)
        {
            hideBrushModeSelection();
            return true;
        }

        // Space key - show brush mode selection
        if (key == juce::KeyPress::spaceKey && currentEditTool == EditTool::Brush && currentChartType != ChartType::Spectral)
        {
            showBrushModeSelection();
            return true;
        }

        // Number keys 1-5 - quick brush mode selection
        if (currentEditTool == EditTool::Brush && currentChartType != ChartType::Spectral)
        {
            if (key.getKeyCode() >= '1' && key.getKeyCode() <= '5')
            {
                switch (key.getKeyCode() - '1')
                {
                case 0: setBrushMode(BrushMode::Relief); break;
                case 1: setBrushMode(BrushMode::Straight); break;
                case 2: setBrushMode(BrushMode::Triangle); break;
                case 3: setBrushMode(BrushMode::Square); break;
                case 4: setBrushMode(BrushMode::Noise); break;
                }
                return true;
            }
        }

        // Delete key
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            handleDeleteKey();
            return true;
        }

        return false;
    }

    WaveformDisplaySection(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        startTimerHz(30);

        // Chart type tabs
        addAndMakeVisible(amplitudeButton);
        amplitudeButton.setButtonText("amplitude");
        amplitudeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b82f6));
        amplitudeButton.onClick = [this] { setChartType(ChartType::Amplitude); };

        addAndMakeVisible(frequencyButton);
        frequencyButton.setButtonText("frequency");
        frequencyButton.onClick = [this] { setChartType(ChartType::Frequency); };

        addAndMakeVisible(phaseButton);
        phaseButton.setButtonText("phase");
        phaseButton.onClick = [this] { setChartType(ChartType::Phase); };

        addAndMakeVisible(volumeButton);
        volumeButton.setButtonText("volume");
        volumeButton.onClick = [this] { setChartType(ChartType::Volume); };

        addAndMakeVisible(panButton);
        panButton.setButtonText("pan");
        panButton.onClick = [this] { setChartType(ChartType::Pan); };

        addAndMakeVisible(spectralButton);
        spectralButton.setButtonText("spectral");
        spectralButton.onClick = [this] { setChartType(ChartType::Spectral); };

        // Spectral control buttons
        addAndMakeVisible(analyzeIndicesButton);
        analyzeIndicesButton.setButtonText("analyze indices");
        analyzeIndicesButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3b82f6));
        analyzeIndicesButton.onClick = [this] { analyzeSpectralIndices(); };

        addAndMakeVisible(applySpectralButton);
        applySpectralButton.setButtonText("apply changes");
        applySpectralButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981));
        applySpectralButton.setEnabled(false);
        applySpectralButton.onClick = [this] { applySpectralModifications(); };

        addAndMakeVisible(clearSpectralButton);
        clearSpectralButton.setButtonText("clear edits");
        clearSpectralButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffef4444));
        clearSpectralButton.setEnabled(false);
        clearSpectralButton.onClick = [this] {
            processor.clearAllModifications();
            spectralIndicesModified = false;
            applySpectralButton.setEnabled(false);
            clearSpectralButton.setEnabled(false);
            spectrogramNeedsUpdate = true;
            repaint();
            };

        addAndMakeVisible(resetZoomButton);
        resetZoomButton.setButtonText("Reset View");
        resetZoomButton.onClick = [this] { resetZoom(); };

        // Editing tools
        addAndMakeVisible(toolSizeLabel);
        toolSizeLabel.setText("size:", juce::dontSendNotification);
        toolSizeLabel.setFont(juce::Font(11.0f));

        addAndMakeVisible(toolSizeSlider);
        toolSizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        toolSizeSlider.setRange(1.0, 20.0, 1.0);
        toolSizeSlider.setValue(5.0);
        toolSizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);

        addAndMakeVisible(toolIntensityLabel);
        toolIntensityLabel.setText("intensity:", juce::dontSendNotification);
        toolIntensityLabel.setFont(juce::Font(11.0f));

        addAndMakeVisible(toolIntensitySlider);
        toolIntensitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
        toolIntensitySlider.setRange(0.1, 2.0, 0.1);
        toolIntensitySlider.setValue(1.0);
        toolIntensitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);

        setChartType(ChartType::Amplitude);
        setEditTool(EditTool::Brush);
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2d2d2d));

        if (currentChartType == ChartType::Spectral)
        {
            paintSpectralView(g);
            auto chartArea = waveArea.reduced(10);
            drawPlayPositionMarker(g, chartArea);
        }
        else
        {
            paintWaveArea(g);
        }

        // Zoom indicator
        if (horizontalZoom > 1.0f || verticalZoom > 1.0f)
        {
            g.setColour(juce::Colour(0xff3b82f6).withAlpha(0.8f));
            g.setFont(juce::Font(11.0f, juce::Font::bold));

            juce::String zoomText = "ðŸ”";
            if (std::abs(horizontalZoom - verticalZoom) < 0.01f && horizontalZoom > 1.0f)
            {
                zoomText += juce::String(horizontalZoom, 1) + "x";
            }
            else
            {
                if (horizontalZoom > 1.0f)
                    zoomText += "H:" + juce::String(horizontalZoom, 1) + "x ";
                if (verticalZoom > 1.0f)
                    zoomText += "V:" + juce::String(verticalZoom, 1) + "x";
            }

            zoomText += " | Shift+drag to PAN";

            g.drawText(zoomText,
                waveArea.getX() + 10, waveArea.getY() + 10, 300, 20,
                juce::Justification::centredLeft);
        }

// Instructions
        g.setColour(juce::Colours::grey);
        g.setFont(10.0f);
        juce::String instructions = getInstructionsText();
        
        // Add brush mode info if using brush
        if (currentEditTool == EditTool::Brush && currentChartType != ChartType::Spectral)
        {
            instructions += " | Brush: " + getBrushModeName(currentBrushMode) + " (SPACE for modes)";
        }
        
        g.drawText(instructions,
            waveArea.getX(), waveArea.getBottom() + 5,
            waveArea.getWidth(), 15,
            juce::Justification::centred);
            
        // Draw brush mode selection menu
        drawBrushModeMenu(g);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Chart type tabs
        auto tabArea = area.removeFromTop(40);
        int buttonWidth = 100;
        int spacing = 5;

        amplitudeButton.setBounds(tabArea.removeFromLeft(buttonWidth));
        tabArea.removeFromLeft(spacing);
        frequencyButton.setBounds(tabArea.removeFromLeft(buttonWidth));
        tabArea.removeFromLeft(spacing);
        phaseButton.setBounds(tabArea.removeFromLeft(buttonWidth));
        tabArea.removeFromLeft(spacing);
        volumeButton.setBounds(tabArea.removeFromLeft(buttonWidth));
        tabArea.removeFromLeft(spacing);
        panButton.setBounds(tabArea.removeFromLeft(buttonWidth));
        tabArea.removeFromLeft(spacing);
        spectralButton.setBounds(tabArea.removeFromLeft(buttonWidth));

        area.removeFromTop(10);

        // Spectral control buttons
        bool isSpectral = (currentChartType == ChartType::Spectral);

        analyzeIndicesButton.setVisible(isSpectral);
        applySpectralButton.setVisible(isSpectral);
        clearSpectralButton.setVisible(isSpectral);
        resetZoomButton.setVisible(isSpectral);
        toolSizeLabel.setVisible(isSpectral);
        toolSizeSlider.setVisible(isSpectral);
        toolIntensityLabel.setVisible(isSpectral);
        toolIntensitySlider.setVisible(isSpectral);

        if (isSpectral)
        {
            auto spectralControlArea = area.removeFromTop(40);
            analyzeIndicesButton.setBounds(spectralControlArea.removeFromLeft(140));
            spectralControlArea.removeFromLeft(10);
            applySpectralButton.setBounds(spectralControlArea.removeFromLeft(140));
            spectralControlArea.removeFromLeft(10);
            clearSpectralButton.setBounds(spectralControlArea.removeFromLeft(140));
            spectralControlArea.removeFromLeft(10);
            resetZoomButton.setBounds(spectralControlArea.removeFromLeft(140));

            area.removeFromTop(10);

            // Tool controls for spectral
            auto toolArea = area.removeFromTop(30);
            toolSizeLabel.setBounds(toolArea.removeFromLeft(50));
            toolSizeSlider.setBounds(toolArea.removeFromLeft(150));
            toolArea.removeFromLeft(20);
            toolIntensityLabel.setBounds(toolArea.removeFromLeft(80));
            toolIntensitySlider.setBounds(toolArea.removeFromLeft(150));

            area.removeFromTop(10);
        }

        waveArea = area;
    }

    void timerCallback() override
    {
        updatePlayPosition();
        repaint();
    }

    // Getter methods for key detection
    juce::Rectangle<float> getWaveArea() const { return waveArea.toFloat(); }
    float getPanOffsetX() const { return panOffsetX; }
    float getHorizontalZoom() const { return horizontalZoom; }

void mouseDown(const juce::MouseEvent& e) override
    {
        // Handle brush mode menu clicks first
        if (brushModeMenuVisible)
        {
            handleBrushModeMenuClick(e.position);
            return;
        }

        if (!waveArea.contains(e.position.toInt()))
            return;

        if (currentChartType == ChartType::Spectral)
        {
            handleSpectralMouseDown(e);
        }
        else
        {
            handleSampleLevelMouseDown(e);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (currentChartType == ChartType::Spectral)
        {
            handleSpectralMouseDrag(e);
        }
        else
        {
            handleSampleLevelMouseDrag(e);
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (currentChartType == ChartType::Spectral)
        {
            handleSpectralMouseUp(e);
        }
        else
        {
            handleSampleLevelMouseUp(e);
        }
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (!waveArea.contains(e.position.toInt()))
            return;

        bool isCtrl = e.mods.isCtrlDown() || e.mods.isCommandDown();
        float zoomDelta = wheel.deltaY * 0.5f;

        if (isCtrl)
        {
            applyVerticalZoom(e, zoomDelta);
        }
        else
        {
            applyHorizontalZoom(e, zoomDelta);
        }

        repaint();
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;

ChartType currentChartType = ChartType::Amplitude;
    EditTool currentEditTool = EditTool::Brush;
    BrushMode currentBrushMode = BrushMode::Straight;

    juce::TextButton amplitudeButton, frequencyButton, phaseButton;
    juce::TextButton volumeButton, panButton, spectralButton;
    juce::TextButton analyzeIndicesButton, applySpectralButton, clearSpectralButton;
    juce::TextButton resetZoomButton;

    juce::Label toolSizeLabel, toolIntensityLabel;
    juce::Slider toolSizeSlider, toolIntensitySlider;

    juce::Rectangle<int> waveArea;

    float horizontalZoom = 1.0f;
    float verticalZoom = 1.0f;
    float panOffsetX = 0.0f;
    float panOffsetY = 0.0f;

juce::String getEditToolName(EditTool tool)
    {
        switch (tool)
        {
        case EditTool::Brush:         return "Brush";
        case EditTool::Line:          return "Line";
        case EditTool::RegionSelect:  return "Region Select";
        case EditTool::VerticalScale: return "Vertical Scale";
        case EditTool::Paint:         return "Paint";
        case EditTool::Amplify:       return "Amplify";
        case EditTool::Attenuate:     return "Attenuate";
        case EditTool::Remove:        return "Remove";
        case EditTool::Noise:         return "Noise";
        default:                      return "Unknown";
        }
    }

    juce::String getBrushModeName(BrushMode mode)
    {
        switch (mode)
        {
        case BrushMode::Relief:   return "Relief Appreciation";
        case BrushMode::Straight:  return "Straight";
        case BrushMode::Triangle:  return "Triangle";
        case BrushMode::Square:    return "Square";
        case BrushMode::Noise:     return "Noise";
        default:                   return "Unknown";
        }
    }

    bool spectrogramNeedsUpdate = true;
    juce::Image cachedSpectrogram;
    bool spectralIndicesModified = false;

    bool isSpectralEditing = false;
    bool isSpectralPanning = false;
    juce::Point<float> spectralPanStart;

    float lastCachedZoomH = 1.0f;
    float lastCachedZoomV = 1.0f;
    float lastCachedPanX = 0.0f;
    float lastCachedPanY = 0.0f;

juce::Random randomGenerator;

    static constexpr int kNumVisualSegments = 30;
    float playMarkerBlinkPhase = 0.0f;

    // Brush mode selection menu
    bool brushModeMenuVisible = false;
    juce::Rectangle<int> brushModeMenuBounds;
    std::vector<juce::Rectangle<int>> brushModeButtonBounds;

    struct PlayMarker
    {
        float currentPlayPosition = 0.0f;
        bool isPlaying = false;
        int currentVisualSegment = 0;
        int voiceIndex = -1;
        float pitchRatio = 1.0f;
        float lastSegmentTime = 0.0f;
    };

    std::vector<PlayMarker> activeMarkers;

    // ========== SAMPLE-LEVEL EDITING ==========

    void modifyFeatureAtPosition(juce::Point<float> pos)
    {
        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        if (numSamples == 0)
            return;

        auto chartArea = waveArea.reduced(10);
        chartArea.removeFromTop(20);

        if (!chartArea.contains(pos.toInt()))
            return;

        float normalizedX = (pos.x - chartArea.getX()) / chartArea.getWidth();
        normalizedX = juce::jlimit(0.0f, 1.0f, normalizedX);

        int startSample = static_cast<int>(panOffsetX * numSamples);
        int visibleSamples = static_cast<int>((numSamples / horizontalZoom));
        int sampleIndex = startSample + static_cast<int>(normalizedX * visibleSamples);
        sampleIndex = juce::jlimit(0, numSamples - 1, sampleIndex);

        float normalizedY = 1.0f - ((pos.y - chartArea.getY()) / chartArea.getHeight());
        normalizedY = juce::jlimit(0.0f, 1.0f, normalizedY);

        auto stats = features.calculateStatistics();

int baseRadius = 15;
        int smoothRadius = std::max(3, static_cast<int>(baseRadius / std::sqrt(horizontalZoom)));

        // Apply different brush modes
        auto applyBrushMode = [&](float currentValue, float targetValue, int distanceFromCenter) -> float
        {
            switch (currentBrushMode)
            {
            case BrushMode::Relief:
                // Preserve existing relief, don't create new bumps
                if (std::abs(currentValue - targetValue) < 0.01f)
                    return currentValue; // Keep existing relief
                else
                    return targetValue; // Follow user drawing
                
            case BrushMode::Straight:
                // Maximum precision drawing
                return targetValue;
                
            case BrushMode::Triangle:
            {
                // Create triangular waves
                float phase = (distanceFromCenter % 20) / 20.0f * juce::MathConstants<float>::twoPi;
                float triangleWave = 2.0f * std::asin(std::sin(phase)) / juce::MathConstants<float>::pi;
                return targetValue + triangleWave * 0.1f;
            }
            
            case BrushMode::Square:
            {
                // Create square waves
                float phase = (distanceFromCenter % 15) / 15.0f * juce::MathConstants<float>::twoPi;
                float squareWave = (std::sin(phase) > 0) ? 1.0f : -1.0f;
                return targetValue + squareWave * 0.1f;
            }
            
            case BrushMode::Noise:
            {
                // Add random noise
                float noise = (randomGenerator.nextFloat() * 2.0f - 1.0f) * 0.15f;
                return targetValue + noise;
            }
            
            default:
                return targetValue;
            }
        };

        switch (currentChartType)
        {
case ChartType::Amplitude:
        {
            float value = (normalizedY - 0.5f) * 2.0f;
            processor.setFeatureAmplitudeAt(sampleIndex, value);

            for (int i = -smoothRadius; i <= smoothRadius; ++i)
            {
                int idx = sampleIndex + i;
                if (idx >= 0 && idx < numSamples && i != 0)
                {
                    float currentValue = features[idx].amplitude;
                    
                    if (currentBrushMode == BrushMode::Relief)
                    {
                        // Relief mode: preserve existing characteristics
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float blendedValue = applyBrushMode(currentValue, value, i);
                        float smoothedValue = currentValue + (blendedValue - currentValue) * weight * 0.5f;
                        processor.setFeatureAmplitudeAt(idx, smoothedValue);
                    }
                    else if (currentBrushMode == BrushMode::Straight)
                    {
                        // Straight mode: precise smooth interpolation
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float smoothedValue = currentValue + (value - currentValue) * weight * 0.8f;
                        processor.setFeatureAmplitudeAt(idx, smoothedValue);
                    }
                    else
                    {
                        // Pattern modes: apply brush mode directly
                        float patternValue = applyBrushMode(currentValue, value, std::abs(i));
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 12.0f));
                        float finalValue = currentValue + (patternValue - currentValue) * weight * 0.6f;
                        processor.setFeatureAmplitudeAt(idx, finalValue);
                    }
                }
            }
            break;
        }

case ChartType::Frequency:
        {
            float freqRange = stats.maxFrequency - stats.minFrequency;
            if (freqRange < 1.0f) freqRange = 1000.0f;

            float value = stats.minFrequency + normalizedY * freqRange;
            value = juce::jlimit(20.0f, 20000.0f, value);
            processor.setFeatureFrequencyAt(sampleIndex, value);

            for (int i = -smoothRadius; i <= smoothRadius; ++i)
            {
                int idx = sampleIndex + i;
                if (idx >= 0 && idx < numSamples && i != 0)
                {
                    float currentValue = features[idx].frequency;
                    
                    if (currentBrushMode == BrushMode::Relief)
                    {
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float blendedValue = applyBrushMode(currentValue, value, i);
                        float smoothedValue = currentValue + (blendedValue - currentValue) * weight * 0.5f;
                        processor.setFeatureFrequencyAt(idx, smoothedValue);
                    }
                    else if (currentBrushMode == BrushMode::Straight)
                    {
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float smoothedValue = currentValue + (value - currentValue) * weight * 0.8f;
                        processor.setFeatureFrequencyAt(idx, smoothedValue);
                    }
                    else
                    {
                        float patternValue = applyBrushMode(currentValue, value, std::abs(i));
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 12.0f));
                        float finalValue = currentValue + (patternValue - currentValue) * weight * 0.6f;
                        processor.setFeatureFrequencyAt(idx, finalValue);
                    }
                }
            }
            break;
        }

case ChartType::Phase:
        {
            float value = normalizedY * juce::MathConstants<float>::twoPi;
            processor.setFeaturePhaseAt(sampleIndex, value);

            for (int i = -smoothRadius; i <= smoothRadius; ++i)
            {
                int idx = sampleIndex + i;
                if (idx >= 0 && idx < numSamples && i != 0)
                {
                    float currentValue = features[idx].phase;
                    
                    if (currentBrushMode == BrushMode::Relief)
                    {
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float blendedValue = applyBrushMode(currentValue, value, i);
                        float smoothedValue = currentValue + (blendedValue - currentValue) * weight * 0.5f;
                        processor.setFeaturePhaseAt(idx, smoothedValue);
                    }
                    else if (currentBrushMode == BrushMode::Straight)
                    {
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float smoothedValue = currentValue + (value - currentValue) * weight * 0.8f;
                        processor.setFeaturePhaseAt(idx, smoothedValue);
                    }
                    else
                    {
                        float patternValue = applyBrushMode(currentValue, value, std::abs(i));
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 12.0f));
                        float finalValue = currentValue + (patternValue - currentValue) * weight * 0.6f;
                        processor.setFeaturePhaseAt(idx, finalValue);
                    }
                }
            }
            break;
        }

case ChartType::Volume:
        {
            float maxVolume = juce::jmax(stats.maxVolume, 1.0f) * 1.1f;
            float value = normalizedY * maxVolume;
            value = juce::jlimit(0.0f, 2.0f, value);

            processor.setFeatureVolumeAt(sampleIndex, value);

            for (int i = -smoothRadius; i <= smoothRadius; ++i)
            {
                int idx = sampleIndex + i;
                if (idx >= 0 && idx < numSamples && i != 0)
                {
                    float currentValue = features[idx].volume;
                    
                    if (currentBrushMode == BrushMode::Relief)
                    {
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float blendedValue = applyBrushMode(currentValue, value, i);
                        float smoothedValue = currentValue + (blendedValue - currentValue) * weight * 0.5f;
                        processor.setFeatureVolumeAt(idx, smoothedValue);
                    }
                    else if (currentBrushMode == BrushMode::Straight)
                    {
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float smoothedValue = currentValue + (value - currentValue) * weight * 0.8f;
                        processor.setFeatureVolumeAt(idx, smoothedValue);
                    }
                    else
                    {
                        float patternValue = applyBrushMode(currentValue, value, std::abs(i));
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 12.0f));
                        float finalValue = currentValue + (patternValue - currentValue) * weight * 0.6f;
                        processor.setFeatureVolumeAt(idx, finalValue);
                    }
                }
            }
            break;
        }

case ChartType::Pan:
        {
            float value = normalizedY;
            processor.setFeaturePanAt(sampleIndex, value);

            for (int i = -smoothRadius; i <= smoothRadius; ++i)
            {
                int idx = sampleIndex + i;
                if (idx >= 0 && idx < numSamples && i != 0)
                {
                    float currentValue = features[idx].pan;
                    
                    if (currentBrushMode == BrushMode::Relief)
                    {
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float blendedValue = applyBrushMode(currentValue, value, i);
                        float smoothedValue = currentValue + (blendedValue - currentValue) * weight * 0.5f;
                        processor.setFeaturePanAt(idx, smoothedValue);
                    }
                    else if (currentBrushMode == BrushMode::Straight)
                    {
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                        float smoothedValue = currentValue + (value - currentValue) * weight * 0.8f;
                        processor.setFeaturePanAt(idx, smoothedValue);
                    }
                    else
                    {
                        float patternValue = applyBrushMode(currentValue, value, std::abs(i));
                        float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 12.0f));
                        float finalValue = currentValue + (patternValue - currentValue) * weight * 0.6f;
                        processor.setFeaturePanAt(idx, finalValue);
                    }
                }
            }
            break;
        }
        }

        repaint();
    }

void interpolateEditPath(juce::Point<float> from, juce::Point<float> to)
    {
        float distance = from.getDistanceFrom(to);
        
        // Adjust steps based on brush mode
        int steps;
        switch (currentBrushMode)
        {
        case BrushMode::Straight:
        case BrushMode::Relief:
            steps = std::max(1, static_cast<int>(distance / 1.0f)); // Higher precision
            break;
        case BrushMode::Triangle:
        case BrushMode::Square:
        case BrushMode::Noise:
            steps = std::max(1, static_cast<int>(distance / 3.0f)); // Lower precision for patterns
            break;
        default:
            steps = std::max(1, static_cast<int>(distance / 2.0f));
            break;
        }

        for (int i = 0; i <= steps; ++i)
        {
            float t = i / static_cast<float>(steps);
            juce::Point<float> interpolated = from + (to - from) * t;
            
            // Add some randomness for noise mode
            if (currentBrushMode == BrushMode::Noise)
            {
                float noiseAmount = 2.0f; // pixels of random offset
                interpolated.x += (randomGenerator.nextFloat() * 2.0f - 1.0f) * noiseAmount;
                interpolated.y += (randomGenerator.nextFloat() * 2.0f - 1.0f) * noiseAmount;
            }
            
            modifyFeatureAtPosition(interpolated);
        }
    }

    void applyLineEdit(juce::Point<float> start, juce::Point<float> end)
    {
        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        if (numSamples == 0)
            return;

        auto chartArea = waveArea.reduced(10);
        chartArea.removeFromTop(20);

        auto posToSampleAndValue = [&](juce::Point<float> pos) -> std::pair<int, float>
            {
                float normalizedX = (pos.x - chartArea.getX()) / chartArea.getWidth();
                normalizedX = juce::jlimit(0.0f, 1.0f, normalizedX);

                int startSample = static_cast<int>(panOffsetX * numSamples);
                int visibleSamples = static_cast<int>(numSamples / horizontalZoom);
                int sampleIndex = startSample + static_cast<int>(normalizedX * visibleSamples);
                sampleIndex = juce::jlimit(0, numSamples - 1, sampleIndex);

                float normalizedY = 1.0f - ((pos.y - chartArea.getY()) / chartArea.getHeight());
                normalizedY = juce::jlimit(0.0f, 1.0f, normalizedY);

                float value = 0.0f;
                auto stats = features.calculateStatistics();

                switch (currentChartType)
                {
                case ChartType::Amplitude:
                    value = (normalizedY - 0.5f) * 2.0f;
                    break;

                case ChartType::Frequency:
                {
                    float freqRange = stats.maxFrequency - stats.minFrequency;
                    if (freqRange < 1.0f) freqRange = 1000.0f;
                    value = stats.minFrequency + normalizedY * freqRange;
                    value = juce::jlimit(20.0f, 20000.0f, value);
                    break;
                }

                case ChartType::Phase:
                    value = normalizedY * juce::MathConstants<float>::twoPi;
                    break;

                case ChartType::Volume:
                {
                    float maxVolume = juce::jmax(stats.maxVolume, 1.0f) * 1.1f;
                    value = normalizedY * maxVolume;
                    value = juce::jlimit(0.0f, 2.0f, value);
                    break;
                }

                case ChartType::Pan:
                    value = normalizedY;
                    value = juce::jlimit(0.0f, 1.0f, value);
                    break;
                }

                return { sampleIndex, value };
            };

        auto [startSample, startValue] = posToSampleAndValue(start);
        auto [endSample, endValue] = posToSampleAndValue(end);

        if (startSample > endSample)
        {
            std::swap(startSample, endSample);
            std::swap(startValue, endValue);
        }

        int numSteps = endSample - startSample + 1;

        for (int i = 0; i < numSteps; ++i)
        {
            int sampleIdx = startSample + i;
            if (sampleIdx < 0 || sampleIdx >= numSamples)
                continue;

            float t = static_cast<float>(i) / juce::jmax(1, numSteps - 1);
            float interpolatedValue = startValue + (endValue - startValue) * t;

            switch (currentChartType)
            {
            case ChartType::Amplitude:
                processor.setFeatureAmplitudeAt(sampleIdx, interpolatedValue);
                break;

            case ChartType::Frequency:
                processor.setFeatureFrequencyAt(sampleIdx, interpolatedValue);
                break;

            case ChartType::Phase:
                processor.setFeaturePhaseAt(sampleIdx, interpolatedValue);
                break;

            case ChartType::Volume:
                processor.setFeatureVolumeAt(sampleIdx, interpolatedValue);
                break;

            case ChartType::Pan:
                processor.setFeaturePanAt(sampleIdx, interpolatedValue);
                break;
            }
        }
    }

    void applyVerticalScale(float scaleFactor)
    {
        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        if (numSamples == 0)
            return;

        int startSample = 0;
        int endSample = numSamples - 1;

        if (hasRegionSelection)
        {
            startSample = regionStartSample;
            endSample = regionEndSample;
        }

        switch (currentChartType)
        {
        case ChartType::Amplitude:
        {
            for (int i = startSample; i <= endSample; ++i)
            {
                float currentValue = features[i].amplitude;
                float newValue = currentValue * scaleFactor;
                newValue = juce::jlimit(-1.0f, 1.0f, newValue);
                processor.setFeatureAmplitudeAt(i, newValue);
            }
            break;
        }

        case ChartType::Frequency:
        {
            auto stats = features.calculateStatistics();
            float minFreq = stats.minFrequency;

            for (int i = startSample; i <= endSample; ++i)
            {
                float currentValue = features[i].frequency;
                float offset = currentValue - minFreq;
                float newValue = minFreq + (offset * scaleFactor);
                newValue = juce::jlimit(20.0f, 20000.0f, newValue);
                processor.setFeatureFrequencyAt(i, newValue);
            }
            break;
        }

        case ChartType::Phase:
        {
            float center = juce::MathConstants<float>::pi;

            for (int i = startSample; i <= endSample; ++i)
            {
                float currentValue = features[i].phase;
                float offset = currentValue - center;
                float newValue = center + (offset * scaleFactor);

                while (newValue < 0)
                    newValue += juce::MathConstants<float>::twoPi;
                while (newValue >= juce::MathConstants<float>::twoPi)
                    newValue -= juce::MathConstants<float>::twoPi;

                processor.setFeaturePhaseAt(i, newValue);
            }
            break;
        }

        case ChartType::Volume:
        {
            for (int i = startSample; i <= endSample; ++i)
            {
                float currentValue = features[i].volume;
                float newValue = currentValue * scaleFactor;
                newValue = juce::jlimit(0.0f, 2.0f, newValue);
                processor.setFeatureVolumeAt(i, newValue);
            }
            break;
        }

        case ChartType::Pan:
        {
            for (int i = startSample; i <= endSample; ++i)
            {
                float currentValue = features[i].pan;
                float offset = currentValue - 0.5f;
                float newValue = 0.5f + (offset * scaleFactor);
                newValue = juce::jlimit(0.0f, 1.0f, newValue);
                processor.setFeaturePanAt(i, newValue);
            }
            break;
        }
        }

        processor.markFeaturesAsModified();
        processor.applyFeatureChangesToSample();
    }

    void finalizeRegionSelection()
    {
        if (!processor.hasFeatureData())
            return;

        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        auto chartArea = waveArea.reduced(10);
        chartArea.removeFromTop(20);

        float startX = juce::jmin(regionDragStart.x, regionDragEnd.x);
        float endX = juce::jmax(regionDragStart.x, regionDragEnd.x);

        float normalizedStart = (startX - chartArea.getX()) / chartArea.getWidth();
        float normalizedEnd = (endX - chartArea.getX()) / chartArea.getWidth();

        normalizedStart = juce::jlimit(0.0f, 1.0f, normalizedStart);
        normalizedEnd = juce::jlimit(0.0f, 1.0f, normalizedEnd);

        int startSample = static_cast<int>(panOffsetX * numSamples);
        int visibleSamples = static_cast<int>(numSamples / horizontalZoom);

        regionStartSample = startSample + static_cast<int>(normalizedStart * visibleSamples);
        regionEndSample = startSample + static_cast<int>(normalizedEnd * visibleSamples);

        regionStartSample = juce::jlimit(0, numSamples - 1, regionStartSample);
        regionEndSample = juce::jlimit(0, numSamples - 1, regionEndSample);

        if (regionEndSample > regionStartSample)
        {
            hasRegionSelection = true;
            isRegionFocused = false;

            DBG("✅ Region created in GREEN mode: samples " + juce::String(regionStartSample) +
                " to " + juce::String(regionEndSample) + " - Press DELETE to remove content");
        }
    }

    bool isPointInRegion(juce::Point<float> point)
    {
        if (!hasRegionSelection || !processor.hasFeatureData())
            return false;

        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        auto chartArea = waveArea.reduced(10);

        float startX = chartArea.getX() +
            ((regionStartSample - panOffsetX * numSamples) / (numSamples / horizontalZoom)) * chartArea.getWidth();
        float endX = chartArea.getX() +
            ((regionEndSample - panOffsetX * numSamples) / (numSamples / horizontalZoom)) * chartArea.getWidth();

        return point.x >= startX && point.x <= endX &&
            point.y >= chartArea.getY() && point.y <= chartArea.getBottom();
    }

void handleDeleteKey()
    {
        if (!processor.hasFeatureData())
            return;

        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        if (!hasRegionSelection)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                processor.setFeatureAmplitudeAt(i, 0.0f);
            }

            processor.markFeaturesAsModified();
            processor.applyFeatureChangesToSample();
        }
        else if (isRegionFocused)
        {
            hasRegionSelection = false;
            isRegionFocused = false;
            repaint();
        }
        else
        {
            processor.removeFeatureSamples(regionStartSample, regionEndSample);
            hasRegionSelection = false;
            isRegionFocused = false;
            repaint();
        }
    }

    void paintSpectralAtPosition(juce::Point<float> pos)
    {
        auto contentArea = waveArea.reduced(10);
        if (!contentArea.contains(pos.toInt())) return;

        const SpectralIndexData* indices = processor.getIndexDatabase().getOverviewIndices();
        if (!indices) return;

        float normalizedX, normalizedY;
        screenToSpectralCoords(pos, contentArea, normalizedX, normalizedY);

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

        float surroundingMag = analyzeSurroundingMagnitude(indices, frameIdx, binIdx, toolRadius * 2);

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
                if (distance > toolSize) continue;

                float weight = std::exp(-(distance * distance) / (2.0f * toolSize * toolSize / 9.0f));

                auto currentIndex = indices->getIndex(targetFrame, targetBin);
                float originalMag = currentIndex.originalMagnitude;
                if (originalMag < 0.0001f) originalMag = currentIndex.magnitude;

                float newMagnitude = applySpectralTool(currentIndex, originalMag, surroundingMag, weight, intensity);

                processor.modifyIndexAt(targetFrame, targetBin, newMagnitude, currentIndex.phase);
            }
        }

        spectralIndicesModified = true;
        spectrogramNeedsUpdate = true;
        repaint();
    }

    float applySpectralTool(const SpectralIndex& index, float originalMag, float surroundingMag, float weight, float intensity)
    {
        float newMag = index.magnitude;

        switch (currentEditTool)
        {
        case EditTool::Paint:
        {
            if (index.magnitude < 0.001f)
            {
                if (surroundingMag > 0.001f)
                {
                    newMag = surroundingMag * intensity * weight * 0.5f;
                }
                else
                {
                    newMag = 0.01f * intensity * weight;
                }
            }
            else
            {
                float boostFactor = 1.0f + (intensity * weight * 0.3f);
                newMag = index.magnitude * boostFactor;

                float maxAllowed = juce::jmax(originalMag, index.magnitude) * 3.0f;
                newMag = juce::jmin(newMag, maxAllowed);

                if (newMag > originalMag * 2.0f)
                {
                    float excess = newMag - originalMag * 2.0f;
                    excess = std::tanh(excess / juce::jmax(originalMag, 0.001f)) * originalMag;
                    newMag = originalMag * 2.0f + excess;
                }
            }
            break;
        }

        case EditTool::Amplify:
        {
            float amplifyFactor = 1.0f + (intensity * weight * 0.5f);
            newMag = index.magnitude * amplifyFactor;
            float maxAllowed = juce::jmax(originalMag, 0.001f) * 4.0f;
            newMag = juce::jmin(newMag, maxAllowed);
            break;
        }

        case EditTool::Attenuate:
        {
            if (surroundingMag > 0.0001f)
            {
                float targetMag = surroundingMag;
                float blendFactor = intensity * weight;
                newMag = index.magnitude * (1.0f - blendFactor) + targetMag * blendFactor;
            }
            else
            {
                float attenuation = 1.0f - (intensity * weight * 0.7f);
                attenuation = juce::jmax(0.01f, attenuation);
                newMag = index.magnitude * attenuation;
            }
            float minAllowed = originalMag * 0.01f;
            newMag = juce::jmax(newMag, minAllowed);
            break;
        }

        case EditTool::Remove:
        {
            float removeFactor = intensity * weight;
            newMag = index.magnitude * (1.0f - removeFactor);
            newMag = juce::jmax(newMag, 0.0001f);
            break;
        }

        case EditTool::Noise:
        {
            float noiseFactor = (randomGenerator.nextFloat() * 2.0f - 1.0f) * intensity * weight;

            if (index.magnitude < 0.001f)
            {
                newMag = std::abs(noiseFactor) * 0.01f;
            }
            else
            {
                newMag = index.magnitude * (1.0f + noiseFactor * 0.3f);
                newMag = juce::jmax(0.0001f, newMag);
                float maxAllowed = originalMag * 3.0f;
                newMag = juce::jmin(newMag, maxAllowed);
            }
            break;
        }

        default:
            break;
        }

        return newMag;
    }

    float analyzeSurroundingMagnitude(const SpectralIndexData* indices, int frameIdx, int binIdx, int radius)
    {
        if (!indices) return 0.0f;

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

                if (df == 0 && db == 0) continue;

                auto idx = indices->getIndex(targetFrame, targetBin);
                totalMag += idx.magnitude;
                count++;
            }
        }

        return (count > 0) ? (totalMag / count) : 0.0f;
    }

    void screenToSpectralCoords(juce::Point<float> screenPos, juce::Rectangle<int> contentArea,
        float& normalizedX, float& normalizedY)
    {
        float localX = (screenPos.x - contentArea.getX()) / (float)contentArea.getWidth();
        float localY = (screenPos.y - contentArea.getY()) / (float)contentArea.getHeight();

        float viewWidth = 1.0f / horizontalZoom;
        float viewHeight = 1.0f / verticalZoom;

        normalizedX = panOffsetX + localX * viewWidth;
        normalizedY = 1.0f - (panOffsetY + localY * viewHeight);

        normalizedX = juce::jlimit(0.0f, 1.0f, normalizedX);
        normalizedY = juce::jlimit(0.0f, 1.0f, normalizedY);
    }

    void constrainSpectralPan()
    {
        float viewWidth = 1.0f / horizontalZoom;
        float viewHeight = 1.0f / verticalZoom;

        panOffsetX = juce::jlimit(0.0f, juce::jmax(0.0f, 1.0f - viewWidth), panOffsetX);
        panOffsetY = juce::jlimit(0.0f, juce::jmax(0.0f, 1.0f - viewHeight), panOffsetY);
    }

    void paintWaveArea(juce::Graphics& g)
    {
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(waveArea.toFloat(), 8.0f);

        if (!processor.hasSampleLoaded())
        {
            g.setColour(juce::Colours::grey);
            g.setFont(16.0f);
            g.drawText("Load a sample to view and edit features",
                waveArea, juce::Justification::centred);
            return;
        }

        g.setColour(juce::Colour(0xffd0d0d0));
        g.drawRoundedRectangle(waveArea.toFloat(), 8.0f, 2.0f);

        auto chartArea = waveArea.reduced(10);
        drawEditableChart(g, chartArea, currentChartType);
        drawPlayPositionMarker(g, chartArea);
    }

    void drawEditableChart(juce::Graphics& g, juce::Rectangle<int> area, ChartType type)
    {
        const auto& features = processor.getFeatureData();
        if (features.getNumSamples() == 0)
            return;

        g.saveState();
        g.reduceClipRegion(area);

        static FeatureData::Statistics cachedStats;
        static bool statsInitialized = false;

        if (!statsInitialized)
        {
            cachedStats = features.calculateStatistics();
            statsInitialized = true;
        }

        auto stats = cachedStats;

        juce::Colour chartColour;
        juce::String title;

        switch (type)
        {
        case ChartType::Amplitude:
            chartColour = juce::Colour(0xff3b82f6);
            title = "Amplitude";
            break;
        case ChartType::Frequency:
            chartColour = juce::Colour(0xff10b981);
            title = "Frequency (Hz)";
            break;
        case ChartType::Phase:
            chartColour = juce::Colour(0xfff59e0b);
            title = "Phase (radians)";
            break;
        case ChartType::Volume:
            chartColour = juce::Colour(0xffec4899);
            title = "Volume (dB Scale)";
            break;
        case ChartType::Pan:
            chartColour = juce::Colour(0xff06b6d4);
            title = "Pan (L-R Balance)";
            break;
        }

        g.setColour(chartColour);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(title, area.removeFromTop(20), juce::Justification::centredLeft);

        int numSamples = features.getNumSamples();

        int startSample = static_cast<int>(panOffsetX * numSamples);
        int endSample = static_cast<int>((panOffsetX + (1.0f / horizontalZoom)) * numSamples);
        startSample = juce::jlimit(0, numSamples - 1, startSample);
        endSample = juce::jlimit(startSample + 1, numSamples, endSample);

        int visibleSamples = endSample - startSample;
        int step = std::max(1, visibleSamples / 2400);

        float baseMin = 0.0f;
        float baseMax = 1.0f;

        switch (type)
        {
        case ChartType::Amplitude:
            baseMin = -1.0f;
            baseMax = 1.0f;
            break;
        case ChartType::Frequency:
            baseMin = stats.minFrequency * 0.95f;
            baseMax = stats.maxFrequency * 1.05f;
            break;
        case ChartType::Phase:
            baseMin = 0.0f;
            baseMax = juce::MathConstants<float>::twoPi;
            break;
        case ChartType::Volume:
            baseMin = 0.0f;
            baseMax = 2.0f;
            break;
        case ChartType::Pan:
            baseMin = 0.0f;
            baseMax = 1.0f;
            break;
        }

        float fullRange = baseMax - baseMin;
        float viewHeight = fullRange / verticalZoom;

        float centerValue = baseMin + fullRange * 0.5f;
        float visibleMin = centerValue - viewHeight * (0.5f - panOffsetY);
        float visibleMax = centerValue + viewHeight * (0.5f + panOffsetY);

        if (type == ChartType::Amplitude)
        {
            float absMax = juce::jmax(std::abs(visibleMin), std::abs(visibleMax));
            visibleMin = -absMax;
            visibleMax = absMax;
        }

        float valueRange = visibleMax - visibleMin;
        if (valueRange < 0.0001f)
            valueRange = 1.0f;

        juce::Path path;
        bool firstPoint = true;

        for (int i = startSample; i < endSample; i += step)
        {
            float normalizedX = static_cast<float>(i - startSample) / visibleSamples;
            float x = area.getX() + normalizedX * area.getWidth();

            float value = 0.0f;
            float y = 0.0f;

            switch (type)
            {
            case ChartType::Amplitude:
                value = features[i].amplitude;
                {
                    float normalized = (value - visibleMin) / valueRange;
                    y = juce::jlimit(
                        (float)area.getY(),
                        (float)area.getBottom(),
                        area.getBottom() - normalized * area.getHeight()
                    );
                }
                break;

            case ChartType::Frequency:
                value = features[i].frequency;
                {
                    float normalized = (value - visibleMin) / valueRange;
                    y = juce::jlimit(
                        (float)area.getY(),
                        (float)area.getBottom(),
                        area.getBottom() - normalized * area.getHeight() * 0.95f
                    );
                }
                break;

            case ChartType::Phase:
                value = features[i].phase;
                {
                    float normalized = (value - visibleMin) / valueRange;
                    y = juce::jlimit(
                        (float)area.getY(),
                        (float)area.getBottom(),
                        area.getBottom() - normalized * area.getHeight() * 0.95f
                    );
                }
                break;

            case ChartType::Volume:
                value = features[i].volume;
                {
                    float normalized = (value - visibleMin) / valueRange;
                    y = juce::jlimit(
                        (float)area.getY(),
                        (float)area.getBottom(),
                        area.getBottom() - normalized * area.getHeight() * 0.95f
                    );
                }
                break;

            case ChartType::Pan:
                value = features[i].pan;
                {
                    float normalized = (value - visibleMin) / valueRange;
                    y = juce::jlimit(
                        (float)area.getY(),
                        (float)area.getBottom(),
                        area.getBottom() - normalized * area.getHeight() * 0.95f
                    );
                }
                break;
            }

            if (firstPoint)
            {
                path.startNewSubPath(x, y);
                firstPoint = false;
            }
            else
            {
                path.lineTo(x, y);
            }
        }

        juce::Path fillPath = path;
        fillPath.lineTo(area.getRight(), area.getBottom());
        fillPath.lineTo(area.getX(), area.getBottom());
        fillPath.closeSubPath();

        g.setColour(chartColour.withAlpha(0.1f));
        g.fillPath(fillPath);

        g.setColour(chartColour);
        g.strokePath(path, juce::PathStrokeType(2.0f));

        g.restoreState();
    }

    void drawPlayPositionMarker(juce::Graphics& g, juce::Rectangle<int> chartArea)
    {
        if (!processor.hasSampleLoaded() || activeMarkers.empty())
            return;

        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        if (numSamples == 0)
            return;

        int startSample = static_cast<int>(panOffsetX * numSamples);
        int endSample = static_cast<int>((panOffsetX + (1.0f / horizontalZoom)) * numSamples);
        startSample = juce::jlimit(0, numSamples - 1, startSample);
        endSample = juce::jlimit(startSample + 1, numSamples, endSample);

        int visibleSamples = endSample - startSample;

        for (const auto& marker : activeMarkers)
        {
            float segmentCenter = (marker.currentVisualSegment + 0.5f) / kNumVisualSegments;
            int segmentSample = static_cast<int>(segmentCenter * numSamples);

            if (segmentSample < startSample || segmentSample >= endSample)
                continue;

            float normalizedVisibleX = static_cast<float>(segmentSample - startSample) / visibleSamples;
            float markerX = chartArea.getX() + normalizedVisibleX * chartArea.getWidth();

            drawNegativeMarker(g, chartArea, markerX, marker);
        }
    }

    void drawNegativeMarker(juce::Graphics& g, juce::Rectangle<int> chartArea, float markerX, const PlayMarker& marker)
    {
        juce::Rectangle<float> markerRect(markerX - 6, static_cast<float>(chartArea.getY()),
            12.0f, static_cast<float>(chartArea.getHeight()));

        g.saveState();

        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.fillRect(markerRect);

        g.setColour(juce::Colours::cyan);
        g.drawRect(markerRect, 2.0f);

        g.restoreState();

        juce::Colour stepColour = getVoiceColor(marker.voiceIndex);
        g.setColour(stepColour);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        juce::String segmentText = "V" + juce::String(marker.voiceIndex + 1) + " SEG" + juce::String(marker.currentVisualSegment + 1) + "/" + juce::String(kNumVisualSegments);
        g.drawText(segmentText, markerX + 15, chartArea.getY() + 5, 100, 15, juce::Justification::centredLeft);
    }

    juce::Colour getVoiceColor(int voiceIndex) const
    {
        static const juce::Colour voiceColors[] = {
            juce::Colours::cyan, juce::Colours::magenta, juce::Colours::yellow,
            juce::Colours::lime, juce::Colours::orange, juce::Colours::pink,
            juce::Colours::lightblue, juce::Colours::lightcoral
        };
        return voiceColors[voiceIndex % 8];
    }

    void paintSpectralView(juce::Graphics& g)
    {
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(waveArea.toFloat(), 8.0f);

        const auto& indexDB = processor.getIndexDatabase();

        if (!indexDB.hasSampleLoaded())
        {
            g.setColour(juce::Colours::white);
            g.setFont(20.0f);
            g.drawText("Click 'Analyze Indices' to view spectral data",
                waveArea.reduced(20), juce::Justification::centred);
            return;
        }

        const auto* indices = indexDB.getOverviewIndices();
        if (indices)
        {
            drawSpectrogramWithImageData(g, waveArea.reduced(10), *indices);
        }

        // Modification status
        if (spectralIndicesModified)
        {
            auto stats = processor.getModificationStatistics();
            g.setColour(juce::Colour(0xff10b981));
            g.setFont(juce::Font(13.0f, juce::Font::bold));

            juce::String statusText = "Modified: " + juce::String(stats.totalModifiedBins) +
                " bins - Click 'Apply Changes'";

            auto contentArea = waveArea.reduced(10);
            g.drawText(statusText,
                contentArea.getX(), contentArea.getY() + 50,
                contentArea.getWidth(), 20,
                juce::Justification::centred);
        }
    }

    void drawSpectrogramWithImageData(juce::Graphics& g, juce::Rectangle<int> area, const SpectralIndexData& indices)
    {
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(area.toFloat(), 8.0f);

        if (indices.getNumFrames() == 0)
            return;

        int width = area.getWidth();
        int height = area.getHeight();

        bool zoomOrPanChanged =
            std::abs(horizontalZoom - lastCachedZoomH) > 0.001f ||
            std::abs(verticalZoom - lastCachedZoomV) > 0.001f ||
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

        float dbFloor = -80.0f;
        float dbCeiling = 0.0f;
        float dynamicRange = dbCeiling - dbFloor;

        for (int py = 0; py < height; ++py)
        {
            const float minFreq = 20.0f;
            float viewHeight = 1.0f / verticalZoom;
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
                float viewWidth = 1.0f / horizontalZoom;
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
        lastCachedZoomH = horizontalZoom;
        lastCachedZoomV = verticalZoom;
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

            float viewHeight = 1.0f / verticalZoom;

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

            float viewWidth = 1.0f / horizontalZoom;

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

    void drawHeader(juce::Graphics& g, juce::Rectangle<int> area, const SpectralIndexData& indices)
    {
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText("SPECTRAL OVERVIEW", area.getX() + 10, area.getY() + 10, 300, 20,
            juce::Justification::centredLeft);

        g.setFont(juce::Font(11.0f));
        int numFrames = indices.getNumFrames();
        int numBins = indices.getNumBins();
        juce::String stats = juce::String(numFrames) + " frames x " +
            juce::String(numBins) + " bins";
        g.drawText(stats, area.getX() + 10, area.getY() + 30, 400, 15,
            juce::Justification::centredLeft);
    }

    juce::String getInstructionsText() const
    {
        juce::String instructions;

        if (currentChartType == ChartType::Spectral)
        {
            instructions = "Spectral: ";
            switch (currentEditTool)
            {
            case EditTool::Paint:      instructions += "Click & drag to paint | "; break;
            case EditTool::Amplify:    instructions += "Click & drag to amplify | "; break;
            case EditTool::Attenuate:  instructions += "Click & drag to attenuate | "; break;
            case EditTool::Remove:     instructions += "Click & drag to remove | "; break;
            case EditTool::Noise:      instructions += "Click & drag to add noise | "; break;
            default: break;
            }
        }
        else
        {
            instructions = "Sample-level: ";
            switch (currentEditTool)
            {
            case EditTool::Brush:         instructions += "Click & drag to edit | "; break;
            case EditTool::Line:          instructions += "Drag to draw line | "; break;
            case EditTool::RegionSelect:  instructions += "Drag to select | DELETE to remove | "; break;
            case EditTool::VerticalScale: instructions += "Drag handle to scale | "; break;
            default: break;
            }
        }

        instructions += "Alt+Wheel: H-Zoom | Ctrl+Wheel: V-Zoom | Shift+Drag: Pan";

        return instructions;
    }

    void resetZoom()
    {
        horizontalZoom = 1.0f;
        verticalZoom = 1.0f;
        panOffsetX = 0.0f;
        panOffsetY = 0.0f;
        spectrogramNeedsUpdate = true;
        repaint();
    }

    // Sample-level editing state
    bool isDragging = false;
    bool isPanning = false;
    bool isDrawingLine = false;
    bool isDrawingRegion = false;
    bool isDraggingVerticalScale = false;
    float verticalScaleDragStartY = 0.0f;
    float verticalScaleFactor = 1.0f;
    juce::Point<float> lastEditPos;
    juce::Point<float> lastMousePos;
    juce::Point<float> lineStartPos;
    juce::Point<float> lineEndPos;
    juce::Point<float> regionDragStart;
    juce::Point<float> regionDragEnd;
    bool hasRegionSelection = false;
    bool isRegionFocused = false;
    int regionStartSample = 0;
    int regionEndSample = 0;

    void setChartType(ChartType type)
    {
        currentChartType = type;

        if (processor.hasFeatureData() && type != ChartType::Spectral)
        {
            switch (type)
            {
            case ChartType::Frequency:
                if (!processor.areFrequenciesComputed())
                    processor.computeFrequencies();
                break;
            case ChartType::Phase:
                if (!processor.arePhasesComputed())
                    processor.computePhases();
                break;
            case ChartType::Volume:
                if (!processor.areVolumesComputed())
                    processor.computeVolumes();
                break;
            case ChartType::Pan:
                if (!processor.arePansComputed())
                    processor.computePans();
                break;
            default:
                break;
            }
        }

        amplitudeButton.setColour(juce::TextButton::buttonColourId,
            type == ChartType::Amplitude ? juce::Colour(0xff3b82f6) : juce::Colour(0xff4a4a4a));
        frequencyButton.setColour(juce::TextButton::buttonColourId,
            type == ChartType::Frequency ? juce::Colour(0xff3b82f6) : juce::Colour(0xff4a4a4a));
        phaseButton.setColour(juce::TextButton::buttonColourId,
            type == ChartType::Phase ? juce::Colour(0xff3b82f6) : juce::Colour(0xff4a4a4a));
        volumeButton.setColour(juce::TextButton::buttonColourId,
            type == ChartType::Volume ? juce::Colour(0xff3b82f6) : juce::Colour(0xff4a4a4a));
        panButton.setColour(juce::TextButton::buttonColourId,
            type == ChartType::Pan ? juce::Colour(0xff3b82f6) : juce::Colour(0xff4a4a4a));
        spectralButton.setColour(juce::TextButton::buttonColourId,
            type == ChartType::Spectral ? juce::Colour(0xff3b82f6) : juce::Colour(0xff4a4a4a));

        if (type == ChartType::Spectral)
            setEditTool(EditTool::Paint);
        else
            setEditTool(EditTool::Brush);

        resetZoom();
        resized();
        repaint();
    }



    void applyHorizontalZoom(const juce::MouseEvent& e, float zoomDelta)
    {
        // ✅ ИСПРАВЛЕНО: Работает для ВСЕХ chart types
        if (currentChartType == ChartType::Spectral)
        {
            // Spectral chart zoom
            auto contentArea = waveArea.reduced(10);
            const auto* indices = processor.getIndexDatabase().getOverviewIndices();
            if (!indices || indices->getNumFrames() == 0)
                return;

            float mouseNormX = (e.position.x - contentArea.getX()) / (float)contentArea.getWidth();
            mouseNormX = juce::jlimit(0.0f, 1.0f, mouseNormX);

            float viewWidth = 1.0f / horizontalZoom;
            float timeUnderMouse = panOffsetX + mouseNormX * viewWidth;

            horizontalZoom *= (1.0f + zoomDelta);
            horizontalZoom = juce::jlimit(0.5f, 20.0f, horizontalZoom);

            if (horizontalZoom <= 0.5f)
            {
                horizontalZoom = 0.5f;
                panOffsetX = 0.0f;
                spectrogramNeedsUpdate = true;
                return;
            }

            float newViewWidth = 1.0f / horizontalZoom;
            panOffsetX = timeUnderMouse - mouseNormX * newViewWidth;
            panOffsetX = juce::jlimit(0.0f, juce::jmax(0.0f, 1.0f - newViewWidth), panOffsetX);

            spectrogramNeedsUpdate = true;
        }
        else
        {
            // ✅ НОВОЕ: Feature charts zoom (Amplitude, Frequency, Phase, Volume, Pan)
            auto chartArea = waveArea.reduced(10);
            chartArea.removeFromTop(20);  // Skip title

            if (!processor.hasFeatureData())
                return;

            const auto& features = processor.getFeatureData();
            int numSamples = features.getNumSamples();
            if (numSamples == 0)
                return;

            // Calculate mouse position in normalized coordinates
            float mouseNormX = (e.position.x - chartArea.getX()) / (float)chartArea.getWidth();
            mouseNormX = juce::jlimit(0.0f, 1.0f, mouseNormX);

            // Calculate which sample is under mouse
            int startSample = static_cast<int>(panOffsetX * numSamples);
            int visibleSamples = static_cast<int>(numSamples / horizontalZoom);
            int sampleUnderMouse = startSample + static_cast<int>(mouseNormX * visibleSamples);

            // Apply zoom
            horizontalZoom *= (1.0f + zoomDelta);
            horizontalZoom = juce::jlimit(1.0f, 50.0f, horizontalZoom);

            if (horizontalZoom <= 1.0f)
            {
                horizontalZoom = 1.0f;
                panOffsetX = 0.0f;
                repaint();
                return;
            }

            // Recalculate pan to keep sample under mouse in same position
            int newVisibleSamples = static_cast<int>(numSamples / horizontalZoom);
            int newStartSample = sampleUnderMouse - static_cast<int>(mouseNormX * newVisibleSamples);
            newStartSample = juce::jlimit(0, numSamples - newVisibleSamples, newStartSample);

            panOffsetX = newStartSample / static_cast<float>(numSamples);
            panOffsetX = juce::jlimit(0.0f, juce::jmax(0.0f, 1.0f - (1.0f / horizontalZoom)), panOffsetX);

            repaint();
        }
    }

    void applyVerticalZoom(const juce::MouseEvent& e, float zoomDelta)
    {
        if (currentChartType == ChartType::Spectral)
        {
            auto contentArea = waveArea.reduced(10);
            const auto* indices = processor.getIndexDatabase().getOverviewIndices();
            if (!indices || indices->getNumFrames() == 0)
                return;

            float mouseNormY = (e.position.y - contentArea.getY()) / (float)contentArea.getHeight();
            mouseNormY = juce::jlimit(0.0f, 1.0f, mouseNormY);

            float freqNormUnderMouse = 1.0f - mouseNormY;
            float viewHeight = 1.0f / verticalZoom;
            float freqPosUnderMouse = panOffsetY + (1.0f - mouseNormY) * viewHeight;

            verticalZoom *= (1.0f + zoomDelta);
            verticalZoom = juce::jlimit(0.5f, 20.0f, verticalZoom);

            if (verticalZoom <= 0.5f)
            {
                verticalZoom = 0.5f;
                panOffsetY = 0.0f;
                spectrogramNeedsUpdate = true;
                return;
            }

            float newViewHeight = 1.0f / verticalZoom;
            panOffsetY = freqPosUnderMouse - (1.0f - mouseNormY) * newViewHeight;
            panOffsetY = juce::jlimit(0.0f, juce::jmax(0.0f, 1.0f - newViewHeight), panOffsetY);

            spectrogramNeedsUpdate = true;
        }
    }

    void handleSpectralMouseDown(const juce::MouseEvent& e)
    {
        if (e.mods.isShiftDown() && e.mods.isLeftButtonDown())
        {
            isSpectralPanning = true;
            spectralPanStart = e.position;
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            return;
        }

        if (e.mods.isLeftButtonDown())
        {
            isSpectralEditing = true;
            paintSpectralAtPosition(e.position);
        }
    }

    void handleSpectralMouseDrag(const juce::MouseEvent& e)
    {
        if (isSpectralPanning)
        {
            float deltaX = (e.position.x - spectralPanStart.x) / waveArea.getWidth();
            float deltaY = (e.position.y - spectralPanStart.y) / waveArea.getHeight();

            float viewWidth = 1.0f / horizontalZoom;
            float viewHeight = 1.0f / verticalZoom;

            panOffsetX -= deltaX * viewWidth;
            panOffsetY += deltaY * viewHeight;

            constrainSpectralPan();
            spectralPanStart = e.position;
            spectrogramNeedsUpdate = true;
            repaint();
        }
        else if (isSpectralEditing)
        {
            paintSpectralAtPosition(e.position);
        }
    }

    void handleSpectralMouseUp(const juce::MouseEvent& e)
    {
        if (isSpectralPanning)
        {
            isSpectralPanning = false;
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
        }

        if (isSpectralEditing)
        {
            isSpectralEditing = false;

            if (spectralIndicesModified)
            {
                applySpectralButton.setEnabled(true);
                clearSpectralButton.setEnabled(true);
            }
        }
    }

    void handleSampleLevelMouseDown(const juce::MouseEvent& e)
    {
        if (currentEditTool == EditTool::VerticalScale)
        {
            auto chartArea = waveArea.reduced(10);
            chartArea.removeFromTop(20);
            float handleX = chartArea.getRight() + 5;
            float handleCenterY = chartArea.getCentreY();

            juce::Rectangle<float> handleRect(
                handleX - 4, handleCenterY - 15,
                8, 30
            );

            if (handleRect.contains(e.position.toFloat()))
            {
                isDraggingVerticalScale = true;
                verticalScaleDragStartY = e.position.y;
                verticalScaleFactor = 1.0f;
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
                return;
            }
        }

        if (e.mods.isShiftDown() && horizontalZoom > 1.0f)
        {
            isPanning = true;
            lastMousePos = e.position;
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }
        else
        {
            switch (currentEditTool)
            {
            case EditTool::Brush:
                isDragging = true;
                lastEditPos = e.position;
                modifyFeatureAtPosition(e.position);
                break;

            case EditTool::Line:
                isDrawingLine = true;
                lineStartPos = e.position;
                lineEndPos = e.position;
                break;

            case EditTool::RegionSelect:
                if (hasRegionSelection && isPointInRegion(e.position))
                {
                    isRegionFocused = !isRegionFocused;
                }
                else
                {
                    isDrawingRegion = true;
                    regionDragStart = e.position;
                    regionDragEnd = e.position;
                    hasRegionSelection = false;
                    isRegionFocused = false;
                }
                repaint();
                break;

            case EditTool::VerticalScale:
                break;

            default:
                break;
            }
        }
    }

    void handleSampleLevelMouseDrag(const juce::MouseEvent& e)
    {
        if (isDraggingVerticalScale)
        {
            float deltaY = verticalScaleDragStartY - e.position.y;
            verticalScaleFactor = 1.0f + (deltaY * 0.01f);
            verticalScaleFactor = juce::jlimit(0.1f, 10.0f, verticalScaleFactor);
            repaint();
            return;
        }
        if (isPanning)
        {
            float deltaX = e.position.x - lastMousePos.x;
            float deltaY = e.position.y - lastMousePos.y;

            // Horizontal panning
            if (horizontalZoom > 1.0f)
            {
                panOffsetX -= deltaX / waveArea.getWidth() * (1.0f / horizontalZoom);
                panOffsetX = juce::jlimit(0.0f, 1.0f - (1.0f / horizontalZoom), panOffsetX);
            }

            // Vertical panning
            if (verticalZoom > 1.0f)
            {
                panOffsetY += deltaY / waveArea.getHeight() * (1.0f / verticalZoom);
                panOffsetY = juce::jlimit(0.0f, 1.0f - (1.0f / verticalZoom), panOffsetY);
            }

            lastMousePos = e.position;
            repaint();
        }
        else if (isDragging && currentEditTool == EditTool::Brush)
        {
            interpolateEditPath(lastEditPos, e.position);
            lastEditPos = e.position;
        }
        else if (isDrawingLine)
        {
            lineEndPos = e.position;
            repaint();
        }
        else if (isDrawingRegion)
        {
            regionDragEnd = e.position;
            repaint();
        }
    }

    void handleSampleLevelMouseUp(const juce::MouseEvent& e)
    {
        if (isDraggingVerticalScale)
        {
            isDraggingVerticalScale = false;
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            applyVerticalScale(verticalScaleFactor);
            verticalScaleFactor = 1.0f;
            repaint();
            return;
        }
        if (isPanning)
        {
            isPanning = false;
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            return;
        }

        if (isDragging && currentEditTool == EditTool::Brush)
        {
            isDragging = false;
            processor.markFeaturesAsModified();
            processor.applyFeatureChangesToSample();
        }
        else if (isDrawingLine && currentEditTool == EditTool::Line)
        {
            isDrawingLine = false;
            applyLineEdit(lineStartPos, lineEndPos);
            processor.markFeaturesAsModified();
            processor.applyFeatureChangesToSample();
            repaint();
        }
        else if (isDrawingRegion)
        {
            isDrawingRegion = false;
            finalizeRegionSelection();
            repaint();
        }
    }

    void updatePlayPosition()
    {
        if (!processor.hasSampleLoaded())
            return;

        auto& samplePlayer = processor.getSamplePlayer();
        activeMarkers.clear();

        if (samplePlayer.isAnyVoicePlaying())
        {
            const auto& features = processor.getFeatureData();
            int numSamples = features.getNumSamples();

            if (numSamples > 0)
            {
                int voiceIndex = 0;
                samplePlayer.forEachVoice([&](const SamplePlayer::Voice* voice)
                    {
                        if (voice && voice->isPlaying && !voice->isReleasing)
                        {
                            PlayMarker marker;
                            marker.voiceIndex = voiceIndex;
                            marker.pitchRatio = static_cast<float>(voice->pitchRatio);
                            marker.isPlaying = true;
                            marker.currentPlayPosition = static_cast<float>(voice->currentPosition) / numSamples;
                            activeMarkers.push_back(marker);
                        }
                        voiceIndex++;
                    });
            }
        }

        playMarkerBlinkPhase += 0.05f;
        if (playMarkerBlinkPhase >= 1.0f)
            playMarkerBlinkPhase = 0.0f;
}

    void showBrushModeSelection()
    {
        brushModeMenuVisible = true;
        
        // Create menu bounds centered on mouse position
        juce::Point<int> mousePos = getMouseXYRelative();
        int menuWidth = 200;
        int menuHeight = 30 * 5 + 10; // 5 modes + padding
        int menuX = juce::jlimit(10, getWidth() - menuWidth - 10, mousePos.x - menuWidth / 2);
        int menuY = juce::jlimit(10, getHeight() - menuHeight - 10, mousePos.y - menuHeight / 2);
        
        brushModeMenuBounds = juce::Rectangle<int>(menuX, menuY, menuWidth, menuHeight);
        
        // Create button bounds
        brushModeButtonBounds.clear();
        for (int i = 0; i < 5; ++i)
        {
            juce::Rectangle<int> buttonBounds(menuX + 5, menuY + 5 + i * 30, menuWidth - 10, 25);
            brushModeButtonBounds.push_back(buttonBounds);
        }
        
        repaint();
    }

    void hideBrushModeSelection()
    {
        brushModeMenuVisible = false;
        brushModeButtonBounds.clear();
        repaint();
    }

    void handleBrushModeMenuClick(juce::Point<float> pos)
    {
        if (!brushModeMenuVisible) return;
        
        for (int i = 0; i < brushModeButtonBounds.size(); ++i)
        {
            if (brushModeButtonBounds[i].contains(pos.toInt()))
            {
                switch (i)
                {
                case 0: setBrushMode(BrushMode::Relief); break;
                case 1: setBrushMode(BrushMode::Straight); break;
                case 2: setBrushMode(BrushMode::Triangle); break;
                case 3: setBrushMode(BrushMode::Square); break;
                case 4: setBrushMode(BrushMode::Noise); break;
                }
                hideBrushModeSelection();
                return;
            }
        }
        
        // Click outside menu - hide it
        hideBrushModeSelection();
    }

    void drawBrushModeMenu(juce::Graphics& g)
    {
        if (!brushModeMenuVisible) return;
        
        // Draw menu background
        g.setColour(juce::Colour(0xff2d2d2d).withAlpha(0.95f));
        g.fillRoundedRectangle(brushModeMenuBounds.toFloat(), 8.0f);
        
        g.setColour(juce::Colour(0xff555555));
        g.drawRoundedRectangle(brushModeMenuBounds.toFloat(), 8.0f, 2.0f);
        
        // Draw buttons
        juce::Array<juce::String> modeNames = {
            "1. Relief Appreciation",
            "2. Straight",
            "3. Triangle",
            "4. Square", 
            "5. Noise"
        };
        
        for (int i = 0; i < brushModeButtonBounds.size(); ++i)
        {
            bool isSelected = false;
            switch (i)
            {
            case 0: isSelected = (currentBrushMode == BrushMode::Relief); break;
            case 1: isSelected = (currentBrushMode == BrushMode::Straight); break;
            case 2: isSelected = (currentBrushMode == BrushMode::Triangle); break;
            case 3: isSelected = (currentBrushMode == BrushMode::Square); break;
            case 4: isSelected = (currentBrushMode == BrushMode::Noise); break;
            }
            
            // Button background
            if (isSelected)
            {
                g.setColour(juce::Colour(0xff3b82f6).withAlpha(0.3f));
                g.fillRoundedRectangle(brushModeButtonBounds[i].toFloat(), 4.0f);
            }
            
            // Button text
            g.setColour(isSelected ? juce::Colour(0xff3b82f6) : juce::Colours::white);
            g.setFont(juce::Font(12.0f));
            g.drawText(modeNames[i], brushModeButtonBounds[i], juce::Justification::centredLeft);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplaySection)
};