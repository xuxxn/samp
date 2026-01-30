/*
  UpdaterLauncher.h - Interface to launch standalone Updater application

  This class handles:
  - Checking if Updater is installed
  - Launching Updater with different parameters
  - Fallback to download page if Updater not found
*/

#pragma once
#include <JuceHeader.h>

class UpdaterLauncher
{
public:
    //==============================================================================
    // UPDATER DETECTION
    //==============================================================================

    /**
     * Check if Updater application is installed
     * Returns true if Updater.exe exists in expected location
     */
    static bool isUpdaterInstalled()
    {
        auto updaterFile = getUpdaterExecutable();
        return updaterFile.existsAsFile();
    }

    /**
     * Get path to Updater executable
     * Windows: %LOCALAPPDATA%\YourCompany\Updater\Updater.exe
     * macOS: ~/Library/Application Support/YourCompany/Updater.app
     */
    static juce::File getUpdaterExecutable()
    {
#if JUCE_WINDOWS
        auto localAppData = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);

        return localAppData
            .getChildFile("YourCompany")
            .getChildFile("Updater")
            .getChildFile("Updater.exe");

#elif JUCE_MAC
        auto appSupport = juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);

        return appSupport
            .getChildFile("YourCompany")
            .getChildFile("Updater.app");

#else
        return juce::File();
#endif
    }

    //==============================================================================
    // LAUNCH UPDATER
    //==============================================================================

    /**
     * Launch Updater application with optional arguments
     *
     * Arguments:
     *   --check-now    : Immediately check for updates
     *   --silent       : Run in background (tray only)
     *   --install-now  : Install pending update if available
     *
     * Returns true if launched successfully
     */
    static bool launchUpdater(const juce::String& args = "")
    {
        auto updaterFile = getUpdaterExecutable();

        if (!updaterFile.existsAsFile())
        {
            DBG("? Updater not found at: " + updaterFile.getFullPathName());
            return false;
        }

        DBG("?? Launching Updater: " + updaterFile.getFullPathName());

#if JUCE_WINDOWS
        // Launch with arguments
        bool success = updaterFile.startAsProcess(args);

#elif JUCE_MAC
        // On macOS, use 'open' command
        juce::String command = "open \"" + updaterFile.getFullPathName() + "\"";
        if (args.isNotEmpty())
            command += " --args " + args;

        bool success = (std::system(command.toRawUTF8()) == 0);

#else
        bool success = false;
#endif

        if (success)
        {
            DBG("? Updater launched successfully");
        }
        else
        {
            DBG("? Failed to launch Updater");
        }

        return success;
    }

    /**
     * Check for updates (launches Updater with --check-now)
     * Returns true if Updater was launched successfully
     */
    static bool checkForUpdates()
    {
        if (isUpdaterInstalled())
        {
            return launchUpdater("--check-now");
        }
        else
        {
            DBG("?? Updater not installed, opening download page");
            openDownloadPage();
            return false;
        }
    }

    /**
     * Open Updater window (no arguments)
     */
    static void openUpdaterWindow()
    {
        if (isUpdaterInstalled())
        {
            launchUpdater("");
        }
        else
        {
            openDownloadPage();
        }
    }

    //==============================================================================
    // FALLBACK - DOWNLOAD PAGE
    //==============================================================================

    /**
     * Open download page in browser if Updater not found
     * This is fallback when Updater is not installed
     */
    static void openDownloadPage()
    {
        // GitHub Releases page
        juce::String downloadUrl = "https://github.com/xuxxn/samp/releases/latest";

        DBG("?? Opening download page: " + downloadUrl);

        juce::URL url(downloadUrl);
        url.launchInDefaultBrowser();
    }

    //==============================================================================
    // UTILITY METHODS
    //==============================================================================

    /**
     * Get human-readable status of Updater installation
     */
    static juce::String getUpdaterStatus()
    {
        if (isUpdaterInstalled())
        {
            return "Updater installed at: " + getUpdaterExecutable().getFullPathName();
        }
        else
        {
            return "Updater not found. Please reinstall samp.";
        }
    }

    /**
     * Show alert if Updater not found
     * Returns true if user wants to download
     */
    static bool showUpdaterNotFoundDialog()
    {
        // Use async version for JUCE compatibility
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::WarningIcon)
            .withTitle("Updater Not Found")
            .withMessage("The samp Updater application was not found.\n\n"
                "Would you like to download the latest version from GitHub?")
            .withButton("Yes, Open Browser")
            .withButton("No, Cancel"),
            [](int result)
            {
                if (result == 1) // Yes button
                {
                    openDownloadPage();
                }
            }
        );

        return true; // Always return true since we're using async
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdaterLauncher)
};