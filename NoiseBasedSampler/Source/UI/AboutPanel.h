/*
  AboutPanel.h - About & Update Panel

  Shows:
  - Plugin version
  - Build information
  - Check for updates button
  - Updater status
*/

#pragma once
#include <JuceHeader.h>
#include "../Core/VersionInfo.h"
#include "../Update/UpdaterLauncher.h"

class AboutPanel : public juce::Component
{
public:
    AboutPanel()
    {
        // Title label
        titleLabel.setText("About samp", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centred);
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        // Version label
        versionLabel.setText(PluginVersion::getFullVersionString(),
            juce::dontSendNotification);
        versionLabel.setFont(juce::Font(16.0f));
        versionLabel.setJustificationType(juce::Justification::centred);
        versionLabel.setColour(juce::Label::textColourId,
            juce::Colours::white.withAlpha(0.8f));
        addAndMakeVisible(versionLabel);

        // Build info label
        buildLabel.setText(PluginVersion::getVersionWithBuildInfo(),
            juce::dontSendNotification);
        buildLabel.setFont(juce::Font(12.0f));
        buildLabel.setJustificationType(juce::Justification::centred);
        buildLabel.setColour(juce::Label::textColourId,
            juce::Colours::white.withAlpha(0.5f));
        addAndMakeVisible(buildLabel);

        // Description
        descriptionLabel.setText("Index-Based VST Sampler\nManual index modification system",
            juce::dontSendNotification);
        descriptionLabel.setFont(juce::Font(13.0f));
        descriptionLabel.setJustificationType(juce::Justification::centred);
        descriptionLabel.setColour(juce::Label::textColourId,
            juce::Colours::white.withAlpha(0.6f));
        addAndMakeVisible(descriptionLabel);

        // Check for updates button
        checkUpdateButton.setButtonText("Check for Updates");
        checkUpdateButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff8b5cf6));
        checkUpdateButton.setColour(juce::TextButton::textColourOffId,
            juce::Colours::white);
        checkUpdateButton.onClick = [this] { onCheckForUpdates(); };
        addAndMakeVisible(checkUpdateButton);

        // Updater status label
        updateStatusLabel();
        statusLabel.setFont(juce::Font(11.0f));
        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setColour(juce::Label::textColourId,
            juce::Colours::white.withAlpha(0.4f));
        addAndMakeVisible(statusLabel);

        // GitHub link button
        githubButton.setButtonText("View on GitHub");
        githubButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff374151));
        githubButton.setColour(juce::TextButton::textColourOffId,
            juce::Colours::white.withAlpha(0.8f));
        githubButton.onClick = [this] { onOpenGitHub(); };
        addAndMakeVisible(githubButton);

        // Copyright label
        copyrightLabel.setText("© 2025 YourCompany. All rights reserved.",
            juce::dontSendNotification);
        copyrightLabel.setFont(juce::Font(10.0f));
        copyrightLabel.setJustificationType(juce::Justification::centred);
        copyrightLabel.setColour(juce::Label::textColourId,
            juce::Colours::white.withAlpha(0.3f));
        addAndMakeVisible(copyrightLabel);
    }

    void paint(juce::Graphics& g) override
    {
        // Background
        g.fillAll(juce::Colour(0xff2d2d2d));

        // Decorative elements
        auto bounds = getLocalBounds();

        // Top accent line
        g.setColour(juce::Colour(0xff8b5cf6));
        g.fillRect(bounds.removeFromTop(3));

        // Plugin icon area (if you have one)
        auto iconArea = bounds.reduced(20).removeFromTop(80);
        g.setColour(juce::Colour(0xff8b5cf6).withAlpha(0.2f));
        g.fillRoundedRectangle(iconArea.getCentreX() - 40,
            iconArea.getY(),
            80, 80, 10.0f);

        // Draw simple icon placeholder
        g.setColour(juce::Colour(0xff8b5cf6));
        g.setFont(juce::Font(36.0f, juce::Font::bold));
        g.drawText("S", iconArea, juce::Justification::centred);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);

        // Skip top for icon
        bounds.removeFromTop(100);

        // Title
        titleLabel.setBounds(bounds.removeFromTop(40));
        bounds.removeFromTop(5);

        // Version
        versionLabel.setBounds(bounds.removeFromTop(25));

        // Build info
        buildLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(10);

        // Description
        descriptionLabel.setBounds(bounds.removeFromTop(40));
        bounds.removeFromTop(20);

        // Check for updates button
        auto buttonArea = bounds.removeFromTop(40);
        checkUpdateButton.setBounds(buttonArea.reduced(40, 0));
        bounds.removeFromTop(10);

        // Status
        statusLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(20);

        // GitHub button
        buttonArea = bounds.removeFromTop(35);
        githubButton.setBounds(buttonArea.reduced(60, 0));

        // Copyright at bottom
        bounds = getLocalBounds();
        copyrightLabel.setBounds(bounds.removeFromBottom(30));
    }

private:
    juce::Label titleLabel;
    juce::Label versionLabel;
    juce::Label buildLabel;
    juce::Label descriptionLabel;
    juce::Label statusLabel;
    juce::Label copyrightLabel;
    juce::TextButton checkUpdateButton;
    juce::TextButton githubButton;

    void onCheckForUpdates()
    {
        DBG("?? User clicked: Check for Updates");

        if (UpdaterLauncher::isUpdaterInstalled())
        {
            // Launch Updater
            bool success = UpdaterLauncher::checkForUpdates();

            if (success)
            {
                juce::AlertWindow::showAsync(
                    juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::InfoIcon)
                    .withTitle("Checking for Updates")
                    .withMessage("The Updater has been launched.\n\n"
                        "It will check for updates and notify you if a new version is available.")
                    .withButton("OK"),
                    nullptr
                );
            }
            else
            {
                juce::AlertWindow::showAsync(
                    juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::WarningIcon)
                    .withTitle("Error")
                    .withMessage("Could not launch the Updater.\n\n"
                        "Please try reinstalling samp.")
                    .withButton("OK"),
                    nullptr
                );
            }
        }
        else
        {
            // Updater not found - show dialog
            UpdaterLauncher::showUpdaterNotFoundDialog();
        }

        updateStatusLabel();
    }

    void onOpenGitHub()
    {
        DBG("?? Opening GitHub repository");

        juce::URL url("https://github.com/xuxxn/samp");
        url.launchInDefaultBrowser();
    }

    void updateStatusLabel()
    {
        if (UpdaterLauncher::isUpdaterInstalled())
        {
            statusLabel.setText("? Updater installed", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId,
                juce::Colour(0xff10b981).withAlpha(0.7f)); // Green
        }
        else
        {
            statusLabel.setText("? Updater not found", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId,
                juce::Colour(0xfff59e0b).withAlpha(0.7f)); // Yellow
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutPanel)
};