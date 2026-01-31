/*
  GitHubAPI.h - GitHub Releases API Integration
  
  Handles:
  - Checking for latest release
  - Parsing release information
  - Downloading release files
*/

#pragma once
#include <juce_core/juce_core.h>
#include "../Config.h"

class GitHubAPI
{
public:
    //==========================================================================
    // RELEASE INFORMATION
    //==========================================================================
    
    struct ReleaseInfo
    {
        juce::String version;        // e.g., "1.0.1" (without 'v')
        juce::String tagName;        // e.g., "v1.0.1"
        juce::String downloadUrl;    // Direct download URL for .vst3 file
        juce::String changelog;      // Release notes/body
        juce::Time releaseDate;      // When released
        bool isPrerelease;           // Is it a beta/prerelease
        juce::int64 fileSize;        // Size in bytes
        
        bool isValid() const 
        { 
            return version.isNotEmpty() && downloadUrl.isNotEmpty(); 
        }
        
        juce::String getFileSizeString() const
        {
            double mb = fileSize / (1024.0 * 1024.0);
            return juce::String(mb, 1) + " MB";
        }
    };
    
    //==========================================================================
    // PUBLIC API
    //==========================================================================
    
    /**
     * Check for latest release (synchronous)
     * Returns release info or invalid struct if failed
     */
    static ReleaseInfo getLatestRelease(bool includePrereleases = false)
    {
        UpdaterConfig::logMessage("Checking for latest release...");
        
        juce::String apiUrl = UpdaterConfig::getGitHubAPIUrl();
        juce::URL url(apiUrl);
        
        // Make request (JUCE 8.x simplified API)
        auto response = url.readEntireTextStream(false);
        
        if (response.isEmpty())
        {
            UpdaterConfig::logMessage("ERROR: Empty response from GitHub API");
            return ReleaseInfo();
        }
        
        UpdaterConfig::logMessage("Response received, parsing JSON...");
        
        // Parse JSON
        auto json = juce::JSON::parse(response);
        
        if (json.isVoid())
        {
            UpdaterConfig::logMessage("ERROR: Failed to parse JSON response");
            return ReleaseInfo();
        }
        
        return parseReleaseInfo(json, includePrereleases);
    }
    
    /**
     * Check for latest release (asynchronous with callback)
     */
    static void getLatestReleaseAsync(
        std::function<void(ReleaseInfo)> callback,
        bool includePrereleases = false)
    {
        juce::Thread::launch([callback, includePrereleases]()
        {
            auto info = getLatestRelease(includePrereleases);
            
            // Call callback on message thread
            juce::MessageManager::callAsync([callback, info]()
            {
                callback(info);
            });
        });
    }
    
    /**
     * Download file from URL with progress callback
     * 
     * progressCallback: void(float progress, int bytesDownloaded, int totalBytes)
     * Returns: true if successful
     */
    static bool downloadFile(
        const juce::String& url,
        const juce::File& destination,
        std::function<void(float, int, int)> progressCallback = nullptr)
    {
        UpdaterConfig::logMessage("Downloading: " + url);
        UpdaterConfig::logMessage("To: " + destination.getFullPathName());
        
        juce::URL downloadUrl(url);
        
        // Create output stream
        auto outputStream = destination.createOutputStream();
        
        if (!outputStream)
        {
            UpdaterConfig::logMessage("ERROR: Failed to create output stream");
            return false;
        }
        
        // Create input stream with progress callback (JUCE 8.x uses int, int)
        auto inputStream = downloadUrl.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withProgressCallback([progressCallback](int bytesDownloaded, int totalBytes)
                {
                    if (progressCallback && totalBytes > 0)
                    {
                        float progress = (float)bytesDownloaded / (float)totalBytes;
                        progressCallback(progress, bytesDownloaded, totalBytes);
                    }
                    return true; // Continue download
                })
        );
        
        if (!inputStream)
        {
            UpdaterConfig::logMessage("ERROR: Failed to create input stream");
            return false;
        }
        
        // Download
        auto bytesWritten = outputStream->writeFromInputStream(*inputStream, -1);
        
        if (bytesWritten > 0)
        {
            UpdaterConfig::logMessage("Download complete: " + 
                                    juce::String(bytesWritten) + " bytes");
            return true;
        }
        else
        {
            UpdaterConfig::logMessage("ERROR: Download failed, 0 bytes written");
            return false;
        }
    }
    
private:
    //==========================================================================
    // PARSING
    //==========================================================================
    
    static ReleaseInfo parseReleaseInfo(const juce::var& json, bool includePrereleases)
    {
        ReleaseInfo info;
        
        if (auto* obj = json.getDynamicObject())
        {
            // Check if prerelease
            info.isPrerelease = obj->getProperty("prerelease");
            
            if (info.isPrerelease && !includePrereleases)
            {
                UpdaterConfig::logMessage("Skipping prerelease");
                return ReleaseInfo();
            }
            
            // Get version
            info.tagName = obj->getProperty("tag_name").toString();
            info.version = info.tagName.trimCharactersAtStart("v");
            
            // Get changelog
            info.changelog = obj->getProperty("body").toString();
            
            // Get release date
            juce::String dateStr = obj->getProperty("published_at").toString();
            info.releaseDate = juce::Time::fromISO8601(dateStr);
            
            // Find .vst3 asset in assets array
            auto assets = obj->getProperty("assets");
            
            if (auto* arr = assets.getArray())
            {
                for (auto& asset : *arr)
                {
                    if (auto* assetObj = asset.getDynamicObject())
                    {
                        juce::String name = assetObj->getProperty("name").toString();
                        
                        // Look for .vst3 or .vst3.zip file
                        if (name.endsWithIgnoreCase(".vst3") || 
                            name.endsWithIgnoreCase(".vst3.zip") ||
                            name.containsIgnoreCase("samp"))
                        {
                            info.downloadUrl = assetObj->getProperty("browser_download_url").toString();
                            info.fileSize = assetObj->getProperty("size");
                            
                            UpdaterConfig::logMessage("Found asset: " + name);
                            UpdaterConfig::logMessage("URL: " + info.downloadUrl);
                            UpdaterConfig::logMessage("Size: " + info.getFileSizeString());
                            
                            break;
                        }
                    }
                }
            }
            
            if (info.downloadUrl.isEmpty())
            {
                UpdaterConfig::logMessage("WARNING: No .vst3 asset found in release");
            }
            
            UpdaterConfig::logMessage("Parsed release: " + info.version + 
                                    (info.isPrerelease ? " (prerelease)" : ""));
        }
        
        return info;
    }
};