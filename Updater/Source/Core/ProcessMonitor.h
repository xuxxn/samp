/*
  ProcessMonitor.h - Monitor running DAW processes
  
  Detects if any DAW is currently running that might be using the plugin
*/

#pragma once
#include <juce_core/juce_core.h>
#include "../Config.h"

class ProcessMonitor
{
public:
    /**
     * Check if any known DAW is currently running
     */
    static bool isAnyDAWRunning()
    {
        auto daws = UpdaterConfig::getKnownDAWProcesses();
        
        for (const auto& daw : daws)
        {
            if (isProcessRunning(daw))
            {
                UpdaterConfig::logMessage("Found running DAW: " + daw);
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * Get list of currently running DAWs
     */
    static juce::StringArray getRunningDAWs()
    {
        juce::StringArray running;
        auto daws = UpdaterConfig::getKnownDAWProcesses();
        
        for (const auto& daw : daws)
        {
            if (isProcessRunning(daw))
                running.add(daw);
        }
        
        return running;
    }
    
    /**
     * Check if a specific file is currently locked/in use
     */
    static bool isFileLocked(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;
        
        // Try to open for writing - if it fails, file is locked
        auto stream = file.createOutputStream();
        
        if (stream == nullptr)
        {
            UpdaterConfig::logMessage("File is locked: " + file.getFullPathName());
            return true; // File is locked
        }
        
        return false; // File is not locked
    }
    
    /**
     * Wait for file to become unlocked (with timeout)
     * Returns true if file became unlocked, false if timeout
     */
    static bool waitForFileUnlock(const juce::File& file, int timeoutMs = 5000)
    {
        UpdaterConfig::logMessage("Waiting for file to unlock: " + 
                                 file.getFullPathName());
        
        auto startTime = juce::Time::getMillisecondCounter();
        
        while (juce::Time::getMillisecondCounter() - startTime < timeoutMs)
        {
            if (!isFileLocked(file))
            {
                UpdaterConfig::logMessage("File unlocked!");
                return true;
            }
            
            juce::Thread::sleep(100); // Check every 100ms
        }
        
        UpdaterConfig::logMessage("Timeout waiting for file unlock");
        return false;
    }

private:
    /**
     * Check if a process with given name is running
     */
    static bool isProcessRunning(const juce::String& processName)
    {
        #if JUCE_WINDOWS
            return isProcessRunningWindows(processName);
        #elif JUCE_MAC
            return isProcessRunningMac(processName);
        #else
            return false;
        #endif
    }
    
    #if JUCE_WINDOWS
    static bool isProcessRunningWindows(const juce::String& processName)
    {
        // Use Windows API to check running processes
        juce::ChildProcess process;
        
        // Use tasklist command
        juce::String command = "tasklist /FI \"IMAGENAME eq " + processName + "\" /NH";
        
        if (process.start(command))
        {
            auto output = process.readAllProcessOutput();
            
            // If process is running, output will contain the process name
            return output.contains(processName.upToFirstOccurrenceOf("*", false, false));
        }
        
        return false;
    }
    #endif
    
    #if JUCE_MAC
    static bool isProcessRunningMac(const juce::String& processName)
    {
        // Use ps command on macOS
        juce::ChildProcess process;
        
        juce::String command = "ps aux | grep \"" + processName + "\" | grep -v grep";
        
        if (process.start(command))
        {
            auto output = process.readAllProcessOutput();
            return output.isNotEmpty();
        }
        
        return false;
    }
    #endif
};