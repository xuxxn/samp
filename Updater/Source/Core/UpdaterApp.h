/*
  UpdaterApp.h - Main Updater Application Class
  
  Coordinates UI, update checks, and user interaction
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Config.h"
#include "UpdateManager.h"
#include "../UI/MainWindow.h"

class UpdaterApp
{
public:
    UpdaterApp()
    {
        UpdaterConfig::logMessage("UpdaterApp initialized");
        
        // Create update manager
        updateManager = std::make_unique<UpdateManager>();
        
        // Setup callbacks
        updateManager->onStateChanged = [this](UpdateManager::State state)
        {
            handleStateChanged(state);
        };
        
        updateManager->onDownloadProgress = [this](float progress)
        {
            if (mainWindow)
                mainWindow->setDownloadProgress(progress);
        };
    }
    
    ~UpdaterApp()
    {
        mainWindow = nullptr;
        updateManager = nullptr;
    }
    
    //==========================================================================
    // COMMANDS (called from Main.cpp)
    //==========================================================================
    
    /**
     * Show main window
     */
    void showMainWindow()
    {
        if (!mainWindow)
        {
            mainWindow = std::make_unique<MainWindow>(*updateManager);
        }
        
        mainWindow->setVisible(true);
        mainWindow->toFront(true);
    }
    
    /**
     * Check for updates immediately
     */
    void checkForUpdatesAsync()
    {
        UpdaterConfig::logMessage("User requested: Check for updates");
        
        showMainWindow();
        updateManager->checkForUpdates();
    }
    
    /**
     * Show tray icon only (minimize to tray)
     */
    void showTrayOnly()
    {
        // TODO: Implement system tray in Phase 1.5
        UpdaterConfig::logMessage("Tray mode not yet implemented");
        showMainWindow();
    }
    
    /**
     * Install pending update
     */
    void installPendingUpdate()
    {
        if (updateManager->getState() == UpdateManager::State::ReadyToInstall)
        {
            updateManager->installUpdate();
        }
        else
        {
            UpdaterConfig::logMessage("No pending update to install");
        }
    }
    
private:
    //==========================================================================
    
    void handleStateChanged(UpdateManager::State state)
    {
        UpdaterConfig::logMessage("State changed: " + getStateString(state));
        
        if (mainWindow)
        {
            mainWindow->updateUI();
        }
        
        // Show notifications or alerts based on state
        if (state == UpdateManager::State::UpdateAvailable)
        {
            auto release = updateManager->getLatestRelease();
            
            UpdaterConfig::logMessage("Update available: v" + release.version);
            
            if (mainWindow)
            {
                mainWindow->showUpdateAvailable(release);
            }
        }
        else if (state == UpdateManager::State::Installed)
        {
            if (mainWindow)
            {
                juce::AlertWindow::showAsync(
                    juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::InfoIcon)
                        .withTitle("Update Installed")
                        .withMessage("samp has been updated successfully!\n\n"
                                   "The new version will be active next time you load the plugin.")
                        .withButton("OK"),
                    nullptr
                );
            }
        }
        else if (state == UpdateManager::State::Error)
        {
            if (mainWindow)
            {
                juce::AlertWindow::showAsync(
                    juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::WarningIcon)
                        .withTitle("Update Error")
                        .withMessage(updateManager->getErrorMessage())
                        .withButton("OK"),
                    nullptr
                );
            }
        }
    }
    
    juce::String getStateString(UpdateManager::State state)
    {
        switch (state)
        {
            case UpdateManager::State::Idle: return "Idle";
            case UpdateManager::State::CheckingForUpdates: return "Checking for updates";
            case UpdateManager::State::UpdateAvailable: return "Update available";
            case UpdateManager::State::Downloading: return "Downloading";
            case UpdateManager::State::ReadyToInstall: return "Ready to install";
            case UpdateManager::State::Installing: return "Installing";
            case UpdateManager::State::Installed: return "Installed";
            case UpdateManager::State::Error: return "Error";
            default: return "Unknown";
        }
    }
    
    //==========================================================================
    
    std::unique_ptr<UpdateManager> updateManager;
    std::unique_ptr<MainWindow> mainWindow;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdaterApp)
};