/*
SyncStatusIndicator.h
Визуальный индикатор синхронности всех индексов

Показывает пользователю:
- ✅ Все индексы синхронны
- ⚠️ Features устарели после spectral edit
- ⚠️ Spectral устарел после feature edit
- 🔄 Кнопка ресинхронизации
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SyncStatusIndicator : public juce::Component,
                            public juce::Timer
{
public:
    SyncStatusIndicator(NoiseBasedSamplerAudioProcessor& proc)
        : processor(proc)
    {
        addAndMakeVisible(resyncButton);
        resyncButton.setButtonText("🔄 Resync All");
        resyncButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b));
        resyncButton.onClick = [this] { performResync(); };
        
        startTimerHz(10); // Быстрое обновление для responsive UI
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        
        // Определяем статус
        bool allSynced = processor.areAllIndicesSynced();
        bool featuresStale = processor.areFeaturesModified();
        bool spectralStale = !processor.areSpectralIndicesSynced();
        
        if (allSynced)
        {
            // ✅ ВСЁ СИНХРОННО
            g.setColour(juce::Colour(0xff10b981).withAlpha(0.15f));
            g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
            
            g.setColour(juce::Colour(0xff10b981));
            g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 2.0f);
            
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("✅ All Indices Synchronized",
                bounds.reduced(10), juce::Justification::centredLeft);
            
            // Прячем кнопку
            resyncButton.setVisible(false);
        }
        else
        {
            // ⚠️ ЕСТЬ РАССИНХРОНИЗАЦИЯ
            g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.15f));
            g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
            
            g.setColour(juce::Colour(0xfff59e0b));
            g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 2.0f);
            
            // Текст зависит от того, что устарело
            auto textArea = bounds.reduced(10);
            
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            
            juce::String message = "⚠️ Indices Out of Sync: ";
            
            if (featuresStale && spectralStale)
            {
                message += "Both Features & Spectral need update";
            }
            else if (featuresStale)
            {
                message += "Features need update after Spectral edit";
            }
            else if (spectralStale)
            {
                message += "Spectral needs update after Feature edit";
            }
            
            g.drawText(message, textArea.removeFromLeft(textArea.getWidth() - 120),
                juce::Justification::centredLeft);
            
            // Показываем кнопку
            resyncButton.setVisible(true);
        }
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        resyncButton.setBounds(bounds.removeFromRight(110).withHeight(30));
    }
    
    void timerCallback() override
    {
        repaint();
    }
    
private:
    NoiseBasedSamplerAudioProcessor& processor;
    juce::TextButton resyncButton;
    
    void performResync()
    {
        DBG("🔄 User clicked Resync All");
        
        // Показываем progress
        resyncButton.setEnabled(false);
        resyncButton.setButtonText("Resyncing...");
        
        // Выполняем ресинхронизацию
        processor.forceFullResync();
        
        // Восстанавливаем кнопку
        resyncButton.setEnabled(true);
        resyncButton.setButtonText("🔄 Resync All");
        
        // Уведомляем пользователя
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "✅ Resync Complete",
            "All indices have been synchronized from current audio state.\n\n"
            "• Features re-extracted\n"
            "• Spectral indices re-analyzed\n"
            "• All views now match audio",
            "OK"
        );
        
        repaint();
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SyncStatusIndicator)
};