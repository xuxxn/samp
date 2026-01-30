/*
AlgorithmLibrary.h
Manages a collection of saved AlgorithmDNA instances.

Features:
- Add/remove algorithms
- Save/load library to disk
- Search and filter
- Export/import individual algorithms
*/

#pragma once
#include <JuceHeader.h>
#include "AlgorithmDNA.h"
#include <vector>
#include <memory>

// ==========================================================================
// ALGORITHM LIBRARY
// ==========================================================================

class AlgorithmLibrary
{
public:
    AlgorithmLibrary() = default;
    
    // ==========================================================================
    // ALGORITHM MANAGEMENT
    // ==========================================================================
    
    bool addAlgorithm(const AlgorithmDNA& algo)
    {
        if (!algo.isValid())
        {
            DBG("❌ Cannot add invalid algorithm");
            return false;
        }
        
        // Check for duplicate names
        for (const auto& existing : algorithms)
        {
            if (existing.metadata.name == algo.metadata.name)
            {
                DBG("⚠️ Algorithm with this name already exists");
                return false;
            }
        }
        
        algorithms.push_back(algo);
        
        DBG("✅ Added algorithm: " + algo.metadata.name);
        DBG("   Total algorithms: " + juce::String(algorithms.size()));
        
        return true;
    }
    
    bool removeAlgorithm(const juce::String& name)
    {
        auto it = std::find_if(algorithms.begin(), algorithms.end(),
            [&name](const AlgorithmDNA& algo) {
                return algo.metadata.name == name;
            });
        
        if (it == algorithms.end())
        {
            DBG("❌ Algorithm not found: " + name);
            return false;
        }
        
        algorithms.erase(it);
        DBG("🗑️ Removed algorithm: " + name);
        
        return true;
    }
    
    AlgorithmDNA* getAlgorithm(const juce::String& name)
    {
        for (auto& algo : algorithms)
        {
            if (algo.metadata.name == name)
                return &algo;
        }
        
        return nullptr;
    }
    
    const AlgorithmDNA* getAlgorithm(const juce::String& name) const
    {
        for (const auto& algo : algorithms)
        {
            if (algo.metadata.name == name)
                return &algo;
        }
        
        return nullptr;
    }
    
    AlgorithmDNA* getAlgorithmByIndex(int index)
    {
        if (index >= 0 && index < static_cast<int>(algorithms.size()))
            return &algorithms[index];
        
        return nullptr;
    }
    
    int getNumAlgorithms() const
    {
        return static_cast<int>(algorithms.size());
    }
    
    const std::vector<AlgorithmDNA>& getAllAlgorithms() const
    {
        return algorithms;
    }
    
    void clear()
    {
        algorithms.clear();
        DBG("🗑️ Library cleared");
    }
    
    // ==========================================================================
    // SEARCH & FILTER
    // ==========================================================================
    
    std::vector<AlgorithmDNA*> searchByType(const juce::String& type)
    {
        std::vector<AlgorithmDNA*> results;
        
        for (auto& algo : algorithms)
        {
            if (algo.metadata.algorithmType == type)
                results.push_back(&algo);
        }
        
        return results;
    }
    
    std::vector<AlgorithmDNA*> searchByAuthor(const juce::String& author)
    {
        std::vector<AlgorithmDNA*> results;
        
        for (auto& algo : algorithms)
        {
            if (algo.metadata.author == author)
                results.push_back(&algo);
        }
        
        return results;
    }
    
    std::vector<AlgorithmDNA*> searchByName(const juce::String& searchTerm)
    {
        std::vector<AlgorithmDNA*> results;
        
        for (auto& algo : algorithms)
        {
            if (algo.metadata.name.containsIgnoreCase(searchTerm))
                results.push_back(&algo);
        }
        
        return results;
    }
    
    // ==========================================================================
    // SAVE/LOAD LIBRARY
    // ==========================================================================
    
    bool saveLibrary(const juce::File& file)
    {
        DBG("===========================================");
        DBG("💾 SAVING ALGORITHM LIBRARY");
        DBG("===========================================");
        DBG("File: " + file.getFullPathName());
        DBG("Algorithms: " + juce::String(algorithms.size()));
        
        // Create library JSON
        auto* libraryJson = new juce::DynamicObject();
        libraryJson->setProperty("version", "1.0");
        libraryJson->setProperty("type", "algorithm_library");
        libraryJson->setProperty("count", static_cast<int>(algorithms.size()));
        libraryJson->setProperty("creationDate", juce::Time::getCurrentTime().toISO8601(true));
        
        // Add algorithms array
        juce::Array<juce::var> algosArray;
        
        for (const auto& algo : algorithms)
        {
            algosArray.add(algo.toJSON());
        }
        
        libraryJson->setProperty("algorithms", algosArray);
        
        // Save JSON metadata
        juce::var jsonVar(libraryJson);
        juce::String jsonString = juce::JSON::toString(jsonVar, true);
        
        bool success = file.replaceWithText(jsonString);
        
        if (success)
        {
            DBG("✅ Library metadata saved");
            
            // Save binary data for each algorithm
            auto parentDir = file.getParentDirectory();
            auto dataDir = parentDir.getChildFile(file.getFileNameWithoutExtension() + "_data");
            dataDir.createDirectory();
            
            for (size_t i = 0; i < algorithms.size(); ++i)
            {
                juce::String binaryFileName = "algo_" + juce::String(i) + ".bin";
                auto binaryFile = dataDir.getChildFile(binaryFileName);
                
                if (!algorithms[i].saveBinaryData(binaryFile))
                {
                    DBG("⚠️ Failed to save binary data for: " + algorithms[i].metadata.name);
                }
            }
            
            DBG("✅ All binary data saved");
        }
        
        DBG("===========================================");
        
        return success;
    }
    
    bool loadLibrary(const juce::File& file)
    {
        if (!file.existsAsFile())
        {
            DBG("❌ Library file not found: " + file.getFullPathName());
            return false;
        }
        
        DBG("===========================================");
        DBG("📂 LOADING ALGORITHM LIBRARY");
        DBG("===========================================");
        DBG("File: " + file.getFullPathName());
        
        // Load JSON metadata
        juce::String jsonString = file.loadFileAsString();
        juce::var jsonVar = juce::JSON::parse(jsonString);
        
        if (!jsonVar.isObject())
        {
            DBG("❌ Invalid JSON format");
            return false;
        }
        
        algorithms.clear();
        
        auto algosArray = jsonVar.getProperty("algorithms", juce::var());
        
        if (!algosArray.isArray())
        {
            DBG("❌ No algorithms array found");
            return false;
        }
        
        auto* array = algosArray.getArray();
        
        DBG("Found " + juce::String(array->size()) + " algorithms");
        
        // Load binary data directory
        auto parentDir = file.getParentDirectory();
        auto dataDir = parentDir.getChildFile(file.getFileNameWithoutExtension() + "_data");
        
        for (int i = 0; i < array->size(); ++i)
        {
            AlgorithmDNA algo;
            algo.fromJSON(array->getUnchecked(i));
            
            // Load binary data
            juce::String binaryFileName = "algo_" + juce::String(i) + ".bin";
            auto binaryFile = dataDir.getChildFile(binaryFileName);
            
            if (binaryFile.existsAsFile())
            {
                if (!algo.loadBinaryData(binaryFile))
                {
                    DBG("⚠️ Failed to load binary data for: " + algo.metadata.name);
                    continue;
                }
            }
            else
            {
                DBG("⚠️ Binary data file not found for: " + algo.metadata.name);
            }
            
            if (algo.isValid())
            {
                algorithms.push_back(algo);
                DBG("  ✅ Loaded: " + algo.metadata.name);
            }
            else
            {
                DBG("  ❌ Invalid algorithm data");
            }
        }
        
        DBG("===========================================");
        DBG("✅ Library loaded: " + juce::String(algorithms.size()) + " algorithms");
        DBG("===========================================");
        
        return true;
    }
    
    // ==========================================================================
    // EXPORT/IMPORT INDIVIDUAL ALGORITHMS
    // ==========================================================================
    
    bool exportAlgorithm(const juce::String& name, const juce::File& exportFile)
    {
        auto* algo = getAlgorithm(name);
        
        if (!algo)
        {
            DBG("❌ Algorithm not found: " + name);
            return false;
        }
        
        DBG("📤 Exporting algorithm: " + name);
        
        // Save JSON metadata
        juce::var jsonVar = algo->toJSON();
        juce::String jsonString = juce::JSON::toString(jsonVar, true);
        
        bool success = exportFile.replaceWithText(jsonString);
        
        if (success)
        {
            // Save binary data alongside
            auto binaryFile = exportFile.withFileExtension(".bin");
            algo->saveBinaryData(binaryFile);
            
            DBG("✅ Algorithm exported to: " + exportFile.getFullPathName());
        }
        
        return success;
    }
    
    bool importAlgorithm(const juce::File& importFile)
    {
        if (!importFile.existsAsFile())
        {
            DBG("❌ File not found: " + importFile.getFullPathName());
            return false;
        }
        
        DBG("📥 Importing algorithm from: " + importFile.getFullPathName());
        
        AlgorithmDNA algo;
        
        // Load JSON metadata
        juce::String jsonString = importFile.loadFileAsString();
        juce::var jsonVar = juce::JSON::parse(jsonString);
        
        algo.fromJSON(jsonVar);
        
        // Load binary data
        auto binaryFile = importFile.withFileExtension(".bin");
        
        if (binaryFile.existsAsFile())
        {
            algo.loadBinaryData(binaryFile);
        }
        else
        {
            DBG("⚠️ Binary data file not found");
        }
        
        if (!algo.isValid())
        {
            DBG("❌ Invalid algorithm data");
            return false;
        }
        
        // Check for name conflicts
        if (getAlgorithm(algo.metadata.name) != nullptr)
        {
            // Auto-rename
            juce::String baseName = algo.metadata.name;
            int counter = 1;
            
            while (getAlgorithm(algo.metadata.name) != nullptr)
            {
                algo.metadata.name = baseName + " (" + juce::String(counter) + ")";
                counter++;
            }
            
            DBG("⚠️ Name conflict - renamed to: " + algo.metadata.name);
        }
        
        return addAlgorithm(algo);
    }
    
    // ==========================================================================
    // STATISTICS
    // ==========================================================================
    
    struct LibraryStats
    {
        int totalAlgorithms = 0;
        int differenceAlgorithms = 0;
        int morphAlgorithms = 0;
        int otherAlgorithms = 0;
        
        std::vector<juce::String> uniqueAuthors;
        
        juce::Time oldestCreationDate;
        juce::Time newestCreationDate;
    };
    
    LibraryStats getStatistics() const
    {
        LibraryStats stats;
        stats.totalAlgorithms = static_cast<int>(algorithms.size());
        
        if (algorithms.empty())
            return stats;
        
        std::set<juce::String> authors;
        
        stats.oldestCreationDate = algorithms[0].metadata.creationDate;
        stats.newestCreationDate = algorithms[0].metadata.creationDate;
        
        for (const auto& algo : algorithms)
        {
            // Count by type
            if (algo.metadata.algorithmType == "difference")
                stats.differenceAlgorithms++;
            else if (algo.metadata.algorithmType == "morph")
                stats.morphAlgorithms++;
            else
                stats.otherAlgorithms++;
            
            // Collect authors
            authors.insert(algo.metadata.author);
            
            // Track dates
            if (algo.metadata.creationDate < stats.oldestCreationDate)
                stats.oldestCreationDate = algo.metadata.creationDate;
            
            if (algo.metadata.creationDate > stats.newestCreationDate)
                stats.newestCreationDate = algo.metadata.creationDate;
        }
        
        stats.uniqueAuthors.assign(authors.begin(), authors.end());
        
        return stats;
    }

private:
    std::vector<AlgorithmDNA> algorithms;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmLibrary)
};