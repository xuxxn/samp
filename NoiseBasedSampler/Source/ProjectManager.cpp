/*
ProjectManager.cpp - Implementation
Handles capturing current processor state and restoring it from saved projects.
*/

#include "ProjectManager.h"
#include "PluginProcessor.h"

// ==========================================================================
// CAPTURE CURRENT STATE FROM PROCESSOR
// ==========================================================================

bool ProjectManager::captureCurrentState(ProjectData& project)
{
    // Check if sample is loaded
    if (!processor.hasSampleLoaded())
    {
        return false;
    }

    const juce::ScopedLock sl(processor.getSampleLock());

    try
    {
        // Get current sample name (or use default)
        juce::String sampleName = "Project";
        if (processor.getCurrentSampleName().isNotEmpty())
            sampleName = processor.getCurrentSampleName();

        // Set metadata
        const auto& originalAudio = processor.getOriginalSample();
        project.setMetadata(
            sampleName,
            processor.getCurrentSampleRate(),
            originalAudio.getNumSamples(),
            originalAudio.getNumChannels()
        );

        // Capture audio buffer
        project.setOriginalAudio(originalAudio);

        // Capture feature data
        project.setFeatureData(processor.getFeatureData());

        // Capture effect state
        EffectStateSnapshot effectState;
        const auto& effectMgr = processor.getEffectStateManager();

        effectState.trimActive = effectMgr.isTrimActive();
        effectState.trimStart = effectMgr.getTrimStart();
        effectState.trimEnd = effectMgr.getTrimEnd();

        effectState.normalizeActive = effectMgr.isNormalizeActive();
        effectState.normalizeTargetDb = effectMgr.getNormalizeTargetDb();
        effectState.normalizeGain = effectMgr.getNormalizeGain();

        effectState.reverseActive = effectMgr.isReverseActive();

        effectState.boostActive = effectMgr.isBoostActive();
        effectState.boostDb = effectMgr.getBoostDb();
        effectState.boostGain = effectMgr.getBoostGain();

        effectState.adsrCutItselfMode = effectMgr.isAdsrCutItselfMode();

        project.setEffectState(effectState);

        // Capture audio state XML
        // Note: AudioStateManager doesn't have toXml/fromXml methods
        // If needed, add these methods to AudioStateManager.h
        // auto& audioStateMgr = processor.getAudioStateManager();
        // project.setAudioStateXml(
        //     std::unique_ptr<juce::XmlElement>(audioStateMgr.toXml())
        // );

        // Capture UI state XML
        auto& uiStateMgr = processor.getUIStateManager();
        project.setUIStateXml(
            std::unique_ptr<juce::XmlElement>(uiStateMgr.toXml())
        );

        DBG("âœ… State captured successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        DBG("âŒ Exception during state capture: " + juce::String(e.what()));
        return false;
    }
}

// ==========================================================================
// RESTORE STATE TO PROCESSOR
// ==========================================================================

void ProjectManager::restoreStateToProcessor(const ProjectData& project)
{
    const juce::ScopedLock sl(processor.getSampleLock());

    try
    {
        DBG("===========================================");
        DBG("ðŸ”„ RESTORING PROJECT STATE");
        DBG("===========================================");

        // Set current sample name
        processor.setCurrentSampleName(project.getMetadata().projectName);

        // Restore audio buffer
        const auto& audio = project.getOriginalAudio();
        processor.setOriginalSample(audio);

        DBG("âœ… Audio restored: " + juce::String(audio.getNumSamples()) + " samples");

        // Restore feature data
        processor.setFeatureData(project.getFeatureData());
        DBG("âœ… Features restored: " + juce::String(project.getFeatureData().getNumSamples()));

        // Restore effect state
        const auto& effectState = project.getEffectState();
        auto& effectMgr = processor.getEffectStateManager();

        // Restore effects one by one
        effectMgr.setTrimActive(
            effectState.trimActive,
            effectState.trimStart,
            effectState.trimEnd
        );

        effectMgr.setNormalizeActive(
            effectState.normalizeActive,
            effectState.normalizeTargetDb,
            effectState.normalizeGain
        );

        effectMgr.setReverseActive(effectState.reverseActive);

        effectMgr.setBoostActive(
            effectState.boostActive,
            effectState.boostDb,
            effectState.boostGain
        );

        effectMgr.setAdsrCutItselfMode(effectState.adsrCutItselfMode);

        DBG("âœ… Effects restored");

        // Restore audio state
        // Note: AudioStateManager doesn't have toXml/fromXml methods
        // if (project.getAudioStateXml() != nullptr)
        // {
        //     processor.getAudioStateManager().fromXml(project.getAudioStateXml());
        //     DBG("âœ… Audio state restored");
        // }

        // Restore UI state
        if (project.getUIStateXml() != nullptr)
        {
            processor.getUIStateManager().fromXml(project.getUIStateXml());
            DBG("âœ… UI state restored");
        }

        // Apply all changes
        processor.applyFeatureChangesToSample();

        // Update sample player - get output buffer directly
        // Note: Using the internal outputBuffer after applying features
        processor.setSampleForPlayback(processor.getOriginalSample());

        DBG("===========================================");
        DBG("âœ… PROJECT FULLY RESTORED");
        DBG("===========================================");
    }
    catch (const std::exception& e)
    {
        DBG("âŒ Exception during state restore: " + juce::String(e.what()));
    }
}