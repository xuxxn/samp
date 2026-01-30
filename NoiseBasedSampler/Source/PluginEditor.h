/*
PluginEditor.h - COMPATIBLE WITH ORIGINAL STRUCTURE
✅ Uses TabbedComponent (like original)
✅ Only MainPanel tab for now
✅ Size: 700x500 (vertical layout)
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PatternPanel.h"
#include "MainPanel.h"
#include "UI/AboutPanel.h"
#include "Core/VersionInfo.h"
#include "ProjectPanel.h"

class NoiseBasedSamplerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    NoiseBasedSamplerAudioProcessorEditor(NoiseBasedSamplerAudioProcessor& p)
        : AudioProcessorEditor(&p), audioProcessor(p)
    {
        setSize(700, 600);  // Vertical layout with more space for waveform

        addAndMakeVisible(tabbedComponent);

        // Main tab
        tabbedComponent.addTab("Main", juce::Colours::darkgrey,
            new MainPanel(audioProcessor), true);

        // Algo tab (placeholder)
        tabbedComponent.addTab("algo", juce::Colours::darkgrey,
            createPlaceholderPanel("Algorithm Panel"), true);

        // Pattern tab with actual PatternPanel
        patternPanel.reset(new PatternPanel(audioProcessor));
        tabbedComponent.addTab("pattern", juce::Colours::darkgrey,
            patternPanel.get(), true);

        auto* projectPanel = new ProjectPanel(audioProcessor);
        tabbedComponent.addTab("project", juce::Colours::darkgrey,
            projectPanel, true);

        tabbedComponent.setTabBarDepth(35);
        tabbedComponent.setOutline(0);
        tabbedComponent.setCurrentTabIndex(0);

        tabbedComponent.addTab("about", juce::Colours::darkgrey,
            new AboutPanel(), true);

        DBG("✅ Plugin Editor initialized - Vertical Layout (700x600)");
    }

    ~NoiseBasedSamplerAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2d2d2d));
    }

    void resized() override
    {
        tabbedComponent.setBounds(getLocalBounds());
    }

private:
    NoiseBasedSamplerAudioProcessor& audioProcessor;
    juce::TabbedComponent tabbedComponent{ juce::TabbedButtonBar::TabsAtTop };
    std::unique_ptr<PatternPanel> patternPanel;

    juce::Component* createPlaceholderPanel(const juce::String& name)
    {
        class PlaceholderPanel : public juce::Component
        {
        public:
            PlaceholderPanel(const juce::String& panelName) : name(panelName) {}

            void paint(juce::Graphics& g) override
            {
                g.fillAll(juce::Colour(0xff2d2d2d));

                g.setColour(juce::Colours::white.withAlpha(0.3f));
                g.setFont(juce::Font(18.0f, juce::Font::bold));
                g.drawText(name, getLocalBounds(), juce::Justification::centred);

                g.setFont(juce::Font(14.0f));
                g.drawText("Coming soon...", getLocalBounds().translated(0, 30),
                    juce::Justification::centred);
            }

        private:
            juce::String name;
        };

        return new PlaceholderPanel(name);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseBasedSamplerAudioProcessorEditor)
};  