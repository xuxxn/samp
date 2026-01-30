/*
ProjectPanel.h - Project Management UI
Features:
- Project list with waveform thumbnails
- Click = preview audio
- Click + Enter = load full project
- Arrow keys navigation
- Delete key to remove projects
- Settings for folder path and max projects
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ProjectManager.h"
#include <memory>

// ==========================================================================
// COLORS
// ==========================================================================

namespace ProjectPanelColors
{
    const juce::Colour BACKGROUND(0xff2d2d2d);
    const juce::Colour PANEL_BG(0xff374151);
    const juce::Colour SELECTED_BG(0xff4b5563);
    const juce::Colour HOVER_BG = juce::Colour(0xff4b5563).withAlpha(0.5f);
    const juce::Colour ACCENT(0xff8b5cf6);  // Purple
    const juce::Colour BORDER(0xff8b5cf6);
    const juce::Colour TEXT_PRIMARY = juce::Colours::white;
    const juce::Colour TEXT_SECONDARY = juce::Colours::white.withAlpha(0.6f);
}

// ==========================================================================
// PROJECT ROW COMPONENT
// ==========================================================================

class ProjectRowComponent : public juce::Component
{
public:
    ProjectRowComponent(const ProjectMetadata& meta, int index)
        : metadata(meta)
        , rowIndex(index)
        , isSelected(false)
        , isHovered(false)
    {
    }

    void paint(juce::Graphics& g) override
    {
        using namespace ProjectPanelColors;

        auto bounds = getLocalBounds();

        // Background
        if (isSelected)
            g.setColour(SELECTED_BG);
        else if (isHovered)
            g.setColour(HOVER_BG);
        else
            g.setColour(PANEL_BG);

        g.fillRect(bounds);

        // Border
        if (isSelected)
        {
            g.setColour(ACCENT);
            g.drawRect(bounds, 2);
        }
        else
        {
            g.setColour(BORDER.withAlpha(0.3f));
            g.drawRect(bounds, 1);
        }

        // Index number
        auto indexBounds = bounds.removeFromLeft(40);
        g.setColour(TEXT_SECONDARY);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(juce::String(rowIndex + 1), indexBounds,
            juce::Justification::centred);

        bounds.removeFromLeft(5);

        // Waveform thumbnail
        auto waveformBounds = bounds.removeFromLeft(150).reduced(5);
        drawWaveformThumbnail(g, waveformBounds);

        bounds.removeFromLeft(10);

        // Project info
        g.setColour(TEXT_PRIMARY);
        g.setFont(juce::Font(13.0f, juce::Font::bold));

        auto nameBounds = bounds.removeFromTop(20);
        g.drawText(metadata.projectName, nameBounds,
            juce::Justification::left, true);

        g.setColour(TEXT_SECONDARY);
        g.setFont(juce::Font(11.0f));

        auto infoBounds = bounds.removeFromTop(15);
        juce::String info = metadata.getFormattedDuration() + " | " +
            juce::String(metadata.sampleRate / 1000.0, 1) + " kHz";
        g.drawText(info, infoBounds, juce::Justification::left);

        auto dateBounds = bounds.removeFromTop(15);
        g.drawText(metadata.getFormattedDate(), dateBounds,
            juce::Justification::left);
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        isHovered = true;
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        isHovered = false;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown() && onClick)
            onClick();
    }

    void setSelected(bool selected)
    {
        if (isSelected != selected)
        {
            isSelected = selected;
            repaint();
        }
    }

    bool getSelected() const { return isSelected; }

    void setThumbnailData(const std::vector<float>& data)
    {
        thumbnailData = data;
        repaint();
    }

    const ProjectMetadata& getMetadata() const { return metadata; }

    std::function<void()> onClick;

private:
    ProjectMetadata metadata;
    int rowIndex;
    bool isSelected;
    bool isHovered;
    std::vector<float> thumbnailData;

    void drawWaveformThumbnail(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        using namespace ProjectPanelColors;

        // Background
        g.setColour(BACKGROUND);
        g.fillRect(bounds);

        // Border
        g.setColour(BORDER.withAlpha(0.5f));
        g.drawRect(bounds, 1);

        if (thumbnailData.empty())
        {
            // No thumbnail - draw placeholder
            g.setColour(TEXT_SECONDARY);
            g.setFont(juce::Font(10.0f));
            g.drawText("No preview", bounds, juce::Justification::centred);
            return;
        }

        // Draw waveform
        juce::Path waveformPath;
        float width = static_cast<float>(bounds.getWidth());
        float height = static_cast<float>(bounds.getHeight());
        float centerY = bounds.getY() + height * 0.5f;

        int numPoints = static_cast<int>(thumbnailData.size());
        float pointsPerPixel = numPoints / width;

        for (int x = 0; x < bounds.getWidth(); ++x)
        {
            int dataIndex = static_cast<int>(x * pointsPerPixel);

            if (dataIndex >= 0 && dataIndex < numPoints)
            {
                float amplitude = thumbnailData[dataIndex];
                float y1 = centerY - (amplitude * height * 0.4f);
                float y2 = centerY + (amplitude * height * 0.4f);

                if (x == 0)
                    waveformPath.startNewSubPath(bounds.getX() + x, y1);
                else
                    waveformPath.lineTo(bounds.getX() + x, y1);
            }
        }

        // Draw bottom half (mirrored)
        for (int x = bounds.getWidth() - 1; x >= 0; --x)
        {
            int dataIndex = static_cast<int>(x * pointsPerPixel);

            if (dataIndex >= 0 && dataIndex < numPoints)
            {
                float amplitude = thumbnailData[dataIndex];
                float y2 = centerY + (amplitude * height * 0.4f);
                waveformPath.lineTo(bounds.getX() + x, y2);
            }
        }

        waveformPath.closeSubPath();

        g.setColour(ACCENT);
        g.fillPath(waveformPath);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectRowComponent)
};

// ==========================================================================
// SETTINGS SECTION
// ==========================================================================

class ProjectSettingsSection : public juce::Component,
    public juce::Button::Listener
{
public:
    ProjectSettingsSection(ProjectManager& mgr)
        : projectManager(mgr)
    {
        // Folder path
        addAndMakeVisible(folderLabel);
        folderLabel.setText("Project Folder:", juce::dontSendNotification);
        folderLabel.setColour(juce::Label::textColourId,
            ProjectPanelColors::TEXT_PRIMARY);

        addAndMakeVisible(folderPathLabel);
        folderPathLabel.setText(
            projectManager.getSettings().projectFolder.getFullPathName(),
            juce::dontSendNotification);
        folderPathLabel.setColour(juce::Label::textColourId,
            ProjectPanelColors::TEXT_SECONDARY);
        folderPathLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(browseFolderButton);
        browseFolderButton.setButtonText("Browse...");
        browseFolderButton.addListener(this);

        // Max projects
        addAndMakeVisible(maxProjectsLabel);
        maxProjectsLabel.setText("Max Projects:", juce::dontSendNotification);
        maxProjectsLabel.setColour(juce::Label::textColourId,
            ProjectPanelColors::TEXT_PRIMARY);

        addAndMakeVisible(maxProjectsSlider);
        maxProjectsSlider.setRange(5, 50, 1);
        maxProjectsSlider.setValue(projectManager.getSettings().maxProjects);
        maxProjectsSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
        maxProjectsSlider.onValueChange = [this] {
            projectManager.setMaxProjects(
                static_cast<int>(maxProjectsSlider.getValue()));
            };
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(ProjectPanelColors::PANEL_BG);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        // Folder path row
        auto folderRow = bounds.removeFromTop(30);
        folderLabel.setBounds(folderRow.removeFromLeft(100));
        browseFolderButton.setBounds(folderRow.removeFromRight(80));
        folderRow.removeFromRight(5);
        folderPathLabel.setBounds(folderRow);

        bounds.removeFromTop(5);

        // Max projects row
        auto maxRow = bounds.removeFromTop(30);
        maxProjectsLabel.setBounds(maxRow.removeFromLeft(100));
        maxProjectsSlider.setBounds(maxRow.removeFromLeft(150));
    }

    void buttonClicked(juce::Button* button) override
    {
        if (button == &browseFolderButton)
        {
            browseForFolder();
        }
    }

private:
    ProjectManager& projectManager;

    juce::Label folderLabel;
    juce::Label folderPathLabel;
    juce::TextButton browseFolderButton;
    juce::Label maxProjectsLabel;
    juce::Slider maxProjectsSlider;

    void browseForFolder()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Project Folder",
            projectManager.getSettings().projectFolder);

        auto folderChooserFlags = juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectDirectories;

        chooser->launchAsync(folderChooserFlags, [this, chooser](const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder.exists())
                {
                    projectManager.setProjectFolder(folder);
                    folderPathLabel.setText(folder.getFullPathName(),
                        juce::dontSendNotification);
                }
            });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectSettingsSection)
};

// ==========================================================================
// PROJECT PANEL
// ==========================================================================

class ProjectPanel : public juce::Component,
    public juce::Timer
{
public:
    ProjectPanel(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
        , projectManager(*processor.getProjectManager())
        , selectedProjectIndex(-1)
    {
        // Settings section
        settingsSection = std::make_unique<ProjectSettingsSection>(projectManager);
        addAndMakeVisible(settingsSection.get());

        // Viewport for project list
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&projectListComponent, false);
        viewport.setScrollBarsShown(true, false);

        // Refresh project list
        refreshProjectList();

        // Start timer for periodic refresh
        startTimerHz(2);  // Refresh UI at 2 Hz

        setWantsKeyboardFocus(true);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(ProjectPanelColors::BACKGROUND);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Settings section at top
        settingsSection->setBounds(bounds.removeFromTop(80));

        // Viewport for project list
        viewport.setBounds(bounds);
        layoutProjectList();
    }

    void timerCallback() override
    {
        // Periodic refresh to catch new auto-saved projects
        static int counter = 0;
        if (++counter >= 10)  // Every 5 seconds
        {
            counter = 0;
            refreshProjectList();
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::upKey)
        {
            selectPreviousProject();
            return true;
        }
        else if (key == juce::KeyPress::downKey)
        {
            selectNextProject();
            return true;
        }
        else if (key == juce::KeyPress::returnKey)
        {
            loadSelectedProject();
            return true;
        }
        else if (key == juce::KeyPress::deleteKey)
        {
            // deleteSelectedProject();
            return true;
        }

        return false;
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;
    ProjectManager& projectManager;

    std::unique_ptr<ProjectSettingsSection> settingsSection;
    juce::Viewport viewport;
    juce::Component projectListComponent;

    std::vector<std::unique_ptr<ProjectRowComponent>> projectRows;
    int selectedProjectIndex;

    void refreshProjectList()
    {
        projectManager.refreshProjectList();

        const auto& projects = projectManager.getProjectList();

        // Clear old rows
        projectRows.clear();

        // Create new rows
        int index = 0;
        for (const auto& project : projects)
        {
            auto row = std::make_unique<ProjectRowComponent>(project, index);

            row->onClick = [this, index] {
                selectProject(index);
                previewSelectedProject();
                };

            // Load thumbnail asynchronously
            loadThumbnailForRow(row.get(), project.projectId);

            projectListComponent.addAndMakeVisible(row.get());
            projectRows.push_back(std::move(row));

            index++;
        }

        layoutProjectList();
        repaint();
    }

    void layoutProjectList()
    {
        int yPos = 0;
        int rowHeight = 70;
        int rowWidth = viewport.getWidth() - viewport.getScrollBarThickness();

        for (auto& row : projectRows)
        {
            row->setBounds(0, yPos, rowWidth, rowHeight);
            yPos += rowHeight + 2;
        }

        projectListComponent.setSize(rowWidth, yPos);
    }

    void loadThumbnailForRow(ProjectRowComponent* row,
        const juce::String& projectId)
    {
        // Load full project to get thumbnail
        juce::File file;
        const auto& projects = projectManager.getProjectList();

        for (const auto& p : projects)
        {
            if (p.projectId == projectId)
            {
                file = juce::File(p.filePath);
                break;
            }
        }

        if (!file.existsAsFile())
            return;

        ProjectData project;
        if (ProjectSerializer::loadProject(project, file))
        {
            row->setThumbnailData(project.getThumbnailData());
        }
    }

    void selectProject(int index)
    {
        if (selectedProjectIndex >= 0 &&
            selectedProjectIndex < static_cast<int>(projectRows.size()))
        {
            projectRows[selectedProjectIndex]->setSelected(false);
        }

        selectedProjectIndex = index;

        if (selectedProjectIndex >= 0 &&
            selectedProjectIndex < static_cast<int>(projectRows.size()))
        {
            projectRows[selectedProjectIndex]->setSelected(true);

            // Scroll to make selected visible
            int rowHeight = 72;
            int yPos = selectedProjectIndex * rowHeight;
            viewport.setViewPosition(0, yPos - viewport.getHeight() / 2);
        }

        repaint();
    }

    void selectPreviousProject()
    {
        if (selectedProjectIndex > 0)
        {
            selectProject(selectedProjectIndex - 1);
        }
    }

    void selectNextProject()
    {
        if (selectedProjectIndex < static_cast<int>(projectRows.size()) - 1)
        {
            selectProject(selectedProjectIndex + 1);
        }
    }

    void previewSelectedProject()
    {
        if (selectedProjectIndex < 0 ||
            selectedProjectIndex >= static_cast<int>(projectRows.size()))
            return;

        const auto& metadata = projectRows[selectedProjectIndex]->getMetadata();

        juce::AudioBuffer<float> previewAudio;
        if (projectManager.loadProjectForPreview(metadata.projectId, previewAudio))
        {
            processor.setPreviewAudio(previewAudio);
            processor.triggerSample();

            DBG("▶️ Preview: " + metadata.projectName);
        }
    }

    void loadSelectedProject()
    {
        if (selectedProjectIndex < 0 ||
            selectedProjectIndex >= static_cast<int>(projectRows.size()))
            return;

        const auto& metadata = projectRows[selectedProjectIndex]->getMetadata();

        if (projectManager.loadProject(metadata.projectId))
        {
            DBG("✅ Loaded: " + metadata.projectName);

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Project Loaded",
                "Successfully loaded: " + metadata.projectName,
                "OK");
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Load Failed",
                "Could not load project: " + metadata.projectName,
                "OK");
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectPanel)
};