/*
  MainWindow.h - Main UI Window for Updater
  
  Shows update status, download progress, and install button
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Config.h"
#include "../Core/UpdateManager.h"
#include "../Core/GitHubAPI.h"

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(UpdateManager& manager)
        : juce::DocumentWindow("samp Updater",
                              juce::Colours::darkgrey,
                              juce::DocumentWindow::allButtons),
          updateManager(manager)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new ContentComponent(updateManager), true);
        
        centreWithSize(UpdaterConfig::WINDOW_WIDTH, 
                      UpdaterConfig::WINDOW_HEIGHT);
        
        setResizable(false, false);
        setVisible(true);
    }
    
    void closeButtonPressed() override
    {
        // Hide instead of closing (can be reopened)
        setVisible(false);
    }
    
    void setDownloadProgress(float progress)
    {
        if (auto* content = dynamic_cast<ContentComponent*>(getContentComponent()))
        {
            content->setProgress(progress);
        }
    }
    
    void updateUI()
    {
        if (auto* content = dynamic_cast<ContentComponent*>(getContentComponent()))
        {
            content->updateDisplay();
        }
    }
    
    void showUpdateAvailable(const GitHubAPI::ReleaseInfo& release)
    {
        if (auto* content = dynamic_cast<ContentComponent*>(getContentComponent()))
        {
            content->showUpdateInfo(release);
        }
    }
    
private:
    //==========================================================================
    // CONTENT COMPONENT
    //==========================================================================
    
    class ContentComponent : public juce::Component
    {
    public:
        ContentComponent(UpdateManager& manager) : updateManager(manager)
        {
            // Title label
            titleLabel.setText("samp Updater", juce::dontSendNotification);
            titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
            titleLabel.setJustificationType(juce::Justification::centred);
            titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible(titleLabel);
            
            // Status label
            statusLabel.setText("Ready", juce::dontSendNotification);
            statusLabel.setFont(juce::Font(14.0f));
            statusLabel.setJustificationType(juce::Justification::centred);
            statusLabel.setColour(juce::Label::textColourId, 
                                juce::Colours::white.withAlpha(0.7f));
            addAndMakeVisible(statusLabel);
            
            // Version info label
            versionLabel.setText("Current version: Unknown", juce::dontSendNotification);
            versionLabel.setFont(juce::Font(13.0f));
            versionLabel.setJustificationType(juce::Justification::centred);
            versionLabel.setColour(juce::Label::textColourId, 
                                  juce::Colours::white.withAlpha(0.6f));
            addAndMakeVisible(versionLabel);
            
            // Check button
            checkButton.setButtonText("Check for Updates");
            checkButton.onClick = [this] { handleCheckButton(); };
            addAndMakeVisible(checkButton);
            
            // Download button
            downloadButton.setButtonText("Download Update");
            downloadButton.onClick = [this] { handleDownloadButton(); };
            downloadButton.setEnabled(false);
            addAndMakeVisible(downloadButton);
            
            // Install button
            installButton.setButtonText("Install Update");
            installButton.onClick = [this] { handleInstallButton(); };
            installButton.setEnabled(false);
            addAndMakeVisible(installButton);
            
            // Progress bar
            addAndMakeVisible(progressBar);
            progressBar.setVisible(false);
            
            // Changelog text
            changelogText.setMultiLine(true);
            changelogText.setReadOnly(true);
            changelogText.setScrollbarsShown(true);
            changelogText.setColour(juce::TextEditor::backgroundColourId, 
                                   juce::Colour(0xff2d2d2d));
            changelogText.setColour(juce::TextEditor::textColourId, 
                                   juce::Colours::white.withAlpha(0.8f));
            addAndMakeVisible(changelogText);
            changelogText.setVisible(false);
            
            updateDisplay();
        }
        
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff1e1e1e));
            
            // Top accent
            g.setColour(juce::Colour(0xff8b5cf6));
            g.fillRect(0, 0, getWidth(), 3);
        }
        
        void resized() override
        {
            auto bounds = getLocalBounds().reduced(20);
            bounds.removeFromTop(20); // Top padding
            
            titleLabel.setBounds(bounds.removeFromTop(40));
            bounds.removeFromTop(10);
            
            versionLabel.setBounds(bounds.removeFromTop(25));
            bounds.removeFromTop(5);
            
            statusLabel.setBounds(bounds.removeFromTop(30));
            bounds.removeFromTop(20);
            
            // Buttons row
            auto buttonRow = bounds.removeFromTop(40);
            int buttonWidth = (buttonRow.getWidth() - 20) / 3;
            
            checkButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
            buttonRow.removeFromLeft(10);
            downloadButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
            buttonRow.removeFromLeft(10);
            installButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
            
            bounds.removeFromTop(20);
            
            // Progress bar
            progressBar.setBounds(bounds.removeFromTop(20));
            bounds.removeFromTop(10);
            
            // Changelog
            changelogText.setBounds(bounds);
        }
        
        void setProgress(float progress)
        {
            progressBar.setVisible(true);
            progressBar.setPercentageDisplay(true);
            
            // JUCE ProgressBar takes value 0.0-1.0
            progressValue = progress;
            repaint();
        }
        
        void updateDisplay()
        {
            auto state = updateManager.getState();
            
            switch (state)
            {
                case UpdateManager::State::Idle:
                    statusLabel.setText("Ready", juce::dontSendNotification);
                    checkButton.setEnabled(true);
                    downloadButton.setEnabled(false);
                    installButton.setEnabled(false);
                    progressBar.setVisible(false);
                    break;
                    
                case UpdateManager::State::CheckingForUpdates:
                    statusLabel.setText("Checking for updates...", juce::dontSendNotification);
                    checkButton.setEnabled(false);
                    break;
                    
                case UpdateManager::State::UpdateAvailable:
                    statusLabel.setText("Update available!", juce::dontSendNotification);
                    checkButton.setEnabled(true);
                    downloadButton.setEnabled(true);
                    break;
                    
                case UpdateManager::State::Downloading:
                    statusLabel.setText("Downloading...", juce::dontSendNotification);
                    downloadButton.setEnabled(false);
                    progressBar.setVisible(true);
                    break;
                    
                case UpdateManager::State::ReadyToInstall:
                    statusLabel.setText("Ready to install", juce::dontSendNotification);
                    installButton.setEnabled(true);
                    progressBar.setVisible(false);
                    break;
                    
                case UpdateManager::State::Installing:
                    statusLabel.setText("Installing...", juce::dontSendNotification);
                    installButton.setEnabled(false);
                    break;
                    
                case UpdateManager::State::Installed:
                    statusLabel.setText("✅ Updated successfully!", juce::dontSendNotification);
                    checkButton.setEnabled(true);
                    downloadButton.setEnabled(false);
                    installButton.setEnabled(false);
                    break;
                    
                case UpdateManager::State::Error:
                    statusLabel.setText("Error occurred", juce::dontSendNotification);
                    checkButton.setEnabled(true);
                    downloadButton.setEnabled(false);
                    installButton.setEnabled(false);
                    progressBar.setVisible(false);
                    break;
            }
            
            repaint();
        }
        
        void showUpdateInfo(const GitHubAPI::ReleaseInfo& release)
        {
            versionLabel.setText("Latest version: v" + release.version + 
                               " (" + release.getFileSizeString() + ")", 
                               juce::dontSendNotification);
            
            // Show changelog
            changelogText.setText(release.changelog, false);
            changelogText.setVisible(true);
        }
        
    private:
        void handleCheckButton()
        {
            updateManager.checkForUpdates();
        }
        
        void handleDownloadButton()
        {
            updateManager.downloadUpdate();
        }
        
        void handleInstallButton()
        {
            updateManager.installUpdate();
        }
        
        UpdateManager& updateManager;
        
        juce::Label titleLabel;
        juce::Label statusLabel;
        juce::Label versionLabel;
        juce::TextButton checkButton;
        juce::TextButton downloadButton;
        juce::TextButton installButton;
        juce::ProgressBar progressBar { progressValue };
        juce::TextEditor changelogText;
        
        double progressValue = 0.0;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContentComponent)
    };
    
    //==========================================================================
    
    UpdateManager& updateManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};