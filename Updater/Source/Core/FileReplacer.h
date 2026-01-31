/*
  FileReplacer.h - Safe file replacement with backup
  
  Handles replacing the plugin file safely with automatic backup
*/

#pragma once
#include <juce_core/juce_core.h>
#include "../Config.h"
#include "ProcessMonitor.h"

class FileReplacer
{
public:
    enum class Result
    {
        Success,
        FileLocked,
        BackupFailed,
        CopyFailed,
        PermissionDenied,
        FileNotFound
    };
    
    /**
     * Replace plugin file with new version
     * 
     * @param newFile - The new plugin file to install
     * @param createBackup - Whether to create backup of old file
     * @return Result indicating success or failure reason
     */
    static Result replacePlugin(const juce::File& newFile, bool createBackup = true)
    {
        auto targetFile = UpdaterConfig::getPluginInstallPath();
        
        UpdaterConfig::logMessage("===========================================");
        UpdaterConfig::logMessage("REPLACING PLUGIN FILE");
        UpdaterConfig::logMessage("From: " + newFile.getFullPathName());
        UpdaterConfig::logMessage("To: " + targetFile.getFullPathName());
        UpdaterConfig::logMessage("===========================================");
        
        // 1. Check if new file exists
        if (!newFile.existsAsFile())
        {
            UpdaterConfig::logMessage("ERROR: New file does not exist");
            return Result::FileNotFound;
        }
        
        // 2. Check if target file is locked
        if (targetFile.existsAsFile() && ProcessMonitor::isFileLocked(targetFile))
        {
            UpdaterConfig::logMessage("ERROR: Target file is locked");
            
            // Wait for unlock (5 seconds)
            if (!ProcessMonitor::waitForFileUnlock(targetFile, 5000))
            {
                return Result::FileLocked;
            }
        }
        
        // 3. Create backup if requested
        if (createBackup && targetFile.existsAsFile())
        {
            UpdaterConfig::logMessage("Creating backup...");
            
            auto backupFile = UpdaterConfig::getBackupFile();
            
            if (!targetFile.copyFileTo(backupFile))
            {
                UpdaterConfig::logMessage("ERROR: Failed to create backup");
                return Result::BackupFailed;
            }
            
            UpdaterConfig::logMessage("Backup created: " + backupFile.getFullPathName());
        }
        
        // 4. Ensure target directory exists
        targetFile.getParentDirectory().createDirectory();
        
        // 5. Replace file
        UpdaterConfig::logMessage("Copying new file...");
        
        // Delete old file first
        if (targetFile.existsAsFile())
        {
            if (!targetFile.deleteFile())
            {
                UpdaterConfig::logMessage("ERROR: Failed to delete old file");
                
                // Try to restore backup
                if (createBackup)
                    restoreBackup();
                
                return Result::PermissionDenied;
            }
        }
        
        // Copy new file
        if (!newFile.copyFileTo(targetFile))
        {
            UpdaterConfig::logMessage("ERROR: Failed to copy new file");
            
            // Try to restore backup
            if (createBackup)
                restoreBackup();
            
            return Result::CopyFailed;
        }
        
        UpdaterConfig::logMessage("✅ Plugin replaced successfully!");
        UpdaterConfig::logMessage("===========================================");
        
        return Result::Success;
    }
    
    /**
     * Restore plugin from backup
     */
    static bool restoreBackup()
    {
        UpdaterConfig::logMessage("Restoring from backup...");
        
        auto backupFile = UpdaterConfig::getBackupFile();
        auto targetFile = UpdaterConfig::getPluginInstallPath();
        
        if (!backupFile.existsAsFile())
        {
            UpdaterConfig::logMessage("ERROR: Backup file does not exist");
            return false;
        }
        
        // Delete current file
        if (targetFile.existsAsFile())
            targetFile.deleteFile();
        
        // Restore backup
        if (backupFile.copyFileTo(targetFile))
        {
            UpdaterConfig::logMessage("✅ Backup restored successfully");
            return true;
        }
        else
        {
            UpdaterConfig::logMessage("ERROR: Failed to restore backup");
            return false;
        }
    }
    
    /**
     * Delete backup file
     */
    static void deleteBackup()
    {
        auto backupFile = UpdaterConfig::getBackupFile();
        
        if (backupFile.existsAsFile())
        {
            backupFile.deleteFile();
            UpdaterConfig::logMessage("Backup deleted");
        }
    }
    
    /**
     * Get human-readable error message for Result
     */
    static juce::String getErrorMessage(Result result)
    {
        switch (result)
        {
            case Result::Success:
                return "Success";
            case Result::FileLocked:
                return "Plugin file is locked. Please close your DAW and try again.";
            case Result::BackupFailed:
                return "Failed to create backup. Update cancelled.";
            case Result::CopyFailed:
                return "Failed to copy new file. Please check permissions.";
            case Result::PermissionDenied:
                return "Permission denied. Try running as administrator.";
            case Result::FileNotFound:
                return "Update file not found.";
            default:
                return "Unknown error";
        }
    }
    
    /**
     * Extract .zip if downloaded file is zipped
     */
    static juce::File extractIfNeeded(const juce::File& file)
    {
        if (file.getFileExtension().equalsIgnoreCase(".zip"))
        {
            UpdaterConfig::logMessage("Extracting ZIP file...");
            
            auto tempDir = UpdaterConfig::getTempDownloadDir();
            
            // Extract ZIP
            juce::ZipFile zip(file);
            
            if (zip.uncompressTo(tempDir).wasOk())
            {
                UpdaterConfig::logMessage("ZIP extracted successfully");
                
                // Find .vst3 file in extracted files
                for (int i = 0; i < zip.getNumEntries(); ++i)
                {
                    auto entry = zip.getEntry(i);
                    
                    if (entry->filename.endsWithIgnoreCase(".vst3"))
                    {
                        auto extractedFile = tempDir.getChildFile(entry->filename);
                        UpdaterConfig::logMessage("Found VST3: " + extractedFile.getFullPathName());
                        return extractedFile;
                    }
                }
            }
            else
            {
                UpdaterConfig::logMessage("ERROR: Failed to extract ZIP");
            }
        }
        
        return file; // Return original if not ZIP or extraction failed
    }
};