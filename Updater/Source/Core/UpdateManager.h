/*
  UpdateManager.h - Coordinates the entire update process
  
  Main orchestrator for checking, downloading, and installing updates
*/

#pragma once
#include <juce_core/juce_core.h>
#include "../Config.h"
#include "GitHubAPI.h"
#include "FileReplacer.h"
#include "ProcessMonitor.h"

class UpdateManager : public juce::Thread
{
public:
    //==========================================================================
    // UPDATE STATE
    //==========================================================================
    
    enum class State
    {
        Idle,
        CheckingForUpdates,
        UpdateAvailable,
        Downloading,
        ReadyToInstall,
        Installing,
        Installed,
        Error
    };
    
    //==========================================================================
    
    UpdateManager() : juce::Thread("UpdateManager")
    {
    }
    
    ~UpdateManager() override
    {
        stopThread(2000);
    }
    
    //==========================================================================
    // PUBLIC API
    //==========================================================================
    
    /**
     * Check for updates asynchronously
     */
    void checkForUpdates()
    {
        if (!isThreadRunning())
        {
            currentState = State::CheckingForUpdates;
            startThread();
        }
    }
    
    /**
     * Download available update
     */
    void downloadUpdate()
    {
        if (currentState == State::UpdateAvailable && !isThreadRunning())
        {
            currentState = State::Downloading;
            startThread();
        }
    }
    
    /**
     * Install downloaded update
     */
    void installUpdate()
    {
        if (currentState == State::ReadyToInstall)
        {
            currentState = State::Installing;
            performInstall();
        }
    }
    
    //==========================================================================
    // GETTERS
    //==========================================================================
    
    State getState() const { return currentState; }
    GitHubAPI::ReleaseInfo getLatestRelease() const { return latestRelease; }
    float getDownloadProgress() const { return downloadProgress; }
    juce::String getErrorMessage() const { return errorMessage; }
    
    //==========================================================================
    // CALLBACKS
    //==========================================================================
    
    std::function<void(State)> onStateChanged;
    std::function<void(float)> onDownloadProgress;
    
private:
    //==========================================================================
    // THREAD RUN
    //==========================================================================
    
    void run() override
    {
        if (currentState == State::CheckingForUpdates)
        {
            performCheck();
        }
        else if (currentState == State::Downloading)
        {
            performDownload();
        }
    }
    
    //==========================================================================
    // IMPLEMENTATION
    //==========================================================================
    
    void performCheck()
    {
        UpdaterConfig::logMessage("Checking for updates...");
        
        latestRelease = GitHubAPI::getLatestRelease(false);
        
        if (latestRelease.isValid())
        {
            UpdaterConfig::logMessage("Latest version: " + latestRelease.version);
            
            // Compare with current version (simplified - just check if different)
            changeState(State::UpdateAvailable);
        }
        else
        {
            UpdaterConfig::logMessage("No updates found or error");
            errorMessage = "Failed to check for updates";
            changeState(State::Error);
        }
    }
    
    void performDownload()
    {
        UpdaterConfig::logMessage("Starting download...");
        
        auto tempDir = UpdaterConfig::getTempDownloadDir();
        tempDir.createDirectory();
        
        downloadedFile = tempDir.getChildFile("samp_update.vst3");
        
        bool success = GitHubAPI::downloadFile(
            latestRelease.downloadUrl,
            downloadedFile,
            [this](float progress, int bytes, int total)
            {
                downloadProgress = progress;
                
                if (onDownloadProgress)
                {
                    juce::MessageManager::callAsync([this]()
                    {
                        if (onDownloadProgress)
                            onDownloadProgress(downloadProgress);
                    });
                }
            }
        );
        
        if (success)
        {
            UpdaterConfig::logMessage("Download complete!");
            
            // Extract if ZIP
            downloadedFile = FileReplacer::extractIfNeeded(downloadedFile);
            
            changeState(State::ReadyToInstall);
        }
        else
        {
            UpdaterConfig::logMessage("Download failed");
            errorMessage = "Failed to download update";
            changeState(State::Error);
        }
    }
    
    void performInstall()
    {
        UpdaterConfig::logMessage("Installing update...");
        
        // Check if DAW is running
        if (ProcessMonitor::isAnyDAWRunning())
        {
            errorMessage = "Cannot install: DAW is running.\n\n"
                         "Please close your DAW and try again.";
            changeState(State::Error);
            return;
        }
        
        // Replace plugin file
        auto result = FileReplacer::replacePlugin(downloadedFile, true);
        
        if (result == FileReplacer::Result::Success)
        {
            UpdaterConfig::logMessage("✅ Update installed successfully!");
            
            // Cleanup
            downloadedFile.deleteFile();
            
            changeState(State::Installed);
        }
        else
        {
            errorMessage = FileReplacer::getErrorMessage(result);
            UpdaterConfig::logMessage("Installation failed: " + errorMessage);
            changeState(State::Error);
        }
    }
    
    //==========================================================================
    
    void changeState(State newState)
    {
        currentState = newState;
        
        if (onStateChanged)
        {
            juce::MessageManager::callAsync([this, newState]()
            {
                if (onStateChanged)
                    onStateChanged(newState);
            });
        }
    }
    
    //==========================================================================
    // STATE
    //==========================================================================
    
    State currentState = State::Idle;
    GitHubAPI::ReleaseInfo latestRelease;
    juce::File downloadedFile;
    float downloadProgress = 0.0f;
    juce::String errorMessage;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateManager)
};