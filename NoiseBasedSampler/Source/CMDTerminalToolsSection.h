/*
CMDTerminalToolsSection.h - COMPLETE VERSION WITH ADSR MODE
✅ Auto-focus keyboard
✅ Number keys 1-9 activate effects
✅ INDEX tools: brush (default), line, region, scale, peak
✅ SPECTRAL tools: brush (size/intensity), line, analyze, apply, clear
✅ ADSR in GENERAL category
✅ ADSR MODE - full ADSR editor in CMD window
✅ Proper tool selection tracking
✅ FIXED: No circular dependencies - using cached pointer
*/
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WaveformDisplaySection.h"

class CMDTerminalToolsSection : public juce::Component,
    public juce::Timer,
    public juce::MouseListener
{
public:
    enum class Category
    {
        General = 0,
        Index,
        Lfo
    };

    enum class ViewMode
    {
        Tools,
        ADSR,
        Boost,
        Loop,
        Stretch,
        Pitch
    };

    struct Tool
    {
        juce::String name;
        bool isSimple;
        std::function<void()> action;
        std::function<bool()> isActive;
        std::function<juce::String()> getStatusText;
    };

    // ✅ NEW: Brush modes for INDEX category
    bool showBrushModes = false;
    std::vector<Tool> brushModeTools;

    CMDTerminalToolsSection(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc), keyModeActive(false)
    {
        setWantsKeyboardFocus(true);
        startTimerHz(2);

        initializeTools();
        initializeADSR();
        updateCurrentToolList();
        
        // Start key detection automatically
        enterKeyMode();
    }

    void visibilityChanged() override
    {
        if (isVisible())
            grabKeyboardFocus();
    }

    void parentHierarchyChanged() override
    {
        grabKeyboardFocus();
        updateWaveformSectionPointer();
        
        // Add mouse listener for constant key display
        if (waveformSection) {
            waveformSection->addMouseListener(this, true);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.3f));
        g.drawRoundedRectangle(bounds.reduced(1), 6.0f, 1.5f);

        auto area = bounds.reduced(12, 10);

if (currentViewMode == ViewMode::ADSR)
        {
            paintADSRMode(g, area);
        }
        else if (currentViewMode == ViewMode::Boost)
        {
            paintBoostMode(g, area);
        }
        else if (currentViewMode == ViewMode::Loop)
        {
            paintLoopMode(g, area);
        }
        else if (currentViewMode == ViewMode::Stretch)
        {
            paintStretchMode(g, area);
        }
        else if (currentViewMode == ViewMode::Pitch)
        {
            paintPitchMode(g, area);
        }
        else
        {
            paintCategoryBar(g, area.removeFromTop(20));
            area.removeFromTop(8);

            g.setColour(juce::Colour(0xff00ff00).withAlpha(0.2f));
            g.fillRect(area.removeFromTop(1));
            area.removeFromTop(8);

            paintToolList(g, area.removeFromBottom(area.getHeight() - 30));
            area.removeFromBottom(5);

            paintStatusBar(g, area);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12, 10);

if (currentViewMode == ViewMode::ADSR)
        {
            // ADSR controls layout
            area.removeFromTop(25); // Title
            area.removeFromTop(5);

            auto toggleArea = area.removeFromTop(25);
            adsrEnableToggle.setBounds(toggleArea.removeFromLeft(120).toNearestInt());

            area.removeFromTop(10);

            // Curve area
            area.removeFromTop(80);
            area.removeFromTop(10);

            // Values area - 2x2 grid
            int rowH = 35;
            int colW = area.getWidth() / 2;

            auto row1 = area.removeFromTop(rowH);
            attackValue.setBounds(row1.removeFromLeft(colW).reduced(5).toNearestInt());
            decayValue.setBounds(row1.reduced(5).toNearestInt());

            area.removeFromTop(5);

            auto row2 = area.removeFromTop(rowH);
            sustainValue.setBounds(row2.removeFromLeft(colW).reduced(5).toNearestInt());
            releaseValue.setBounds(row2.reduced(5).toNearestInt());
        }
        // Other modes don't need special resizing for now
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        // ESC to exit special modes
        if (key == juce::KeyPress::escapeKey)
        {
            if (currentViewMode == ViewMode::ADSR) exitADSRMode();
            else if (currentViewMode == ViewMode::Boost) currentViewMode = ViewMode::Tools;
            else if (currentViewMode == ViewMode::Loop) currentViewMode = ViewMode::Tools;
            else if (currentViewMode == ViewMode::Stretch) currentViewMode = ViewMode::Tools;
            else if (currentViewMode == ViewMode::Pitch) currentViewMode = ViewMode::Tools;
            else return false;
            

            repaint();
            return true;
        }

        // ✅ ВСТАВЬТЕ СЮДА:
        if (key == juce::KeyPress::spaceKey && currentCategory == Category::Index)
        {
            updateWaveformSectionPointer();
            bool isSpectralMode = waveformSection &&
                waveformSection->getCurrentChartType() == WaveformDisplaySection::ChartType::Spectral;
            if (!isSpectralMode)
            {
                bool isBrushActive = waveformSection &&
                    waveformSection->getCurrentEditTool() == WaveformDisplaySection::EditTool::Brush;
                if (isBrushActive)
                {
                    showBrushModes = !showBrushModes;
                    updateCurrentToolList();
                    DBG(showBrushModes ? "✅ Showing brush modes" : "✅ Hiding brush modes");
                    return true;
                }
            }
        }

        // Handle special modes
        if (currentViewMode == ViewMode::Boost)
        {
            handleBoostKeyPress(key);
            return true;
        }
        else if (currentViewMode == ViewMode::Loop)
        {
            handleLoopKeyPress(key);
            return true;
        }
        else if (currentViewMode == ViewMode::Stretch)
        {
            handleStretchKeyPress(key);
            return true;
        }
        else if (currentViewMode == ViewMode::Pitch)
        {
            handlePitchKeyPress(key);
            return true;
        }

        if (currentViewMode == ViewMode::ADSR)
        {
            // In ADSR mode, don't process tool shortcuts
            return Component::keyPressed(key);
        }

        // Number keys 1-9 activate tools directly
        if (key.getKeyCode() >= '1' && key.getKeyCode() <= '9')
        {
            int toolNum = key.getKeyCode() - '1';
            if (toolNum < currentTools.size())
            {
                selectedToolIndex = toolNum;
                activateSelectedTool();
                repaint();
            }
            return true;
        }

        if (key == juce::KeyPress::upKey)
        {
            if (selectedToolIndex > 0)
            {
                selectedToolIndex--;
                repaint();
            }
            return true;
        }
        else if (key == juce::KeyPress::downKey)
        {
            if (selectedToolIndex < currentTools.size() - 1)
            {
                selectedToolIndex++;
                repaint();
            }
            return true;
        }
        else if (key == juce::KeyPress::leftKey)
        {
            int newCat = (int)currentCategory - 1;
            if (newCat < 0) newCat = 2;
            switchCategory((Category)newCat);
            return true;
        }
        else if (key == juce::KeyPress::rightKey)
        {
            int newCat = ((int)currentCategory + 1) % 3;
            switchCategory((Category)newCat);
            return true;
        }
        else if (key == juce::KeyPress::returnKey)
        {
            activateSelectedTool();
            return true;
        }

        return Component::keyPressed(key);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        grabKeyboardFocus();
    }

    // Called by WaveformDisplaySection when switching to spectral
    void setSpectralMode(bool isSpectral)
    {
        if (isSpectralMode != isSpectral)
        {
            isSpectralMode = isSpectral;

            // If in Index category, update tools
            if (currentCategory == Category::Index)
            {
                updateCurrentToolList();
                selectedToolIndex = 0; // Reset to first tool (brush)
                repaint();
            }
        }
    }

    bool isInSpectralMode() const { return isSpectralMode; }

    // Set the waveform section pointer (called by MainPanel)
    void setWaveformSection(WaveformDisplaySection* waveform)
    {
        waveformSection = waveform;
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;
    WaveformDisplaySection* waveformSection = nullptr;

    Category currentCategory = Category::General;
    ViewMode currentViewMode = ViewMode::Tools;
    int selectedToolIndex = 0;
    bool cursorVisible = true;
    bool isSpectralMode = false;

    std::vector<Tool> generalTools;
    std::vector<Tool> indexNormalTools;
    std::vector<Tool> indexSpectralTools;
    std::vector<Tool> lfoTools;
    std::vector<Tool> currentTools;

    // Key mode state
    bool keyModeActive;
    juce::String currentKeyNote;
    juce::Point<float> lastMousePos;
    
    // Boost mode state
    float boostValue = 0.0f;  // -20 to +20 dB
    
    // Loop mode state
    bool loopActive = false;
    float loopStart = 0.0f;
    float loopEnd = 1.0f;
    
    // Stretch mode state
    enum class StretchMode
    {
        TimeStretch,
        PitchShift,
        Granular,
        Formant
    };
    StretchMode currentStretchMode = StretchMode::TimeStretch;
    float stretchRatio = 1.0f;
    float pitchShiftSemitones = 0.0f;
    float grainSize = 50.0f;  // ms
    float formantShift = 0.0f;  // semitones
    
    // Pitch mode state
    float pitchValue = 0.0f;  // semitones
    bool finePitchMode = false;

    // ADSR components
    juce::ToggleButton adsrEnableToggle;
    juce::Label attackValue;
    juce::Label decayValue;
    juce::Label sustainValue;
    juce::Label releaseValue;

    float adsrAttack = 0.01f;
    float adsrDecay = 0.1f;
    float adsrSustain = 0.7f;
    float adsrRelease = 0.3f;

    void timerCallback() override
    {
        // ✅ NEW: Auto-update brush modes visibility
        if (currentCategory == Category::Index && showBrushModes) {
            updateWaveformSectionPointer();
            bool isSpectralMode = waveformSection &&
                waveformSection->getCurrentChartType() == WaveformDisplaySection::ChartType::Spectral;
            if (!isSpectralMode) {
                bool isBrushActive = waveformSection &&
                    waveformSection->getCurrentEditTool() == WaveformDisplaySection::EditTool::Brush;
                if (!isBrushActive) {
                    showBrushModes = false;
                    updateCurrentToolList();
                }
            }
            else {
                showBrushModes = false;
                updateCurrentToolList();
            }
        }

        cursorVisible = !cursorVisible;
        repaint();
    }

    void updateWaveformSectionPointer()
    {
        // Try to find and cache the waveform section from sibling components
        if (auto* parent = getParentComponent())
        {
            for (int i = 0; i < parent->getNumChildComponents(); ++i)
            {
                if (auto* wf = dynamic_cast<WaveformDisplaySection*>(parent->getChildComponent(i)))
                {
                    waveformSection = wf;
                    return;
                }
            }
        }
    }

    void initializeADSR()
    {
        // Enable toggle
        adsrEnableToggle.setButtonText("ADSR ENABLED");
        adsrEnableToggle.setToggleState(true, juce::dontSendNotification);
        adsrEnableToggle.onClick = [this]()
            {
                bool enabled = adsrEnableToggle.getToggleState();
                processor.getSamplePlayer().setADSREnabled(enabled);
                adsrEnableToggle.setButtonText(enabled ? "ADSR ENABLED" : "ONE-SHOT MODE");
                repaint();
            };
        addChildComponent(adsrEnableToggle);

        // Value labels
        auto setupValueLabel = [this](juce::Label& label, const juce::String& param)
            {
                label.setEditable(true);
                label.setColour(juce::Label::backgroundColourId, juce::Colours::black);
                label.setColour(juce::Label::textColourId, juce::Colour(0xff00ff00));
                label.setColour(juce::Label::outlineColourId, juce::Colour(0xff00ff00).withAlpha(0.3f));
                label.setFont(juce::Font(11.0f, juce::Font::bold));
                label.setJustificationType(juce::Justification::centred);

                label.onTextChange = [this, &label, param]()
                    {
                        float value = label.getText().getFloatValue();
                        if (param == "A")
                        {
                            adsrAttack = juce::jlimit(0.001f, 2.0f, value);
                            *processor.attackParam = adsrAttack;
                        }
                        else if (param == "D")
                        {
                            adsrDecay = juce::jlimit(0.001f, 2.0f, value);
                            *processor.decayParam = adsrDecay;
                        }
                        else if (param == "S")
                        {
                            adsrSustain = juce::jlimit(0.0f, 1.0f, value);
                            *processor.sustainParam = adsrSustain;
                        }
                        else if (param == "R")
                        {
                            adsrRelease = juce::jlimit(0.001f, 5.0f, value);
                            *processor.releaseParam = adsrRelease;
                        }
                        repaint();
                    };

                addChildComponent(label);
            };

        setupValueLabel(attackValue, "A");
        setupValueLabel(decayValue, "D");
        setupValueLabel(sustainValue, "S");
        setupValueLabel(releaseValue, "R");

        syncADSRFromProcessor();
    }

    void syncADSRFromProcessor()
    {
        adsrAttack = processor.attackParam->get();
        adsrDecay = processor.decayParam->get();
        adsrSustain = processor.sustainParam->get();
        adsrRelease = processor.releaseParam->get();

        attackValue.setText(juce::String(adsrAttack, 3), juce::dontSendNotification);
        decayValue.setText(juce::String(adsrDecay, 3), juce::dontSendNotification);
        sustainValue.setText(juce::String(adsrSustain, 2), juce::dontSendNotification);
        releaseValue.setText(juce::String(adsrRelease, 3), juce::dontSendNotification);

        bool enabled = processor.getSamplePlayer().isADSREnabled();
        adsrEnableToggle.setToggleState(enabled, juce::dontSendNotification);
        adsrEnableToggle.setButtonText(enabled ? "ADSR ENABLED" : "ONE-SHOT MODE");
    }

    void paintADSRMode(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Title
        g.setColour(juce::Colour(0xff00ff00));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        auto titleArea = area.removeFromTop(20);
        g.drawText("ADSR ENVELOPE EDITOR", titleArea, juce::Justification::centredLeft);

        area.removeFromTop(5);

        // Toggle area (component drawn automatically)
        area.removeFromTop(25);
        area.removeFromTop(10);

        // Draw ADSR curve
        auto curveArea = area.removeFromTop(80);
        drawADSRCurve(g, curveArea.toNearestInt());

        area.removeFromTop(10);

        // Parameter labels (components drawn automatically)
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.7f));
        g.setFont(juce::Font(10.0f));

        int rowH = 35;
        int colW = area.getWidth() / 2;

        auto row1 = area.removeFromTop(rowH);
        auto attackLabelArea = row1.removeFromLeft(colW).reduced(5);
        g.drawText("ATTACK (s)", attackLabelArea.removeFromTop(12), juce::Justification::centredLeft);

        auto decayLabelArea = row1.reduced(5);
        g.drawText("DECAY (s)", decayLabelArea.removeFromTop(12), juce::Justification::centredLeft);

        area.removeFromTop(5);

        auto row2 = area.removeFromTop(rowH);
        auto sustainLabelArea = row2.removeFromLeft(colW).reduced(5);
        g.drawText("SUSTAIN (0-1)", sustainLabelArea.removeFromTop(12), juce::Justification::centredLeft);

        auto releaseLabelArea = row2.reduced(5);
        g.drawText("RELEASE (s)", releaseLabelArea.removeFromTop(12), juce::Justification::centredLeft);

        // Exit hint
        area.removeFromTop(10);
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.5f));
        g.setFont(juce::Font(9.0f));
        g.drawText("Press ESC to return to tools", area, juce::Justification::centredLeft);
    }

    void drawADSRCurve(juce::Graphics& g, juce::Rectangle<int> area)
    {
        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRect(area);

        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.1f));
        g.drawRect(area, 1);

        bool enabled = adsrEnableToggle.getToggleState();

        if (!enabled)
        {
            g.setColour(juce::Colour(0xff00ff00).withAlpha(0.5f));
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("ONE-SHOT MODE", area, juce::Justification::centred);
            return;
        }

        // Grid
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.1f));
        for (int i = 1; i < 4; ++i)
        {
            float y = area.getY() + (i / 4.0f) * area.getHeight();
            g.drawLine(area.getX(), y, area.getRight(), y, 1.0f);
        }

        // Calculate proportions
        float totalTime = adsrAttack + adsrDecay + 0.3f + adsrRelease;
        float attackProp = adsrAttack / totalTime;
        float decayProp = adsrDecay / totalTime;
        float holdProp = 0.3f / totalTime;
        float releaseProp = adsrRelease / totalTime;

        // Draw curve
        juce::Path path;

        float startX = area.getX() + 5;
        float startY = area.getBottom() - 5;
        float width = area.getWidth() - 10;
        float height = area.getHeight() - 10;

        float attackEndX = startX + width * attackProp;
        float peakY = area.getY() + 5;

        path.startNewSubPath(startX, startY);
        path.lineTo(attackEndX, peakY);

        float decayEndX = attackEndX + width * decayProp;
        float sustainY = area.getY() + 5 + height * (1.0f - adsrSustain);

        path.lineTo(decayEndX, sustainY);

        float holdEndX = decayEndX + width * holdProp;
        path.lineTo(holdEndX, sustainY);

        float releaseEndX = holdEndX + width * releaseProp;
        path.lineTo(releaseEndX, startY);

        // Stroke
        g.setColour(juce::Colour(0xff00ff00));
        g.strokePath(path, juce::PathStrokeType(2.0f));

        // Fill
        juce::Path fillPath = path;
        fillPath.lineTo(releaseEndX, startY);
        fillPath.lineTo(startX, startY);
        fillPath.closeSubPath();

        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.15f));
        g.fillPath(fillPath);

        // Labels
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.7f));
        g.setFont(juce::Font(9.0f, juce::Font::bold));

        g.drawText("A", startX, area.getBottom() - 15, attackEndX - startX, 12,
            juce::Justification::centred);
        g.drawText("D", attackEndX, area.getBottom() - 15, decayEndX - attackEndX, 12,
            juce::Justification::centred);
        g.drawText("S", decayEndX, area.getBottom() - 15, holdEndX - decayEndX, 12,
            juce::Justification::centred);
        g.drawText("R", holdEndX, area.getBottom() - 15, releaseEndX - holdEndX, 12,
            juce::Justification::centred);
    }

    void enterADSRMode()
    {
        currentViewMode = ViewMode::ADSR;
        syncADSRFromProcessor();

        // Show ADSR components
        adsrEnableToggle.setVisible(true);
        attackValue.setVisible(true);
        decayValue.setVisible(true);
        sustainValue.setVisible(true);
        releaseValue.setVisible(true);

        resized();
        repaint();
    }

    void exitADSRMode()
    {
        currentViewMode = ViewMode::Tools;

        // Hide ADSR components
        adsrEnableToggle.setVisible(false);
        attackValue.setVisible(false);
        decayValue.setVisible(false);
        sustainValue.setVisible(false);
        releaseValue.setVisible(false);

        repaint();
    }
    
    // ===== PAINT METHODS FOR NEW MODES =====
    
    void paintBoostMode(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Title
        g.setColour(juce::Colour(0xff00ff00));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        auto titleArea = area.removeFromTop(20);
        g.drawText("🔊 BOOST CONTROL", titleArea, juce::Justification::centredLeft);
        
        area.removeFromTop(20);
        
        // Boost bar area
        auto barArea = area.reduced(10, 0);
        float barHeight = 40.0f;
        barArea = barArea.withSizeKeepingCentre(barArea.getWidth(), barHeight);
        
        // Background
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(barArea);
        
        // Border
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.5f));
        g.drawRect(barArea, 2.0f);
        
        // Center line (0dB)
        float centerX = barArea.getCentreX();
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.3f));
        g.drawVerticalLine(static_cast<int>(centerX), barArea.getY(), barArea.getBottom());
        
        // Boost indicator
        float normalizedBoost = (boostValue + 20.0f) / 40.0f; // -20 to +20 dB -> 0 to 1
        float indicatorX = barArea.getX() + normalizedBoost * barArea.getWidth();
        
        g.setColour(juce::Colour(0xff00ff00));
        g.drawVerticalLine(static_cast<int>(indicatorX), barArea.getY(), barArea.getBottom());
        
        // Value text
        g.setColour(juce::Colour(0xff00ff00));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        juce::String boostText = juce::String(boostValue, 1) + " dB";
        g.drawText(boostText, barArea.translated(0, -25), juce::Justification::centred);
        
        // Instructions
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.7f));
        g.setFont(juce::Font(10.0f));
        g.drawText("← → to adjust | ENTER to apply | ESC to exit", 
                  area.withTrimmedBottom(10), juce::Justification::centredBottom);
    }
    
    void paintLoopMode(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Title
        g.setColour(juce::Colour(0xff00ff00));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        auto titleArea = area.removeFromTop(20);
        g.drawText("🔄 LOOP CONTROL", titleArea, juce::Justification::centredLeft);
        
        area.removeFromTop(20);
        
        // Waveform area
        auto waveformArea = area.reduced(10, 0);
        waveformArea = waveformArea.withSizeKeepingCentre(waveformArea.getWidth(), 60.0f);
        
        // Background
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(waveformArea);
        
        // Border
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.5f));
        g.drawRect(waveformArea, 2.0f);
        
        // Loop range
        float loopStartX = waveformArea.getX() + loopStart * waveformArea.getWidth();
        float loopEndX = waveformArea.getX() + loopEnd * waveformArea.getWidth();
        juce::Rectangle<float> loopRange(loopStartX, waveformArea.getY(), 
                                        loopEndX - loopStartX, waveformArea.getHeight());
        
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.3f));
        g.fillRect(loopRange);
        
        // Loop indicators
        g.setColour(juce::Colour(0xff00ff00));
        g.drawVerticalLine(static_cast<int>(loopStartX), waveformArea.getY(), waveformArea.getBottom());
        g.drawVerticalLine(static_cast<int>(loopEndX), waveformArea.getY(), waveformArea.getBottom());
        
        // Status
        g.setColour(juce::Colour(0xff00ff00));
        g.setFont(juce::Font(10.0f));
        juce::String statusText = loopActive ? "LOOP ON" : "LOOP OFF";
        g.drawText(statusText, waveformArea.translated(0, -20), juce::Justification::centred);
        
        // Instructions
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.7f));
        g.drawText("L to toggle | ← → to move range | 1-9 to preset | ESC to exit", 
                  area.withTrimmedBottom(10), juce::Justification::centredBottom);
    }
    
    void paintStretchMode(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Title
        g.setColour(juce::Colour(0xff00ff00));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        auto titleArea = area.removeFromTop(20);
        g.drawText("🎛️ STRETCH MODE", titleArea, juce::Justification::centredLeft);
        
        area.removeFromTop(15);
        
        // Mode selector
        auto modeArea = area.removeFromTop(25);
        juce::String modeNames[] = {"TimeStretch", "PitchShift", "Granular", "Formant"};
        juce::String modeAbbr[] = {"TS", "PS", "GR", "FO"};
        
        for (int i = 0; i < 4; ++i)
        {
            auto btnArea = modeArea.removeFromLeft(modeArea.getWidth() / 4).reduced(2);
            
            if (currentStretchMode == static_cast<StretchMode>(i))
            {
                g.setColour(juce::Colour(0xff00ff00));
                g.fillRect(btnArea);
                g.setColour(juce::Colour(0xff000000));
            }
            else
            {
                g.setColour(juce::Colour(0xff333333));
                g.fillRect(btnArea);
                g.setColour(juce::Colour(0xff00ff00));
            }
            
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText(modeAbbr[i], btnArea, juce::Justification::centred);
        }
        
        area.removeFromTop(15);
        
        // Control area
        auto controlArea = area.reduced(10, 0);
        
        // Main parameter display
        g.setColour(juce::Colour(0xff00ff00));
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        
        juce::String mainParam;
        switch (currentStretchMode)
        {
            case StretchMode::TimeStretch:
                mainParam = juce::String(stretchRatio, 2) + "x SPEED";
                break;
            case StretchMode::PitchShift:
                mainParam = juce::String(pitchShiftSemitones, 1) + " SEMITONES";
                break;
            case StretchMode::Granular:
                mainParam = juce::String(grainSize, 0) + "ms GRAIN";
                break;
            case StretchMode::Formant:
                mainParam = juce::String(formantShift, 1) + "st FORMANT";
                break;
        }
        
        g.drawText(mainParam, controlArea.withHeight(30), juce::Justification::centred);
        
        // Instructions
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.7f));
        g.setFont(juce::Font(9.0f));
        g.drawText("1-4 select mode | ← → adjust | ENTER apply | ESC exit", 
                  area.withTrimmedBottom(10), juce::Justification::centredBottom);
    }
    
    void paintPitchMode(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Title
        g.setColour(juce::Colour(0xff00ff00));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        auto titleArea = area.removeFromTop(20);
        g.drawText("🎵 PITCH CONTROL", titleArea, juce::Justification::centredLeft);
        
        area.removeFromTop(20);
        
        // Pitch display
        auto displayArea = area.reduced(10, 0);
        
        // Main pitch value
        g.setColour(juce::Colour(0xff00ff00));
        g.setFont(juce::Font(24.0f, juce::Font::bold));
        juce::String pitchText = (pitchValue >= 0 ? "+" : "") + juce::String(pitchValue, 1) + " st";
        g.drawText(pitchText, displayArea.withHeight(40), juce::Justification::centred);
        
        // Fine pitch indicator
        if (finePitchMode)
        {
            g.setColour(juce::Colour(0xff00ff00).withAlpha(0.7f));
            g.setFont(juce::Font(10.0f));
            g.drawText("FINE MODE", displayArea.translated(0, 25), juce::Justification::centred);
        }
        
        // Visual pitch indicator
        auto indicatorArea = displayArea.removeFromTop(80).translated(0, 50);
        float centerX = indicatorArea.getCentreX();
        float centerY = indicatorArea.getCentreY();
        
        // Background circle
        g.setColour(juce::Colour(0xff333333));
        g.drawEllipse(indicatorArea.withSizeKeepingCentre(120, 60), 2.0f);
        
        // Pitch indicator
        float angle = (pitchValue / 12.0f) * juce::MathConstants<float>::pi; // -12 to +12 semitones
        float indicatorX = centerX + std::sin(angle) * 50;
        float indicatorY = centerY - std::cos(angle) * 25;
        
        g.setColour(juce::Colour(0xff00ff00));
        g.fillEllipse(indicatorX - 8, indicatorY - 8, 16, 16);
        g.drawLine(centerX, centerY, indicatorX, indicatorY, 3.0f);
        
        // Instructions
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.7f));
        g.setFont(juce::Font(9.0f));
        g.drawText("← → coarse | Shift+← → fine | ENTER apply | ESC exit", 
                  area.withTrimmedBottom(10), juce::Justification::centredBottom);
    }

    void initializeTools()
    {
        // GENERAL category - 12 tools including ADSR
        generalTools = {
            {"1. start", false,
             [this]() { showComingSoonMessage("Start"); },
             nullptr, nullptr},

            {"2. length", false,
             [this]() { showComingSoonMessage("Length"); },
             nullptr, nullptr},

            {"3. trim", true,
             [this]() { processor.toggleTrim(); },
             [this]() { return processor.isTrimActive(); },
             nullptr},

            {"4. normalize", true,
             [this]() { processor.toggleNormalize(); },
             [this]() { return processor.isNormalizeActive(); },
             [this]() {
                 if (processor.isNormalizeActive()) {
                     auto& effectState = processor.getEffectStateManager();
                     float targetDb = effectState.getNormalizeTargetDb();
                     return juce::String(targetDb, 0) + "dB";
                 }
                 return juce::String();
             }},

            {"5. boost+clip", true,
             [this]() { processor.toggleBoost(); },
             [this]() { return processor.isBoostActive(); },
             [this]() {
                 if (processor.isBoostActive()) {
                     float boostDb = processor.getEffectStateManager().getBoostDb();
                     return juce::String(boostDb, 1) + "dB";
                 }
                 return juce::String();
             }},

            {"6. reverse", true,
             [this]() { processor.toggleReverse(); },
             [this]() { return processor.isReverseActive(); },
             nullptr},

            {"7. loop", false,
             [this]() { 
                 if (currentViewMode == ViewMode::Loop) {
                     currentViewMode = ViewMode::Tools;
                 } else {
                     enterLoopMode();
                 }
             },
             [this]() { return currentViewMode == ViewMode::Loop; },
             [this]() { 
                 if (currentViewMode == ViewMode::Loop) {
                     return juce::String(loopStart, 2) + "-" + juce::String(loopEnd, 2);
                 }
                 return juce::String();
             }},

            {"8. stretch", false,
             [this]() { 
                 if (currentViewMode == ViewMode::Stretch) {
                     currentViewMode = ViewMode::Tools;
                 } else {
                     enterStretchMode();
                 }
             },
             [this]() { return currentViewMode == ViewMode::Stretch; },
             [this]() { 
                 if (currentViewMode == ViewMode::Stretch) {
                     return juce::String(stretchRatio, 2) + "x";
                 }
                 return juce::String();
             }},

            {"9. pitch", false,
             [this]() { 
                 if (currentViewMode == ViewMode::Pitch) {
                     currentViewMode = ViewMode::Tools;
                 } else {
                     enterPitchMode();
                 }
             },
             [this]() { return currentViewMode == ViewMode::Pitch; },
             [this]() { 
                 if (currentViewMode == ViewMode::Pitch) {
                     return juce::String(pitchValue, 1) + "st";
                 }
                 return juce::String();
             }}
        };

        // Add remaining tools (10-12)
        generalTools.push_back({ "10. key", false,
             [this]() { 
                 if (keyModeActive) {
                     exitKeyMode();
                 } else {
                     enterKeyMode();
                 }
             },
             [this]() { return keyModeActive; },
             [this]() { return currentKeyNote; } });

        generalTools.push_back({ "11. adsr", false,
             [this]() { enterADSRMode(); },
             [this]() { return processor.isAdsrCutItselfMode(); },
             [this]() {
                 if (processor.isAdsrCutItselfMode())
                     return juce::String("CUT");
                 return juce::String();
             } });

        generalTools.push_back({ "12. arp", false,
             [this]() { showComingSoonMessage("Arpeggiator"); },
             nullptr, nullptr });

        // INDEX category - NORMAL MODE (5 tools)
        indexNormalTools = {
            {"1. brush", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setEditTool(WaveformDisplaySection::EditTool::Brush);
                     DBG("✅ Set edit tool: Brush");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentEditTool() == WaveformDisplaySection::EditTool::Brush;
             },
             nullptr},

            {"2. line", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setEditTool(WaveformDisplaySection::EditTool::Line);
                     DBG("✅ Set edit tool: Line");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentEditTool() == WaveformDisplaySection::EditTool::Line;
             },
             nullptr},

            {"3. region", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setEditTool(WaveformDisplaySection::EditTool::RegionSelect);
                     DBG("✅ Set edit tool: Region Select");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentEditTool() == WaveformDisplaySection::EditTool::RegionSelect;
             },
             nullptr},

            {"4. scale", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setEditTool(WaveformDisplaySection::EditTool::VerticalScale);
                     DBG("✅ Set edit tool: Vertical Scale");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentEditTool() == WaveformDisplaySection::EditTool::VerticalScale;
             },
             nullptr},

            {"5. peak", false,
             [this]() { showComingSoonMessage("Peak"); },
             nullptr, nullptr}
        };

        // INDEX category - SPECTRAL MODE (5 tools)
        indexSpectralTools = {
            {"1. brush", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setEditTool(WaveformDisplaySection::EditTool::Brush);
                     DBG("✅ Set spectral edit tool: Brush");
                 }
                 showComingSoonMessage("Spectral Brush (size/intensity controls coming)");
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentEditTool() == WaveformDisplaySection::EditTool::Brush;
             },
             nullptr},

            {"2. line", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setEditTool(WaveformDisplaySection::EditTool::Line);
                     DBG("✅ Set spectral edit tool: Line");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentEditTool() == WaveformDisplaySection::EditTool::Line;
             },
             nullptr},

            {"3. analyze", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection)
                     waveformSection->analyzeSpectralIndices();
             },
             nullptr, nullptr},

            {"4. apply", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection)
                     waveformSection->applySpectralModifications();
             },
             nullptr, nullptr},

            {"5. clear", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection)
                     waveformSection->clearSpectralEdits();
             },
             nullptr, nullptr}
        };

        // LFO category
        lfoTools = {
            {"1. lfo settings", false,
             [this]() { showComingSoonMessage("LFO"); },
             nullptr, nullptr}
        };

        // ✅ NEW: Initialize brush mode tools
        brushModeTools = {
            {"   relief", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setBrushMode(WaveformDisplaySection::BrushMode::Relief);
                     DBG("✅ Set brush mode: Relief");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentBrushMode() == WaveformDisplaySection::BrushMode::Relief;
             },
             nullptr},

            {"   straight", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setBrushMode(WaveformDisplaySection::BrushMode::Straight);
                     DBG("✅ Set brush mode: Straight");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentBrushMode() == WaveformDisplaySection::BrushMode::Straight;
             },
             nullptr},

            {"   triangle", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setBrushMode(WaveformDisplaySection::BrushMode::Triangle);
                     DBG("✅ Set brush mode: Triangle");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentBrushMode() == WaveformDisplaySection::BrushMode::Triangle;
             },
             nullptr},

            {"   square", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setBrushMode(WaveformDisplaySection::BrushMode::Square);
                     DBG("✅ Set brush mode: Square");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentBrushMode() == WaveformDisplaySection::BrushMode::Square;
             },
             nullptr},

            {"   noise", false,
             [this]() {
                 updateWaveformSectionPointer();
                 if (waveformSection) {
                     waveformSection->setBrushMode(WaveformDisplaySection::BrushMode::Noise);
                     DBG("✅ Set brush mode: Noise");
                 }
             },
             [this]() -> bool {
                 updateWaveformSectionPointer();
                 if (!waveformSection) return false;
                 return waveformSection->getCurrentBrushMode() == WaveformDisplaySection::BrushMode::Noise;
             },
             nullptr}
        };
    }

    void updateCurrentToolList()
    {
        updateWaveformSectionPointer();

        bool isSpectralMode = waveformSection &&
            waveformSection->getCurrentChartType() == WaveformDisplaySection::ChartType::Spectral;

        switch (currentCategory)
        {
        case Category::General:
            currentTools = generalTools;
            break;

        case Category::Index:
            // ✅ NEW: Check if brush is active
            if (!isSpectralMode)
            {
                bool isBrushActive = waveformSection &&
                    waveformSection->getCurrentEditTool() == WaveformDisplaySection::EditTool::Brush;

                if (isBrushActive && showBrushModes)
                {
                    // Show brush + brush modes
                    currentTools = indexNormalTools;

                    // Insert brush modes after first tool (brush)
                    currentTools.insert(
                        currentTools.begin() + 1,
                        brushModeTools.begin(),
                        brushModeTools.end()
                    );
                }
                else
                {
                    // Show normal tools
                    currentTools = indexNormalTools;
                    showBrushModes = false;
                }
            }
            else
            {
                currentTools = indexSpectralTools;
                showBrushModes = false;
            }
            break;

        case Category::Lfo:
            currentTools = lfoTools;
            break;
        }

        selectedToolIndex = juce::jlimit(0, juce::jmax(0, (int)currentTools.size() - 1), selectedToolIndex);
        repaint();
    }

    void switchCategory(Category newCategory)
    {
        if (currentCategory != newCategory)
        {
            currentCategory = newCategory;
            selectedToolIndex = 0;
            updateCurrentToolList();
        }
    }

    void activateSelectedTool()
    {
        if (selectedToolIndex < currentTools.size())
        {
            auto& tool = currentTools[selectedToolIndex];
            if (tool.action)
                tool.action();
        }
    }

    void paintCategoryBar(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const std::vector<juce::String> categories = { "GENERAL", "INDEX", "LFO" };
        float tabWidth = area.getWidth() / 3.0f;

        for (int i = 0; i < 3; ++i)
        {
            auto tabArea = area.removeFromLeft(tabWidth);
            bool isSelected = (i == (int)currentCategory);

            if (isSelected)
            {
                g.setColour(juce::Colour(0xff00ff00).withAlpha(0.2f));
                g.fillRect(tabArea);
            }

            g.setColour(isSelected ? juce::Colour(0xff00ff00) : juce::Colour(0xff00ff00).withAlpha(0.5f));
            g.setFont(juce::Font(10.0f, isSelected ? juce::Font::bold : juce::Font::plain));
            g.drawText(categories[i], tabArea, juce::Justification::centred);
        }
    }

    void paintToolList(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float lineHeight = 18.0f;

        for (int i = 0; i < currentTools.size(); ++i)
        {
            auto& tool = currentTools[i];
            auto toolArea = area.removeFromTop(lineHeight);

            bool isSelected = (i == selectedToolIndex);
            bool isActive = tool.isActive ? tool.isActive() : false;

            // ✅ NEW: Check if this is a brush mode tool (has leading spaces)
            bool isBrushModeTool = tool.name.startsWith("   ");

            // Background for selected
            if (isSelected && !isBrushModeTool)
            {
                g.setColour(juce::Colour(0xff00ff00).withAlpha(0.15f));
                g.fillRect(toolArea);
            }

            // Cursor
            if (isSelected && cursorVisible && !isBrushModeTool)
            {
                g.setColour(juce::Colour(0xff00ff00));
                g.fillRect(toolArea.getX(), toolArea.getY(), 3.0f, toolArea.getHeight());
            }

            // Tool name color
            juce::Colour textColour;

            if (isBrushModeTool)
            {
                // ✅ NEW: Brush modes - green if active, gray if not
                textColour = isActive
                    ? juce::Colour(0xff00ff00)  // Green for active mode
                    : juce::Colour(0xff666666);  // Gray for inactive mode
            }
            else
            {
                // Normal tools
                textColour = isActive
                    ? juce::Colour(0xff00ff00)
                    : (isSelected ? juce::Colour(0xff00ff00).withAlpha(0.9f)
                        : juce::Colour(0xff00ff00).withAlpha(0.6f));
            }

            g.setColour(textColour);

            // ✅ NEW: Brush modes use normal font
            juce::Font font = isBrushModeTool
                ? juce::Font(11.0f, juce::Font::plain)
                : juce::Font(11.0f, isActive ? juce::Font::bold : juce::Font::plain);

            g.setFont(font);

            juce::String displayName = tool.name;
            if (isActive && !isBrushModeTool)
                displayName += " [ACTIVE]";

            // Add status text if available
            if (tool.getStatusText)
            {
                auto statusText = tool.getStatusText();
                if (statusText.isNotEmpty())
                    displayName += " [" + statusText + "]";
            }

            g.drawText(displayName, toolArea.translated(10, 0), juce::Justification::centredLeft);
        }
    }

    void paintStatusBar(juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.5f));
        g.setFont(juce::Font(9.0f));

        juce::String status = "↑↓ navigate | ← → switch category | ENTER activate | 1-9 direct select";
        
        // Always show key note display, even when key mode is not active
        if (!currentKeyNote.isEmpty())
        {
            status += " | 🎵 NOTE: " + currentKeyNote;
        }
        
        g.drawText(status, area, juce::Justification::centredLeft);
    }

    void showComingSoonMessage(const juce::String& feature)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            feature,
            feature + " coming soon...",
            "OK");
    }

    // ===== KEY DETECTION FUNCTIONS =====
    
    juce::String frequencyToNote(float frequency)
    {
        if (frequency <= 0.0f) return "--";
        
        // A4 = 440Hz is reference
        const float A4 = 440.0f;
        const juce::String noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        
        // Calculate MIDI note number: C4 = MIDI 60, A4 = MIDI 69
        float midiNote = 12.0f * std::log2(frequency / A4) + 69.0f;
        int roundedMidiNote = static_cast<int>(std::round(midiNote));
        
        // Clamp to valid MIDI range
        roundedMidiNote = juce::jlimit(0, 127, roundedMidiNote);
        
        // Calculate octave and note index (C4 = octave 4, MIDI note 60)
        int octave = (roundedMidiNote / 12) - 1;
        int noteIndex = roundedMidiNote % 12;
        
        juce::String noteString = noteNames[noteIndex] + juce::String(octave);
        
        // No cents display - only note name
        
        return noteString;
    }
    
    float getFrequencyAtPosition(juce::Point<float> pos)
    {
        if (!waveformSection) return 0.0f;
        
        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();
        
        if (numSamples == 0) return 0.0f;
        
        // Get waveform area (similar to modifyFeatureAtPosition)
        auto chartArea = waveformSection->getWaveArea(); // Need to add this method
        chartArea = chartArea.reduced(10);
        chartArea.removeFromTop(20);
        
        if (!chartArea.toFloat().contains(pos)) return 0.0f;
        
        float normalizedX = (pos.x - chartArea.getX()) / chartArea.getWidth();
        normalizedX = juce::jlimit(0.0f, 1.0f, normalizedX);
        
        int startSample = static_cast<int>(waveformSection->getPanOffsetX() * numSamples);
        int visibleSamples = static_cast<int>(numSamples / waveformSection->getHorizontalZoom());
        int sampleIndex = startSample + static_cast<int>(normalizedX * visibleSamples);
        sampleIndex = juce::jlimit(0, numSamples - 1, sampleIndex);
        
        // Additional bounds check to prevent access violation
        if (sampleIndex < 0 || sampleIndex >= numSamples) return 0.0f;
        
        // Check if frequency is valid
        float frequency = features[sampleIndex].frequency;
        if (frequency <= 0.0f || frequency > 20000.0f) return 0.0f;
        
        return frequency;
    }
    
    void updateKeyAtPosition(juce::Point<float> pos)
    {
        float frequency = getFrequencyAtPosition(pos);
        currentKeyNote = frequencyToNote(frequency);
        lastMousePos = pos;
    }
    
    void enterKeyMode()
    {
        keyModeActive = true;
        currentKeyNote = "--";
        DBG("🎵 Key detection mode activated - hover over waveform to see notes");
        
        // Mouse listener is always active now for constant key display
        updateWaveformSectionPointer();
        if (waveformSection) {
            waveformSection->addMouseListener(this, true);
        }
        
        repaint();
    }
    
    void exitKeyMode()
    {
        keyModeActive = false;
        currentKeyNote = "";
        DBG("🎵 Key detection mode deactivated");
        
        // Don't remove mouse listener - we want constant key display even when mode is inactive
        // if (waveformSection) {
        //     waveformSection->removeMouseListener(this);
        // }
        
        repaint();
    }
    
    // ===== BOOST MODE =====
    void enterBoostMode()
    {
        currentViewMode = ViewMode::Boost;
        boostValue = processor.getBoostDb();
        DBG("🔊 Boost mode activated - use ← → to adjust");
        grabKeyboardFocus();
        repaint();
    }
    
    // ===== LOOP MODE =====
    void enterLoopMode()
    {
        currentViewMode = ViewMode::Loop;
        loopActive = processor.isLoopActive();
        loopStart = 0.0f;
        loopEnd = 1.0f;
        DBG("🔄 Loop mode activated");
        grabKeyboardFocus();
        repaint();
    }
    
    // ===== STRETCH MODE =====
    void enterStretchMode()
    {
        currentViewMode = ViewMode::Stretch;
        stretchRatio = processor.getTimeStretch();
        currentStretchMode = StretchMode::TimeStretch;
        DBG("🎛️ Stretch mode activated");
        grabKeyboardFocus();
        repaint();
    }
    
    // ===== PITCH MODE =====
    void enterPitchMode()
    {
        currentViewMode = ViewMode::Pitch;
        pitchValue = processor.getPitchShift();
        finePitchMode = false;
        DBG("🎵 Pitch mode activated");
        grabKeyboardFocus();
        repaint();
    }
    
    void handleBoostKeyPress(const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::leftKey)
        {
            boostValue = juce::jlimit(-20.0f, 20.0f, boostValue - 1.0f);
            processor.setBoostDb(boostValue);
            DBG("🔊 CMD: Boost set to " + juce::String(boostValue, 1) + "dB | Param value: " + juce::String(processor.getBoostDb(), 1) + "dB");
            repaint();
        }
        else if (key == juce::KeyPress::rightKey)
        {
            boostValue = juce::jlimit(-20.0f, 20.0f, boostValue + 1.0f);
            processor.setBoostDb(boostValue);
            DBG("🔊 CMD: Boost set to " + juce::String(boostValue, 1) + "dB | Param value: " + juce::String(processor.getBoostDb(), 1) + "dB");
            repaint();
        }
        else if (key == juce::KeyPress::returnKey)
        {
            DBG("✅ Boost applied: " + juce::String(boostValue, 1) + " dB");
        }
    }
    
    void handleLoopKeyPress(const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::leftKey)
        {
            float range = loopEnd - loopStart;
            loopStart = juce::jlimit(0.0f, 0.95f, loopStart - 0.01f);
            loopEnd = juce::jlimit(loopStart + 0.05f, 1.0f, loopStart + range);
            // TODO: Apply to SamplePlayer when setLoopRange is implemented
            // processor.getSamplePlayer().setLoopRange(loopStart, loopEnd);
            DBG("🔄 Loop: " + juce::String(loopStart, 2) + "-" + juce::String(loopEnd, 2));
            repaint();
        }
        else if (key == juce::KeyPress::rightKey)
        {
            float range = loopEnd - loopStart;
            loopEnd = juce::jlimit(0.05f, 1.0f, loopEnd + 0.01f);
            loopStart = juce::jlimit(0.0f, loopEnd - 0.05f, loopEnd - range);
            // TODO: Apply to SamplePlayer when setLoopRange is implemented
            // processor.getSamplePlayer().setLoopRange(loopStart, loopEnd);
            DBG("🔄 Loop: " + juce::String(loopStart, 2) + "-" + juce::String(loopEnd, 2));
            repaint();
        }
        else if (key.getTextCharacter() == 'l' || key.getTextCharacter() == 'L')
        {
            loopActive = !loopActive;
            // TODO: Apply to SamplePlayer when setLoopActive is implemented
            // processor.getSamplePlayer().setLoopActive(loopActive);
            DBG("🔄 Loop " + juce::String(loopActive ? "ON" : "OFF"));
            repaint();
        }
        else if (key.getKeyCode() >= '1' && key.getKeyCode() <= '9')
        {
            // Preset loop ranges
            float presets[] = {0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f};
            int preset = key.getKeyCode() - '1';
            float presetSize = 0.125f; // 1/8 of sample
            loopStart = presets[preset];
            loopEnd = juce::jlimit(0.0f, 1.0f, loopStart + presetSize);
            // TODO: Apply to SamplePlayer when setLoopRange is implemented
            // processor.getSamplePlayer().setLoopRange(loopStart, loopEnd);
            DBG("🔄 Loop preset: " + juce::String(loopStart, 2) + "-" + juce::String(loopEnd, 2));
            repaint();
        }
    }
    
    void handleStretchKeyPress(const juce::KeyPress& key)
    {
        if (key.getKeyCode() >= '1' && key.getKeyCode() <= '4')
        {
            currentStretchMode = static_cast<StretchMode>(key.getKeyCode() - '1');
            DBG("🎛️ Stretch mode: " + juce::String(static_cast<int>(currentStretchMode)));
            repaint();
        }
        else if (key == juce::KeyPress::leftKey)
        {
            switch (currentStretchMode)
            {
                case StretchMode::TimeStretch:
                    stretchRatio = juce::jlimit(0.25f, 4.0f, stretchRatio - 0.05f);
                    break;
                case StretchMode::PitchShift:
                    pitchShiftSemitones = juce::jlimit(-12.0f, 12.0f, pitchShiftSemitones - 0.5f);
                    break;
                case StretchMode::Granular:
                    grainSize = juce::jlimit(5.0f, 500.0f, grainSize - 5.0f);
                    break;
                case StretchMode::Formant:
                    formantShift = juce::jlimit(-12.0f, 12.0f, formantShift - 0.5f);
                    break;
            }
            repaint();
        }
        else if (key == juce::KeyPress::rightKey)
        {
            switch (currentStretchMode)
            {
                case StretchMode::TimeStretch:
                    stretchRatio = juce::jlimit(0.25f, 4.0f, stretchRatio + 0.05f);
                    break;
                case StretchMode::PitchShift:
                    pitchShiftSemitones = juce::jlimit(-12.0f, 12.0f, pitchShiftSemitones + 0.5f);
                    break;
                case StretchMode::Granular:
                    grainSize = juce::jlimit(5.0f, 500.0f, grainSize + 5.0f);
                    break;
                case StretchMode::Formant:
                    formantShift = juce::jlimit(-12.0f, 12.0f, formantShift + 0.5f);
                    break;
            }
            repaint();
        }
        else if (key == juce::KeyPress::returnKey)
        {
            applyStretchSettings();
        }
    }
    
    void handlePitchKeyPress(const juce::KeyPress& key)
    {
        bool shiftHeld = key.getModifiers().isShiftDown();
        float step = shiftHeld ? 0.1f : 1.0f;
        
        if (key == juce::KeyPress::leftKey)
        {
            pitchValue = juce::jlimit(-12.0f, 12.0f, pitchValue - step);
            finePitchMode = shiftHeld;
            repaint();
        }
        else if (key == juce::KeyPress::rightKey)
        {
            pitchValue = juce::jlimit(-12.0f, 12.0f, pitchValue + step);
            finePitchMode = shiftHeld;
            repaint();
        }
        else if (key == juce::KeyPress::returnKey)
        {
            applyPitchShift();
        }
    }
    
    void applyStretchSettings()
    {
        DBG("🎛️ Applying stretch settings...");
        // TODO: Implement actual stretch processing
        // This would require phase vocoder or granular processing
    }
    
    void applyPitchShift()
    {
        processor.setPitchShift(pitchValue);
        DBG("🎵 Applying pitch shift: " + juce::String(pitchValue, 1) + " semitones");
    }
    
    void mouseMove(const juce::MouseEvent& event) override
    {
        if (waveformSection)
        {
            try {
                juce::Point<float> relativePos = event.getEventRelativeTo(waveformSection).position;
                updateKeyAtPosition(relativePos);
                repaint();
            }
            catch (...)
            {
                // Prevent crashes - set empty note on error
                currentKeyNote = "";
                DBG("❌ Error in key detection");
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CMDTerminalToolsSection)
};