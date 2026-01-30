#pragma once
#include <JuceHeader.h>

namespace PluginVersion
{
    inline constexpr const char* VERSION = "1.0.0";
    inline constexpr int VERSION_MAJOR = 1;
    inline constexpr int VERSION_MINOR = 0;
    inline constexpr int VERSION_PATCH = 0;
    
    inline juce::String getVersionString() 
    { 
        return VERSION; 
    }
    
    inline bool isNewerVersion(const juce::String& newVersion)
    {
        // Compare versions (1.2.3 vs 1.2.4)
        auto current = juce::StringArray::fromTokens(VERSION, ".", "");
        auto incoming = juce::StringArray::fromTokens(newVersion, ".", "");
        
        for (int i = 0; i < 3; ++i)
        {
            int currentNum = current[i].getIntValue();
            int incomingNum = incoming[i].getIntValue();
            
            if (incomingNum > currentNum) return true;
            if (incomingNum < currentNum) return false;
        }
        
        return false; // Same version
    }
}