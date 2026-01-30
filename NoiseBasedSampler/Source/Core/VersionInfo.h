/*
  VersionInfo.h - Central Version Management for samp

  This file contains:
  - Current plugin version (synced with version.txt in repo)
  - Version comparison logic
  - Build information

  IMPORTANT: Update this file when releasing new version!
*/

#pragma once
#include <JuceHeader.h>

namespace PluginVersion
{
    //==============================================================================
    // VERSION INFORMATION
    //==============================================================================

    // Current version - UPDATE THIS WHEN RELEASING!
    // Must match version.txt in repository root
    inline constexpr const char* VERSION = "1.0.0";
    inline constexpr int VERSION_MAJOR = 1;
    inline constexpr int VERSION_MINOR = 0;
    inline constexpr int VERSION_PATCH = 0;

    // Plugin metadata
    inline constexpr const char* PLUGIN_NAME = "samp";
    inline constexpr const char* PLUGIN_FULL_NAME = "Index-Based Sampler";
    inline constexpr const char* COMPANY_NAME = "YourCompany";

    // Build information
    inline constexpr const char* BUILD_DATE = __DATE__;
    inline constexpr const char* BUILD_TIME = __TIME__;

    //==============================================================================
    // VERSION UTILITIES
    //==============================================================================

    /**
     * Get version as string (e.g., "1.0.0")
     */
    inline juce::String getVersionString()
    {
        return VERSION;
    }

    /**
     * Get full version info (e.g., "samp v1.0.0")
     */
    inline juce::String getFullVersionString()
    {
        return juce::String(PLUGIN_NAME) + " v" + VERSION;
    }

    /**
     * Get version with build info (e.g., "v1.0.0 (Built: Jan 30 2025)")
     */
    inline juce::String getVersionWithBuildInfo()
    {
        return "v" + juce::String(VERSION) + " (Built: " + BUILD_DATE + ")";
    }

    /**
     * Compare two version strings
     * Returns:
     *   -1 if version1 < version2
     *    0 if version1 == version2
     *    1 if version1 > version2
     */
    inline int compareVersions(const juce::String& version1, const juce::String& version2)
    {
        auto v1 = juce::StringArray::fromTokens(version1, ".", "");
        auto v2 = juce::StringArray::fromTokens(version2, ".", "");

        // Ensure both have 3 components (major.minor.patch)
        while (v1.size() < 3) v1.add("0");
        while (v2.size() < 3) v2.add("0");

        for (int i = 0; i < 3; ++i)
        {
            int num1 = v1[i].getIntValue();
            int num2 = v2[i].getIntValue();

            if (num1 < num2) return -1;
            if (num1 > num2) return 1;
        }

        return 0; // Equal
    }

    /**
     * Check if newVersion is newer than current version
     */
    inline bool isNewerVersion(const juce::String& newVersion)
    {
        return compareVersions(newVersion, VERSION) > 0;
    }

    /**
     * Check if two versions are equal
     */
    inline bool isSameVersion(const juce::String& version)
    {
        return compareVersions(version, VERSION) == 0;
    }

    /**
     * Parse version string into components
     * Returns {major, minor, patch}
     */
    inline std::array<int, 3> parseVersion(const juce::String& version)
    {
        auto parts = juce::StringArray::fromTokens(version, ".", "");

        std::array<int, 3> result = { 0, 0, 0 };

        for (int i = 0; i < juce::jmin(3, parts.size()); ++i)
        {
            result[i] = parts[i].getIntValue();
        }

        return result;
    }

    //==============================================================================
    // DEBUG INFO
    //==============================================================================

    /**
     * Print version info to debug console
     */
    inline void printVersionInfo()
    {
        DBG("===========================================");
        DBG("Plugin: " + juce::String(PLUGIN_FULL_NAME));
        DBG("Version: " + juce::String(VERSION));
        DBG("Build Date: " + juce::String(BUILD_DATE));
        DBG("Build Time: " + juce::String(BUILD_TIME));
        DBG("===========================================");
    }

    //==============================================================================
    // TESTING
    //==============================================================================

    /**
     * Run version comparison tests
     */
    inline void runVersionTests()
    {
        DBG("Running version comparison tests...");

        // Test 1: Same version
        jassert(compareVersions("1.0.0", "1.0.0") == 0);

        // Test 2: Newer patch
        jassert(compareVersions("1.0.1", "1.0.0") == 1);
        jassert(isNewerVersion("1.0.1") == true);

        // Test 3: Newer minor
        jassert(compareVersions("1.1.0", "1.0.0") == 1);
        jassert(isNewerVersion("1.1.0") == true);

        // Test 4: Newer major
        jassert(compareVersions("2.0.0", "1.0.0") == 1);
        jassert(isNewerVersion("2.0.0") == true);

        // Test 5: Older version
        jassert(compareVersions("0.9.0", "1.0.0") == -1);
        jassert(isNewerVersion("0.9.0") == false);

        DBG("? All version tests passed!");
    }
}

//==============================================================================
// USAGE EXAMPLES
//==============================================================================

/*

 // In PluginProcessor constructor:
 PluginVersion::printVersionInfo();

 // In UI:
 versionLabel.setText(PluginVersion::getFullVersionString(), dontSendNotification);

 // When checking for updates:
 juce::String latestVersion = "1.0.1"; // From GitHub API
 if (PluginVersion::isNewerVersion(latestVersion))
 {
     DBG("Update available: " + latestVersion);
 }

 // Compare versions:
 int result = PluginVersion::compareVersions("1.0.1", "1.0.0");
 if (result > 0)
     DBG("First version is newer");

*/