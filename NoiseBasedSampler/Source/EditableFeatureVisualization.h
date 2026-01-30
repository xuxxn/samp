/*
EditableFeatureVisualization.h
Интерактивный UI компонент для редактирования features с улучшенным редактированием и ZOOM.
Функции:

Три графика: Amplitude, Frequency, Phase
Click & drag для редактирования значений
Mouse wheel для ZOOM (1x-20x)
Shift + drag для PAN (перемещение по увеличенному view)
Плавное редактирование с Gaussian interpolation
Увеличенный smooth radius (15) для более плавных изменений
Интерполяция между точками для плавного рисования
Автоматический reset zoom при изменении семпла

Контекст и связи:

Встроен в MainPanel
Получает FeatureData из PluginProcessor
При mouseUp вызывает processor.applyFeatureChangesToSample()
Отображает текущее состояние features в реальном времени (30 FPS)
Zoom center следует за позицией мыши
Visual feedback: crosshair cursor, zoom indicator, pan hints
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Интерактивный компонент для редактирования features с улучшенным редактированием
class EditableFeatureVisualizationComponent : public juce::Component,
    public juce::Timer
{
public:
    EditableFeatureVisualizationComponent(NoiseBasedSamplerAudioProcessor& p)
        : processor(p)
    {
        startTimerHz(30);
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }

    void timerCallback() override
    {
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();

        if (!processor.hasFeatureData())
        {
            g.setColour(juce::Colours::grey);
            g.setFont(14.0f);
            g.drawText("Load a sample to edit features\n(Click and drag to modify, Mouse wheel to zoom)",
                area, juce::Justification::centred);
            return;
        }

        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        if (numSamples == 0)
            return;

        // 3 графика с интерактивным редактированием
        int chartHeight = (area.getHeight() - 60) / 3;

        amplitudeArea = area.removeFromTop(chartHeight);
        area.removeFromTop(10);
        frequencyArea = area.removeFromTop(chartHeight);
        area.removeFromTop(10);
        phaseArea = area;

        // AMPLITUDE CHART
        drawEditableChart(g, amplitudeArea, ChartType::Amplitude, features);

        // FREQUENCY CHART
        drawEditableChart(g, frequencyArea, ChartType::Frequency, features);

        // PHASE CHART
        drawEditableChart(g, phaseArea, ChartType::Phase, features);

        // Подсказка
        if (isDragging)
        {
            g.setColour(juce::Colour(0xff10b981));
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("Editing... Release to apply",
                getLocalBounds().withHeight(30),
                juce::Justification::centredTop);
        }
        else
        {
            g.setColour(juce::Colours::grey);
            g.setFont(10.0f);

            juce::String hint = "Click & drag to edit | Mouse wheel to ZOOM: " +
                juce::String(zoomLevel, 1) + "x";

            if (zoomLevel > 1.0f)
                hint += " | Hold Shift + drag to PAN";

            g.drawText(hint, getLocalBounds().removeFromBottom(20),
                juce::Justification::centred);
        }

        // Zoom indicator
        if (zoomLevel > 1.0f)
        {
            g.setColour(juce::Colour(0xff3b82f6).withAlpha(0.8f));
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("ZOOM: " + juce::String(zoomLevel, 1) + "x",
                area.getX() + 10, area.getY() + 10, 100, 20,
                juce::Justification::centredLeft);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (!processor.hasFeatureData())
            return;

        // Shift + drag = panning
        if (event.mods.isShiftDown() && zoomLevel > 1.0f)
        {
            isPanning = true;
            lastMousePos = event.position;
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }
        else
        {
            isDragging = true;
            lastEditPos = event.position;
            modifyFeatureAtPosition(event.position);
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!processor.hasFeatureData())
            return;

        if (isPanning)
        {
            // Pan the view
            float deltaX = event.position.x - lastMousePos.x;
            panOffset -= deltaX / getWidth() * (1.0f / zoomLevel);
            panOffset = juce::jlimit(0.0f, 1.0f - (1.0f / zoomLevel), panOffset);
            lastMousePos = event.position;
            repaint();
        }
        else if (isDragging)
        {
            // Непрерывное редактирование вдоль траектории мыши
            interpolateEditPath(lastEditPos, event.position);
            lastEditPos = event.position;
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (isPanning)
        {
            isPanning = false;
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            return;
        }

        if (!isDragging)
            return;

        isDragging = false;

        // Применяем изменения к сэмплу
        processor.applyFeatureChangesToSample();

        DBG("✅ Feature editing applied to sample");
    }

    void mouseWheelMove(const juce::MouseEvent& event,
        const juce::MouseWheelDetails& wheel) override
    {
        if (!processor.hasFeatureData())
            return;

        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        // Определяем какой график под мышью
        juce::Rectangle<int> activeArea;
        if (amplitudeArea.contains(event.position.toInt()))
            activeArea = amplitudeArea;
        else if (frequencyArea.contains(event.position.toInt()))
            activeArea = frequencyArea;
        else if (phaseArea.contains(event.position.toInt()))
            activeArea = phaseArea;
        else
            return;

        // Вычисляем позицию мыши в нормализованных координатах (0-1) ПЕРЕД зумом
        float mouseNormalizedX = (event.position.x - activeArea.getX()) /
            static_cast<float>(activeArea.getWidth());
        mouseNormalizedX = juce::jlimit(0.0f, 1.0f, mouseNormalizedX);

        // Вычисляем текущую позицию сэмпла под мышью ПЕРЕД зумом
        int startSampleBefore = static_cast<int>(panOffset * numSamples);
        int visibleSamplesBefore = static_cast<int>(numSamples / zoomLevel);
        float sampleUnderMouse = startSampleBefore + mouseNormalizedX * visibleSamplesBefore;

        // Применяем zoom
        float oldZoom = zoomLevel;
        float zoomDelta = wheel.deltaY * 0.5f;
        zoomLevel *= (1.0f + zoomDelta);
        zoomLevel = juce::jlimit(1.0f, 20.0f, zoomLevel);

        // Reset pan if zoom is 1.0
        if (zoomLevel <= 1.0f)
        {
            panOffset = 0.0f;
            zoomLevel = 1.0f;
            repaint();
            return;
        }

        // Вычисляем новый pan чтобы сэмпл под мышью остался на месте
        int visibleSamplesAfter = static_cast<int>(numSamples / zoomLevel);
        int desiredStartSample = static_cast<int>(sampleUnderMouse - mouseNormalizedX * visibleSamplesAfter);
        panOffset = desiredStartSample / static_cast<float>(numSamples);

        // Keep pan in bounds
        float maxPan = 1.0f - (1.0f / zoomLevel);
        panOffset = juce::jlimit(0.0f, maxPan, panOffset);

        repaint();
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    // Reset zoom
    void resetZoom()
    {
        zoomLevel = 1.0f;
        panOffset = 0.0f;
        repaint();
    }

private:
    NoiseBasedSamplerAudioProcessor& processor;

    enum class ChartType { Amplitude, Frequency, Phase };

    juce::Rectangle<int> amplitudeArea;
    juce::Rectangle<int> frequencyArea;
    juce::Rectangle<int> phaseArea;

    bool isDragging = false;
    bool isPanning = false;
    float zoomLevel = 1.0f;
    float panOffset = 0.0f;
    juce::Point<float> lastMousePos;
    juce::Point<float> lastEditPos;

    void drawEditableChart(juce::Graphics& g,
        juce::Rectangle<int> area,
        ChartType type,
        const FeatureData& features)
    {
        // Фон
        g.setColour(juce::Colour(0xff2d2d2d));
        g.fillRoundedRectangle(area.toFloat(), 6.0f);

        auto chartArea = area.reduced(5);
        auto stats = features.calculateStatistics();

        // Заголовок и цвет
        juce::Colour chartColour;
        juce::String title;

        switch (type)
        {
        case ChartType::Amplitude:
            chartColour = juce::Colour(0xff3b82f6);
            title = "Amplitude (Click & Drag to Edit)";
            break;
        case ChartType::Frequency:
            chartColour = juce::Colour(0xff10b981);
            title = "Frequency (Hz)";
            break;
        case ChartType::Phase:
            chartColour = juce::Colour(0xfff59e0b);
            title = "Phase (radians)";
            break;
        }

        g.setColour(chartColour);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(title, chartArea.removeFromTop(20), juce::Justification::centredLeft);

        // График с учетом ZOOM и PAN
        int numSamples = features.getNumSamples();

        // Вычисляем видимый диапазон
        int startSample = static_cast<int>(panOffset * numSamples);
        int endSample = static_cast<int>((panOffset + (1.0f / zoomLevel)) * numSamples);
        startSample = juce::jlimit(0, numSamples - 1, startSample);
        endSample = juce::jlimit(startSample + 1, numSamples, endSample);

        int visibleSamples = endSample - startSample;
        int step = std::max(1, visibleSamples / 800);

        juce::Path path;
        bool firstPoint = true;

        for (int i = startSample; i < endSample; i += step)
        {
            float normalizedX = static_cast<float>(i - startSample) / visibleSamples;
            float x = chartArea.getX() + normalizedX * chartArea.getWidth();
            float y = 0.0f;

            switch (type)
            {
            case ChartType::Amplitude:
                y = chartArea.getCentreY() - features[i].amplitude * chartArea.getHeight() * 0.4f;
                break;

            case ChartType::Frequency:
            {
                float freqRange = stats.maxFrequency - stats.minFrequency;
                if (freqRange < 1.0f) freqRange = 1.0f;
                float normalized = (features[i].frequency - stats.minFrequency) / freqRange;
                y = chartArea.getBottom() - normalized * chartArea.getHeight() * 0.9f;
                break;
            }

            case ChartType::Phase:
            {
                float normalized = features[i].phase / juce::MathConstants<float>::twoPi;
                y = chartArea.getBottom() - normalized * chartArea.getHeight() * 0.9f;
                break;
            }
            }

            if (firstPoint)
            {
                path.startNewSubPath(x, y);
                firstPoint = false;
            }
            else
            {
                path.lineTo(x, y);
            }
        }

        // Полупрозрачная заливка под графиком
        juce::Path fillPath = path;
        fillPath.lineTo(chartArea.getRight(), chartArea.getBottom());
        fillPath.lineTo(chartArea.getX(), chartArea.getBottom());
        fillPath.closeSubPath();

        g.setColour(chartColour.withAlpha(0.1f));
        g.fillPath(fillPath);

        // Линия графика
        g.setColour(chartColour);
        g.strokePath(path, juce::PathStrokeType(2.0f));

        // Центральная линия для amplitude
        if (type == ChartType::Amplitude)
        {
            g.setColour(juce::Colours::grey.withAlpha(0.3f));
            g.drawLine(chartArea.getX(), chartArea.getCentreY(),
                chartArea.getRight(), chartArea.getCentreY(), 1.0f);
        }

        // Grid lines для zoom
        if (zoomLevel > 2.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            for (int i = 0; i <= 10; ++i)
            {
                float x = chartArea.getX() + (i / 10.0f) * chartArea.getWidth();
                g.drawLine(x, chartArea.getY(), x, chartArea.getBottom(), 1.0f);
            }
        }

        // Интерактивная зона подсветки
        if (chartArea.contains(getMouseXYRelative()) && !isPanning)
        {
            int mouseX = getMouseXYRelative().x;
            g.setColour(chartColour.withAlpha(0.3f));
            g.drawLine(mouseX, chartArea.getY(), mouseX, chartArea.getBottom(), 2.0f);
        }
    }

    // Интерполяция между двумя точками для плавного редактирования
    void interpolateEditPath(juce::Point<float> from, juce::Point<float> to)
    {
        // Вычисляем расстояние между точками
        float distance = from.getDistanceFrom(to);

        // Количество промежуточных точек (больше = плавнее)
        int steps = std::max(1, static_cast<int>(distance / 2.0f));

        for (int i = 0; i <= steps; ++i)
        {
            float t = i / static_cast<float>(steps);
            juce::Point<float> interpolated = from + (to - from) * t;
            modifyFeatureAtPosition(interpolated);
        }
    }

    void modifyFeatureAtPosition(juce::Point<float> pos)
    {
        const auto& features = processor.getFeatureData();
        int numSamples = features.getNumSamples();

        if (numSamples == 0)
            return;

        // Определяем какой график редактируем
        ChartType chart;
        juce::Rectangle<int> chartRect;

        if (amplitudeArea.contains(pos.toInt()))
        {
            chart = ChartType::Amplitude;
            chartRect = amplitudeArea.reduced(5);
        }
        else if (frequencyArea.contains(pos.toInt()))
        {
            chart = ChartType::Frequency;
            chartRect = frequencyArea.reduced(5);
        }
        else if (phaseArea.contains(pos.toInt()))
        {
            chart = ChartType::Phase;
            chartRect = phaseArea.reduced(5);
        }
        else
        {
            return; // Вне графиков
        }

        chartRect.removeFromTop(20); // Убираем заголовок

        // Конвертируем X позицию в индекс сэмпла с учетом ZOOM и PAN
        float normalizedX = (pos.x - chartRect.getX()) / chartRect.getWidth();
        normalizedX = juce::jlimit(0.0f, 1.0f, normalizedX);

        // Учитываем zoom и pan
        int startSample = static_cast<int>(panOffset * numSamples);
        int visibleSamples = static_cast<int>((numSamples / zoomLevel));
        int sampleIndex = startSample + static_cast<int>(normalizedX * visibleSamples);
        sampleIndex = juce::jlimit(0, numSamples - 1, sampleIndex);

        // Конвертируем Y позицию в значение
        float normalizedY = 1.0f - ((pos.y - chartRect.getY()) / chartRect.getHeight());
        normalizedY = juce::jlimit(0.0f, 1.0f, normalizedY);

        // Применяем к соответствующему параметру
        auto stats = features.calculateStatistics();

        // УВЕЛИЧЕННЫЙ smooth radius для более плавного редактирования
        int baseRadius = 15; // Было 5, теперь 15 (в 3 раза больше)
        int smoothRadius = std::max(3, static_cast<int>(baseRadius / std::sqrt(zoomLevel)));

        switch (chart)
        {
        case ChartType::Amplitude:
        {
            // Amplitude: -1.0 to 1.0
            float value = (normalizedY - 0.5f) * 2.0f;
            processor.setFeatureAmplitudeAt(sampleIndex, value);

            // Плавное редактирование соседних точек с улучшенной интерполяцией
            for (int i = -smoothRadius; i <= smoothRadius; ++i)
            {
                int idx = sampleIndex + i;
                if (idx >= 0 && idx < numSamples && i != 0)
                {
                    // Используем гауссову интерполяцию для более плавного перехода
                    float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                    float currentValue = features[idx].amplitude;
                    float smoothedValue = currentValue + (value - currentValue) * weight * 0.7f;
                    processor.setFeatureAmplitudeAt(idx, smoothedValue);
                }
            }
            break;
        }

        case ChartType::Frequency:
        {
            float freqRange = stats.maxFrequency - stats.minFrequency;
            if (freqRange < 1.0f) freqRange = 1000.0f;

            float value = stats.minFrequency + normalizedY * freqRange;
            value = juce::jlimit(20.0f, 20000.0f, value);
            processor.setFeatureFrequencyAt(sampleIndex, value);

            // Плавное редактирование
            for (int i = -smoothRadius; i <= smoothRadius; ++i)
            {
                int idx = sampleIndex + i;
                if (idx >= 0 && idx < numSamples && i != 0)
                {
                    float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                    float currentValue = features[idx].frequency;
                    float smoothedValue = currentValue + (value - currentValue) * weight * 0.7f;
                    processor.setFeatureFrequencyAt(idx, smoothedValue);
                }
            }
            break;
        }

        case ChartType::Phase:
        {
            float value = normalizedY * juce::MathConstants<float>::twoPi;
            processor.setFeaturePhaseAt(sampleIndex, value);

            // Плавное редактирование
            for (int i = -smoothRadius; i <= smoothRadius; ++i)
            {
                int idx = sampleIndex + i;
                if (idx >= 0 && idx < numSamples && i != 0)
                {
                    float weight = std::exp(-(i * i) / (2.0f * smoothRadius * smoothRadius / 9.0f));
                    float currentValue = features[idx].phase;
                    float smoothedValue = currentValue + (value - currentValue) * weight * 0.7f;
                    processor.setFeaturePhaseAt(idx, smoothedValue);
                }
            }
            break;
        }
        }

        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditableFeatureVisualizationComponent)
};