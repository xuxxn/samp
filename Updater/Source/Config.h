/*
  Config.h - Configuration for samp Updater
  
  All paths, URLs, and settings in one place
*/

#pragma once
#include <juce_core/juce_core.h>

namespace UpdaterConfig
{
    //==========================================================================
    // GITHUB SETTINGS
    //==========================================================================
    
    inline const char* GITHUB_OWNER = "xuxxn";
    inline const char* GITHUB_REPO = "samp";
    
    inline juce::String getGitHubAPIUrl()
    {
        return "https://api.github.com/repos/" + 
               juce::String(GITHUB_OWNER) + "/" + 
               juce::String(GITHUB_REPO) + "/releases/latest";
    }
    
    //==========================================================================
    // PLUGIN INFORMATION
    //==========================================================================
    
    inline const char* COMPANY_NAME = "YourCompany";
    inline const char* PLUGIN_NAME = "samp.vst3";
    inline const char* PLUGIN_DISPLAY_NAME = "samp";
    
    //==========================================================================
    // INSTALLATION PATHS
    //==========================================================================
    
    /**
     * Get plugin installation path
     * Windows: %LOCALAPPDATA%\YourCompany\VST3\samp.vst3
     * macOS: ~/Library/Audio/Plug-Ins/VST3/samp.vst3
     */
    inline juce::File getPluginInstallPath()
    {
        #if JUCE_WINDOWS
            auto localAppData = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory);
            
            return localAppData
                .getChildFile(COMPANY_NAME)
                .getChildFile("VST3")
                .getChildFile(PLUGIN_NAME);
                
        #elif JUCE_MAC
            auto audioPlugins = juce::File::getSpecialLocation(
                juce::File::userApplicationDataDirectory)
                .getChildFile("Audio")
                .getChildFile("Plug-Ins")
                .getChildFile("VST3");
            
            return audioPlugins.getChildFile(PLUGIN_NAME);
            
        #else
            return juce::File();
        #endif
    }
    
    /**
     * Get Updater preferences file path
     */
    inline juce::File getPreferencesFile()
    {
        auto appData = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);
        
        return appData
            .getChildFile(COMPANY_NAME)
            .getChildFile("updater_prefs.xml");
    }
    
    /**
     * Get log file path
     */
    inline juce::File getLogFile()
    {
        auto appData = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);
        
        return appData
            .getChildFile(COMPANY_NAME)
            .getChildFile("updater.log");
    }
    
    /**
     * Get temp directory for downloads
     */
    inline juce::File getTempDownloadDir()
    {
        auto temp = juce::File::getSpecialLocation(
            juce::File::tempDirectory);
        
        return temp.getChildFile("samp_update");
    }
    
    /**
     * Get backup file path
     */
    inline juce::File getBackupFile()
    {
        auto plugin = getPluginInstallPath();
        return plugin.withFileExtension(".vst3.backup");
    }
    
    //==========================================================================
    // UPDATE SETTINGS
    //==========================================================================
    
    // Check for updates every 24 hours
    inline constexpr int CHECK_INTERVAL_HOURS = 24;
    
    // Auto-update enabled by default
    inline constexpr bool AUTO_UPDATE_DEFAULT = true;
    
    // Auto-start with Windows (disabled by default)
    inline constexpr bool START_WITH_WINDOWS_DEFAULT = false;
    
    // Check for beta versions
    inline constexpr bool CHECK_BETA_DEFAULT = false;
    
    //==========================================================================
    // UI SETTINGS
    //==========================================================================
    
    inline constexpr int WINDOW_WIDTH = 500;
    inline constexpr int WINDOW_HEIGHT = 400;
    
    //==========================================================================
    // KNOWN DAW PROCESSES (for process monitoring)
    //==========================================================================
    
    inline juce::StringArray getKnownDAWProcesses()
    {
        return {
            "fl64.exe",           // FL Studio
            "fl.exe",             // FL Studio 32-bit
            "Ableton Live.exe",   // Ableton
            "Live.exe",           // Ableton (alternative)
            "Cubase*.exe",        // Cubase (wildcard)
            "Studio One.exe",     // Studio One
            "reaper.exe",         // Reaper
            "REAPER.exe",         // Reaper (alternative case)
            "Logic Pro X",        // Logic Pro (macOS)
            "Bitwig Studio.exe",  // Bitwig
            "Renoise.exe"         // Renoise
        };
    }
    
    //==========================================================================
    // LOGGING
    //==========================================================================
    
    inline void logMessage(const juce::String& message)
    {
        auto logFile = getLogFile();
        
        // Create parent directory if needed
        logFile.getParentDirectory().createDirectory();
        
        // Append to log file
        if (auto stream = logFile.createOutputStream())
        {
            auto timestamp = juce::Time::getCurrentTime().toString(true, true);
            stream->writeText("[" + timestamp + "] " + message + "\n", 
                            false, false, nullptr);
        }
        
        // Also output to debug console
        DBG(message);
    }
    
    //==========================================================================
    // HELPER FUNCTIONS
    //==========================================================================
    
    /**
     * Check if plugin is installed
     */
    inline bool isPluginInstalled()
    {
        return getPluginInstallPath().existsAsFile();
    }
    
    /**
     * Get plugin version from filename or metadata
     * (For now returns empty - will be populated when we have version info)
     */
    inline juce::String getInstalledPluginVersion()
    {
        // TODO: Read version from plugin file metadata
        // For now, we'll just check if file exists
        return isPluginInstalled() ? "Unknown" : "Not Installed";
    }
    
    /**
     * Print configuration info to debug console
     */
    inline void printConfig()
    {
        DBG("===========================================");
        DBG("Updater Configuration:");
        DBG("===========================================");
        DBG("GitHub: " + juce::String(GITHUB_OWNER) + "/" + GITHUB_REPO);
        DBG("API URL: " + getGitHubAPIUrl());
        DBG("Plugin Path: " + getPluginInstallPath().getFullPathName());
        DBG("Prefs File: " + getPreferencesFile().getFullPathName());
        DBG("Log File: " + getLogFile().getFullPathName());
        DBG("Plugin Installed: " + juce::String(isPluginInstalled() ? "Yes" : "No"));
        DBG("===========================================");
    }
}