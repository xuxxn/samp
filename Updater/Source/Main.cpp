/*
  Main.cpp - Entry point for samp Updater
  
  This is a standalone JUCE application that:
  - Checks for updates from GitHub
  - Downloads and installs updates
  - Can run in system tray
*/

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Config.h"
#include "Core/UpdaterApp.h"

//==============================================================================
class sampUpdaterApplication : public juce::JUCEApplication
{
public:
    //==========================================================================
    sampUpdaterApplication() = default;

    const juce::String getApplicationName() override       
    { 
        return "samp Updater"; 
    }
    
    const juce::String getApplicationVersion() override    
    { 
        return "1.0.0"; 
    }
    
    bool moreThanOneInstanceAllowed() override             
    { 
        return false; // Only one instance allowed
    }

    //==========================================================================
    void initialise(const juce::String& commandLine) override
    {
        UpdaterConfig::logMessage("===========================================");
        UpdaterConfig::logMessage("samp Updater Starting...");
        UpdaterConfig::logMessage("Version: " + getApplicationVersion());
        UpdaterConfig::logMessage("Command line: " + commandLine);
        UpdaterConfig::printConfig();
        
        // Create main updater app
        updaterApp = std::make_unique<UpdaterApp>();
        
        // Parse command line arguments
        if (commandLine.contains("--check-now"))
        {
            UpdaterConfig::logMessage("Command: Check for updates immediately");
            updaterApp->checkForUpdatesAsync();
        }
        else if (commandLine.contains("--silent"))
        {
            UpdaterConfig::logMessage("Command: Run in silent mode (tray only)");
            updaterApp->showTrayOnly();
        }
        else if (commandLine.contains("--install-now"))
        {
            UpdaterConfig::logMessage("Command: Install pending update");
            updaterApp->installPendingUpdate();
        }
        else
        {
            UpdaterConfig::logMessage("Command: Show main window");
            updaterApp->showMainWindow();
        }
    }

    void shutdown() override
    {
        UpdaterConfig::logMessage("samp Updater Shutting Down...");
        UpdaterConfig::logMessage("===========================================");
        
        updaterApp = nullptr;
    }

    //==========================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit
        // You can do cleanup here
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        // Called when another instance tries to start
        UpdaterConfig::logMessage("Another instance attempted to start: " + commandLine);
        
        // Show main window if another instance tries to start
        if (updaterApp)
            updaterApp->showMainWindow();
    }

private:
    std::unique_ptr<UpdaterApp> updaterApp;
};

//==============================================================================
// This macro generates the main() routine that launches the app
START_JUCE_APPLICATION(sampUpdaterApplication)