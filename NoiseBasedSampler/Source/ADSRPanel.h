/*
ADSRPanel.h
FL Studio style ADSR envelope editor with visual curve and toggle.
Features:
- Visual ADSR curve display (like FL Studio)
- Enable/Disable toggle button
- 4 rotary knobs: Attack, Decay, Sustain, Release
- Real-time curve preview
- One-shot mode when ADSR disabled (sample plays to end)
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class ADSRPanel : public juce::Component,
                  public juce::Timer
{
public:
    ADSRPanel(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        setSize(800, 600);
        startTimerHz(30);

        // ========== ADSR ENABLE TOGGLE (FL Studio style) ==========
        addAndMakeVisible(adsrEnableButton);
        adsrEnableButton.setButtonText("ADSR");
        adsrEnableButton.setToggleState(true, juce::dontSendNotification);
        adsrEnableButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        adsrEnableButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff10b981));
        adsrEnableButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
        adsrEnableButton.onClick = [this] { 
            bool enabled = adsrEnableButton.getToggleState();
            processor.getSamplePlayer().setADSREnabled(enabled);
            updateEnvelopeParameters();
            repaint();
            
            DBG("ADSR " + juce::String(enabled ? "ENABLED" : "DISABLED") + 
                " - " + (enabled ? "envelope mode" : "one-shot mode"));
        };

        // ========== INFO LABELS ==========
        addAndMakeVisible(titleLabel);
        titleLabel.setText("ADSR Envelope", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(infoLabel);
        infoLabel.setText("Enable ADSR for envelope control, or disable for one-shot playback", 
                         juce::dontSendNotification);
        infoLabel.setFont(juce::Font(12.0f));
        infoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        infoLabel.setJustificationType(juce::Justification::centredLeft);

        // ========== ATTACK KNOB ==========
        addAndMakeVisible(attackSlider);
        attackSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        attackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        attackSlider.setRange(0.001, 2.0, 0.001);
        attackSlider.setValue(0.01);
        attackSlider.onValueChange = [this] { updateEnvelopeParameters(); };

        addAndMakeVisible(attackLabel);
        attackLabel.setText("Attack", juce::dontSendNotification);
        attackLabel.setFont(juce::Font(13.0f, juce::Font::bold));
        attackLabel.setColour(juce::Label::textColourId, juce::Colour(0xff60a5fa));
        attackLabel.setJustificationType(juce::Justification::centred);

        // ========== DECAY KNOB ==========
        addAndMakeVisible(decaySlider);
        decaySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        decaySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        decaySlider.setRange(0.001, 2.0, 0.001);
        decaySlider.setValue(0.1);
        decaySlider.onValueChange = [this] { updateEnvelopeParameters(); };

        addAndMakeVisible(decayLabel);
        decayLabel.setText("Decay", juce::dontSendNotification);
        decayLabel.setFont(juce::Font(13.0f, juce::Font::bold));
        decayLabel.setColour(juce::Label::textColourId, juce::Colour(0xff60a5fa));
        decayLabel.setJustificationType(juce::Justification::centred);

        // ========== SUSTAIN KNOB ==========
        addAndMakeVisible(sustainSlider);
        sustainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sustainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        sustainSlider.setRange(0.0, 1.0, 0.01);
        sustainSlider.setValue(0.7);
        sustainSlider.onValueChange = [this] { updateEnvelopeParameters(); };

        addAndMakeVisible(sustainLabel);
        sustainLabel.setText("Sustain", juce::dontSendNotification);
        sustainLabel.setFont(juce::Font(13.0f, juce::Font::bold));
        sustainLabel.setColour(juce::Label::textColourId, juce::Colour(0xff60a5fa));
        sustainLabel.setJustificationType(juce::Justification::centred);

        // ========== RELEASE KNOB ==========
        addAndMakeVisible(releaseSlider);
        releaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
        releaseSlider.setRange(0.001, 5.0, 0.001);
        releaseSlider.setValue(0.3);
        releaseSlider.onValueChange = [this] { updateEnvelopeParameters(); };

        addAndMakeVisible(releaseLabel);
        releaseLabel.setText("Release", juce::dontSendNotification);
        releaseLabel.setFont(juce::Font(13.0f, juce::Font::bold));
        releaseLabel.setColour(juce::Label::textColourId, juce::Colour(0xff60a5fa));
        releaseLabel.setJustificationType(juce::Justification::centred);

        // Sync from processor
        syncFromProcessor();
    }

    ~ADSRPanel() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        // Background
        g.fillAll(juce::Colour(0xff1e1e1e));

        // Visual ADSR curve area
        auto bounds = getLocalBounds().reduced(10);

        // Make layout adaptive to available height so it looks good even
        // when embedded into a shorter panel on the Main tab.
        int h = bounds.getHeight();

        int topTrim = juce::jlimit(40, 100, h / 4);
        int bottomTrim = juce::jlimit(40, 160, h / 3);

        auto curveArea = bounds
            .withTrimmedTop(topTrim)
            .withTrimmedBottom(bottomTrim);

        drawADSRCurve(g, curveArea);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);

        int totalH = area.getHeight();

        // Responsive top section
        int topH = juce::jlimit(50, 90, totalH / 4);
        auto topArea = area.removeFromTop(topH);
        titleLabel.setBounds(topArea.removeFromTop(24).removeFromLeft(220));
        infoLabel.setBounds(topArea.removeFromTop(18).removeFromLeft(400));

        topArea.removeFromTop(4);
        adsrEnableButton.setBounds(topArea.removeFromLeft(100).withHeight(24));

        // Middle: curve visualization (actual area computed in paint)
        int curveH = juce::jlimit(80, 200, totalH / 2);
        auto curveArea = area.removeFromTop(curveH);

        area.removeFromTop(6);

        // Bottom - Rotary knobs in a row (shrink if not enough space)
        int knobsH = juce::jlimit(80, 130, area.getHeight());
        auto knobArea = area.removeFromTop(knobsH);
        int knobSize = 120;
        int spacing = 30;

        auto attackArea = knobArea.removeFromLeft(knobSize);
        attackLabel.setBounds(attackArea.removeFromTop(20));
        attackSlider.setBounds(attackArea);

        knobArea.removeFromLeft(spacing);

        auto decayArea = knobArea.removeFromLeft(knobSize);
        decayLabel.setBounds(decayArea.removeFromTop(20));
        decaySlider.setBounds(decayArea);

        knobArea.removeFromLeft(spacing);

        auto sustainArea = knobArea.removeFromLeft(knobSize);
        sustainLabel.setBounds(sustainArea.removeFromTop(20));
        sustainSlider.setBounds(sustainArea);

        knobArea.removeFromLeft(spacing);

        auto releaseArea = knobArea.removeFromLeft(knobSize);
        releaseLabel.setBounds(releaseArea.removeFromTop(20));
        releaseSlider.setBounds(releaseArea);
    }

    void timerCallback() override
    {
        repaint();
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;

    juce::ToggleButton adsrEnableButton;
    juce::Label titleLabel;
    juce::Label infoLabel;

    juce::Slider attackSlider;
    juce::Label attackLabel;
    
    juce::Slider decaySlider;
    juce::Label decayLabel;
    
    juce::Slider sustainSlider;
    juce::Label sustainLabel;
    
    juce::Slider releaseSlider;
    juce::Label releaseLabel;

    void drawADSRCurve(juce::Graphics& g, juce::Rectangle<int> area)
    {
        // Background panel
        g.setColour(juce::Colour(0xff2d2d2d));
        g.fillRoundedRectangle(area.toFloat(), 8.0f);

        bool adsrEnabled = adsrEnableButton.getToggleState();

        if (!adsrEnabled)
        {
            // Show "ONE-SHOT MODE" message
            g.setColour(juce::Colours::grey);
            g.setFont(juce::Font(16.0f, juce::Font::bold));
            g.drawText("ONE-SHOT MODE", area, juce::Justification::centred);
            
            g.setFont(juce::Font(12.0f));
            g.drawText("Sample plays to end without envelope", 
                      area.withTrimmedTop(30), 
                      juce::Justification::centred);
            return;
        }

        // Draw grid
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        for (int i = 0; i <= 4; ++i)
        {
            float y = area.getY() + (i / 4.0f) * area.getHeight();
            g.drawLine(area.getX(), y, area.getRight(), y, 1.0f);
        }

        // Get ADSR values
        float attack = static_cast<float>(attackSlider.getValue());
        float decay = static_cast<float>(decaySlider.getValue());
        float sustain = static_cast<float>(sustainSlider.getValue());
        float release = static_cast<float>(releaseSlider.getValue());

        // Calculate time proportions (FL Studio style)
        float totalTime = attack + decay + 0.5f + release; // 0.5s hold for sustain
        
        float attackProportion = attack / totalTime;
        float decayProportion = decay / totalTime;
        float holdProportion = 0.5f / totalTime;
        float releaseProportion = release / totalTime;

        // Draw ADSR curve
        juce::Path envelopePath;
        
        float startX = area.getX();
        float startY = area.getBottom();
        float width = area.getWidth();
        float height = area.getHeight();

        // Attack phase
        float attackEndX = startX + width * attackProportion;
        float peakY = area.getY();
        
        envelopePath.startNewSubPath(startX, startY);
        envelopePath.lineTo(attackEndX, peakY);

        // Decay phase
        float decayEndX = attackEndX + width * decayProportion;
        float sustainY = area.getY() + height * (1.0f - sustain);
        
        envelopePath.lineTo(decayEndX, sustainY);

        // Sustain phase (hold)
        float holdEndX = decayEndX + width * holdProportion;
        envelopePath.lineTo(holdEndX, sustainY);

        // Release phase
        float releaseEndX = holdEndX + width * releaseProportion;
        envelopePath.lineTo(releaseEndX, startY);

        // Draw the curve
        g.setColour(juce::Colour(0xff10b981));
        g.strokePath(envelopePath, juce::PathStrokeType(3.0f));

        // Fill under curve
        juce::Path fillPath = envelopePath;
        fillPath.lineTo(releaseEndX, startY);
        fillPath.lineTo(startX, startY);
        fillPath.closeSubPath();

        g.setColour(juce::Colour(0xff10b981).withAlpha(0.2f));
        g.fillPath(fillPath);

        // Draw stage labels
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::Font(11.0f, juce::Font::bold));

        // Attack label
        g.drawText("A", 
                  startX + (attackEndX - startX) / 2 - 10, 
                  area.getBottom() + 10, 
                  20, 20, 
                  juce::Justification::centred);

        // Decay label
        g.drawText("D", 
                  attackEndX + (decayEndX - attackEndX) / 2 - 10, 
                  area.getBottom() + 10, 
                  20, 20, 
                  juce::Justification::centred);

        // Sustain label
        g.drawText("S", 
                  decayEndX + (holdEndX - decayEndX) / 2 - 10, 
                  area.getBottom() + 10, 
                  20, 20, 
                  juce::Justification::centred);

        // Release label
        g.drawText("R", 
                  holdEndX + (releaseEndX - holdEndX) / 2 - 10, 
                  area.getBottom() + 10, 
                  20, 20, 
                  juce::Justification::centred);

        // Draw time markers
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font(10.0f));
        
        g.drawText(juce::String(attack, 3) + "s", 
                  startX, area.getY() - 20, 
                  attackEndX - startX, 15, 
                  juce::Justification::centred);
        
        g.drawText(juce::String(decay, 3) + "s", 
                  attackEndX, area.getY() - 20, 
                  decayEndX - attackEndX, 15, 
                  juce::Justification::centred);
        
        g.drawText(juce::String(sustain, 2), 
                  decayEndX, area.getY() - 20, 
                  holdEndX - decayEndX, 15, 
                  juce::Justification::centred);
        
        g.drawText(juce::String(release, 3) + "s", 
                  holdEndX, area.getY() - 20, 
                  releaseEndX - holdEndX, 15, 
                  juce::Justification::centred);
    }

    void updateEnvelopeParameters()
    {
        *processor.attackParam = static_cast<float>(attackSlider.getValue());
        *processor.decayParam = static_cast<float>(decaySlider.getValue());
        *processor.sustainParam = static_cast<float>(sustainSlider.getValue());
        *processor.releaseParam = static_cast<float>(releaseSlider.getValue());

        repaint();
    }

    void syncFromProcessor()
    {
        attackSlider.setValue(processor.attackParam->get(), juce::dontSendNotification);
        decaySlider.setValue(processor.decayParam->get(), juce::dontSendNotification);
        sustainSlider.setValue(processor.sustainParam->get(), juce::dontSendNotification);
        releaseSlider.setValue(processor.releaseParam->get(), juce::dontSendNotification);
        
        // Sync ADSR enabled state from processor
        bool enabled = processor.getSamplePlayer().isADSREnabled();
        adsrEnableButton.setToggleState(enabled, juce::dontSendNotification);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ADSRPanel)
};