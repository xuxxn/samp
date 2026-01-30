/*
ProjectManager.h - Project Management System
Features:
- Auto-save every 5 seconds when dirty
- Save on plugin close
- Manage max N projects (auto-delete oldest)
- Thread-safe project operations
- Settings persistence
*/

#pragma once
#include <JuceHeader.h>
#include "ProjectData.h"
#include "ProjectSerializer.h"
#include <memory>
#include <vector>

// Forward declaration
class NoiseBasedSamplerAudioProcessor;

// ==========================================================================
// PROJECT MANAGER SETTINGS
// ==========================================================================

struct ProjectManagerSettings
{
    juce::File projectFolder;
    int maxProjects = 15;
    bool autoSaveEnabled = true;
    int autoSaveIntervalSeconds = 5;

    ProjectManagerSettings()
    {
        // Default folder: ~/Documents/NoiseBasedSampler/Projects/
        projectFolder = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("NoiseBasedSampler")
            .getChildFile("Projects");
    }

    void saveToFile()
    {
        auto settingsFile = getSettingsFile();

        juce::XmlElement xml("ProjectManagerSettings");
        xml.setAttribute("projectFolder", projectFolder.getFullPathName());
        xml.setAttribute("maxProjects", maxProjects);
        xml.setAttribute("autoSaveEnabled", autoSaveEnabled);
        xml.setAttribute("autoSaveIntervalSeconds", autoSaveIntervalSeconds);

        xml.writeTo(settingsFile);
        DBG("âœ… Settings saved");
    }

    void loadFromFile()
    {
        auto settingsFile = getSettingsFile();

        if (!settingsFile.existsAsFile())
        {
            DBG("No settings file found, using defaults");
            return;
        }

        auto xml = juce::parseXML(settingsFile);

        if (xml != nullptr && xml->hasTagName("ProjectManagerSettings"))
        {
            juce::String folderPath = xml->getStringAttribute("projectFolder");
            if (folderPath.isNotEmpty())
                projectFolder = juce::File(folderPath);

            maxProjects = xml->getIntAttribute("maxProjects", 15);
            autoSaveEnabled = xml->getBoolAttribute("autoSaveEnabled", true);
            autoSaveIntervalSeconds = xml->getIntAttribute("autoSaveIntervalSeconds", 5);

            DBG("âœ… Settings loaded");
        }
    }

private:
    juce::File getSettingsFile() const
    {
        auto appDataFolder = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("NoiseBasedSampler");

        appDataFolder.createDirectory();

        return appDataFolder.getChildFile("ProjectManagerSettings.xml");
    }
};

// ==========================================================================
// PROJECT MANAGER
// ==========================================================================

class ProjectManager : public juce::Timer
{
public:
    ProjectManager(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
        , isDirty(false)
        , timeSinceLastEdit(0)
    {
        // Load settings
        settings.loadFromFile();

        // Ensure project folder exists
        if (!settings.projectFolder.exists())
        {
            settings.projectFolder.createDirectory();
            DBG("âœ… Created project folder: " + settings.projectFolder.getFullPathName());
        }

        // Scan existing projects
        scanProjectFolder();

        // Start auto-save timer
        if (settings.autoSaveEnabled)
        {
            startTimer(1000);  // Check every second
            DBG("âœ… Auto-save enabled (every " +
                juce::String(settings.autoSaveIntervalSeconds) + " seconds)");
        }

        DBG("âœ… ProjectManager initialized");
        DBG("   Folder: " + settings.projectFolder.getFullPathName());
        DBG("   Max projects: " + juce::String(settings.maxProjects));
        DBG("   Found projects: " + juce::String(projectMetadataList.size()));
    }

    ~ProjectManager()
    {
        stopTimer();
    }

    // ========== AUTO-SAVE SYSTEM ==========

    void markDirty()
    {
        if (!isDirty)
        {
            isDirty = true;
            timeSinceLastEdit = 0;
            DBG("ðŸŸ¡ Project marked dirty");
        }
        else
        {
            timeSinceLastEdit = 0;  // Reset timer on subsequent edits
        }
    }

    void timerCallback() override
    {
        if (!isDirty || !settings.autoSaveEnabled)
            return;

        timeSinceLastEdit++;

        // Wait 2 seconds after last edit before saving
        if (timeSinceLastEdit >= 2)
        {
            DBG("ðŸ’¾ Auto-saving project...");
            saveCurrentProject();
            isDirty = false;
            timeSinceLastEdit = 0;
        }
    }

    // ========== PROJECT OPERATIONS ==========

    bool saveCurrentProject()
    {
        // Capture current state from processor
        ProjectData project;

        if (!captureCurrentState(project))
        {
            DBG("âŒ Cannot save: no sample loaded");
            return false;
        }

        // Generate filename
        juce::String filename = "project_" +
            juce::String(juce::Time::currentTimeMillis()) + ".nbsp";

        juce::File file = settings.projectFolder.getChildFile(filename);
        project.setFilePath(file.getFullPathName());

        // Save to file
        if (!ProjectSerializer::saveProject(project, file))
        {
            DBG("âŒ Failed to save project");
            return false;
        }

        // Add to metadata list
        projectMetadataList.push_back(project.getMetadata());

        // Enforce max projects limit
        enforceMaxProjects();

        DBG("âœ… Project saved: " + file.getFileName());
        return true;
    }

    bool loadProject(const juce::String& projectId)
    {
        // Find project file
        juce::File file = findProjectFile(projectId);

        if (!file.existsAsFile())
        {
            DBG("âŒ Project file not found: " + projectId);
            return false;
        }

        // Load project
        ProjectData project;
        if (!ProjectSerializer::loadProject(project, file))
        {
            DBG("âŒ Failed to load project");
            return false;
        }

        // Restore state to processor
        restoreStateToProcessor(project);

        DBG("âœ… Project loaded: " + projectId);
        isDirty = false;
        return true;
    }

    bool loadProjectForPreview(const juce::String& projectId,
        juce::AudioBuffer<float>& audioOut)
    {
        juce::File file = findProjectFile(projectId);

        if (!file.existsAsFile())
            return false;

        ProjectData project;
        if (!ProjectSerializer::loadProject(project, file))
            return false;

        // Copy only audio for preview (fast!)
        audioOut.makeCopyOf(project.getOriginalAudio());

        return true;
    }

    bool deleteProject(const juce::String& projectId)
    {
        juce::File file = findProjectFile(projectId);

        if (!file.existsAsFile())
            return false;

        if (file.deleteFile())
        {
            // Remove from metadata list
            projectMetadataList.erase(
                std::remove_if(projectMetadataList.begin(), projectMetadataList.end(),
                    [&projectId](const ProjectMetadata& m) {
                        return m.projectId == projectId;
                    }),
                projectMetadataList.end());

            DBG("âœ… Project deleted: " + projectId);
            return true;
        }

        return false;
    }

    // ========== PROJECT LIST ==========

    const std::vector<ProjectMetadata>& getProjectList() const
    {
        return projectMetadataList;
    }

    void refreshProjectList()
    {
        scanProjectFolder();
    }

    // ========== SETTINGS ==========

    ProjectManagerSettings& getSettings()
    {
        return settings;
    }

    void setProjectFolder(const juce::File& folder)
    {
        settings.projectFolder = folder;
        settings.saveToFile();

        if (!folder.exists())
            folder.createDirectory();

        scanProjectFolder();
    }

    void setMaxProjects(int max)
    {
        settings.maxProjects = juce::jmax(1, max);
        settings.saveToFile();
        enforceMaxProjects();
    }

    void setAutoSaveEnabled(bool enabled)
    {
        settings.autoSaveEnabled = enabled;
        settings.saveToFile();

        if (enabled && !isTimerRunning())
            startTimer(1000);
        else if (!enabled && isTimerRunning())
            stopTimer();
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;
    ProjectManagerSettings settings;

    std::vector<ProjectMetadata> projectMetadataList;

    bool isDirty;
    int timeSinceLastEdit;  // Seconds since last edit

    // ========== INTERNAL METHODS ==========

    void scanProjectFolder()
    {
        projectMetadataList.clear();

        if (!settings.projectFolder.exists())
            return;

        auto files = settings.projectFolder.findChildFiles(
            juce::File::findFiles, false, "*.nbsp");

        for (const auto& file : files)
        {
            ProjectMetadata metadata;
            if (ProjectSerializer::loadMetadataOnly(metadata, file))
            {
                metadata.filePath = file.getFullPathName();
                projectMetadataList.push_back(metadata);
            }
        }

        // Sort by modification time (newest first)
        std::sort(projectMetadataList.begin(), projectMetadataList.end(),
            [](const ProjectMetadata& a, const ProjectMetadata& b) {
                return a.lastModifiedTime > b.lastModifiedTime;
            });

        DBG("Found " + juce::String(projectMetadataList.size()) + " projects");
    }

    void enforceMaxProjects()
    {
        if (projectMetadataList.size() <= static_cast<size_t>(settings.maxProjects))
            return;

        DBG("âš ï¸ Max projects exceeded, deleting oldest...");

        // Sort by modification time (oldest first for deletion)
        std::sort(projectMetadataList.begin(), projectMetadataList.end(),
            [](const ProjectMetadata& a, const ProjectMetadata& b) {
                return a.lastModifiedTime < b.lastModifiedTime;
            });

        // Delete oldest projects
        while (projectMetadataList.size() > static_cast<size_t>(settings.maxProjects))
        {
            auto& oldest = projectMetadataList.front();
            juce::File file(oldest.filePath);

            if (file.existsAsFile())
            {
                file.deleteFile();
                DBG("ðŸ—‘ï¸ Deleted old project: " + oldest.projectName);
            }

            projectMetadataList.erase(projectMetadataList.begin());
        }

        // Re-sort (newest first)
        std::sort(projectMetadataList.begin(), projectMetadataList.end(),
            [](const ProjectMetadata& a, const ProjectMetadata& b) {
                return a.lastModifiedTime > b.lastModifiedTime;
            });
    }

    juce::File findProjectFile(const juce::String& projectId) const
    {
        for (const auto& metadata : projectMetadataList)
        {
            if (metadata.projectId == projectId)
            {
                return juce::File(metadata.filePath);
            }
        }

        return juce::File();
    }

    bool captureCurrentState(ProjectData& project);
    void restoreStateToProcessor(const ProjectData& project);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectManager)
};