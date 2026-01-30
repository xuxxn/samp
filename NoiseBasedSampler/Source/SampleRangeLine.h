// ========== SAMPLE RANGE CONTROL LINES ==========
class SampleRangeLine : public juce::Component
{
public:
    enum class LineType { Start, Length };
    
    SampleRangeLine(LineType lineType, NoiseBasedSamplerAudioProcessor& proc)
        : type(lineType), processor(proc), currentPercent(1.0f)
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Draw background
        g.setColour(juce::Colour(0xff1f2937).withAlpha(0.6f));
        g.fillRect(bounds);
        
        // Draw line based on current position
        juce::Colour lineColour = (type == LineType::Start) 
            ? juce::Colour(0xfffb923c) // Orange
            : juce::Colour(0xff3b82f6); // Blue
            
        g.setColour(lineColour);
        
        // Draw control line
        float lineY = bounds.getCentreY();
        float lineThickness = 4.0f;
        
        // Draw vertical line at position
        float xPos = bounds.getX() + currentPercent * bounds.getWidth();
        g.drawLine(xPos, lineY - 10, xPos, lineY + 10, lineThickness);
        
        // Draw handle
        float handleSize = 16.0f;
        juce::Rectangle<float> handle(
            xPos - handleSize/2.0f, 
            lineY - handleSize/2.0f, 
            handleSize, 
            handleSize
        );
        
        g.setColour(lineColour.withAlpha(isMouseOver ? 1.0f : 0.9f));
        g.fillRoundedRectangle(handle, 3.0f);
        
        g.setColour(juce::Colours::white);
        g.drawRoundedRectangle(handle.reduced(1.5f), 3.0f, 1.5f);
        
        // Draw percentage text
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        juce::String label = (type == LineType::Start) ? "START: " : "LENGTH: ";
        label += juce::String(currentPercent * 100.0f, 1) + "%";
        g.drawText(label, bounds.reduced(5), juce::Justification::centredTop);
    }
    
    void mouseEnter(const juce::MouseEvent&) override
    {
        isMouseOver = true;
        repaint();
    }
    
    void mouseExit(const juce::MouseEvent&) override
    {
        isMouseOver = false;
        repaint();
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            isDragging = true;
            dragStartX = e.position.x;
            originalPercent = currentPercent;
            repaint();
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!isDragging) return;
        
        auto bounds = getLocalBounds().toFloat();
        float deltaX = e.position.x - dragStartX;
        float percentDelta = deltaX / bounds.getWidth();
        
        float newPercent = juce::jlimit(0.0f, 1.0f, 
            originalPercent + percentDelta);
            
        if (std::abs(newPercent - currentPercent) > 0.01f)
        {
            currentPercent = newPercent;
            
            // Apply real sample modification immediately
            if (type == LineType::Start)
            {
                processor.applySampleStart(currentPercent);
            }
            else
            {
                processor.applySampleLength(currentPercent);
            }
            
            repaint();
        }
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        if (isDragging)
        {
            isDragging = false;
            
            float finalPercent = currentPercent;
            if (type == LineType::Start)
            {
                processor.applySampleStart(finalPercent);
            }
            else
            {
                processor.applySampleLength(finalPercent);
            }
            
            repaint();
        }
    }
    
    void mouseWheelMove(const juce::MouseEvent&,
        const juce::MouseWheelDetails& wheel) override
    {
        float delta = wheel.deltaY * 0.01f;
        float newPercent = juce::jlimit(0.0f, 1.0f, currentPercent + delta);
        
        if (std::abs(newPercent - currentPercent) > 0.001f)
        {
            currentPercent = newPercent;
            
            if (type == LineType::Start)
            {
                processor.applySampleStart(currentPercent);
            }
            else
            {
                processor.applySampleLength(currentPercent);
            }
            
            repaint();
        }
    }
    
    void updateFromProcessor()
    {
        if (type == LineType::Start)
        {
            currentPercent = processor.getSampleStartPercent();
        }
        else
        {
            currentPercent = processor.getSampleLengthPercent();
        }
        repaint();
    }
    
private:
    LineType type;
    NoiseBasedSamplerAudioProcessor& processor;
    
    float currentPercent = 1.0f; // Start: 1.0 = no offset, Length: 1.0 = full length
    float originalPercent = 1.0f;
    
    bool isDragging = false;
    bool isMouseOver = false;
    float dragStartX = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleRangeLine)
};