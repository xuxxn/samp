cmake_minimum_required(VERSION 3.15)

project(samp_Updater VERSION 1.0.0)

# Путь к JUCE
set(JUCE_DIR "C:/Users/Arseniy/Downloads/juce-8.0.12-windows/JUCE")

# Добавить JUCE напрямую
add_subdirectory(${JUCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

# Создать GUI приложение
juce_add_gui_app(sampUpdater
    COMPANY_NAME "YourCompany"
    PRODUCT_NAME "samp Updater"
    VERSION 1.0.0
)

# Исходники (только .h и .cpp которые есть)
target_sources(sampUpdater PRIVATE
    Source/Main.cpp
    Source/Config.h
    Source/Core/UpdaterApp.h
    Source/Core/GitHubAPI.h
    Source/Core/FileReplacer.h
    Source/Core/ProcessMonitor.h
    Source/Core/UpdateManager.h
    Source/UI/MainWindow.h
)

# Preprocessor definitions
target_compile_definitions(sampUpdater PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:sampUpdater,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:sampUpdater,JUCE_VERSION>"
)

# JUCE modules
target_link_libraries(sampUpdater PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# C++17
target_compile_features(sampUpdater PRIVATE cxx_std_17)

# Windows specific
if(WIN32)
    target_compile_definitions(sampUpdater PRIVATE
        JUCE_WIN32=1
    )
endif()

# macOS specific
if(APPLE)
    target_compile_definitions(sampUpdater PRIVATE
        JUCE_MAC=1
    )
endif()