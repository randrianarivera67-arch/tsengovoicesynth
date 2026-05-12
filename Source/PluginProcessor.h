#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <deque>

class TsengoProcessor : public juce::AudioProcessor,
                        private juce::AudioIODeviceCallback
{
public:
    TsengoProcessor();
    ~TsengoProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return name_; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return true;  }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()   override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return preset_; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    //==========================================================================
    // Mic device
    juce::StringArray getInputDevices();
    void openDevice (const juce::String& name);
    void closeDevice();
    juce::String getCurrentDevice() const { return currentDevice_; }

    // UI read-outs
    float getMicLevel()  const { return micLevel_.load();  }
    float getMidiLevel() const { return midiLevel_.load(); }
    int   getNote()      const { return currentNote_.load(); }
    float getPitchHz()   const { return currentHz_.load();  }
    float getConfidence() const { return confidence_.load(); }

    // Parameters
    std::atomic<float> threshold { 0.10f };
    std::atomic<float> smoothing { 0.15f };
    std::atomic<float> gain      { 1.00f };

private:
    //==========================================================================
    // AudioIODeviceCallback — mic capture
    void audioDeviceIOCallbackWithContext(
        const float* const* in, int numIn,
        float* const* out,     int numOut,
        int numSamples,
        const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    //==========================================================================
    // YIN pitch detector — haute précision
    float yinDetect (const float* buf, int N, float sr);
    float parabolicInterp (const juce::Array<float>& d, int tau);

    //==========================================================================
    // Mic ring buffer
    static constexpr int RING = 65536;
    std::array<float, RING> ring_ {};
    std::atomic<int>  ringWrite_ { 0 };
    float             yinBuf_[2048] {};

    // MIDI state
    std::atomic<int>   currentNote_ { -1 };
    std::atomic<float> currentHz_   { 0.f };
    std::atomic<float> confidence_  { 0.f };
    std::atomic<float> micLevel_    { 0.f };
    std::atomic<float> midiLevel_   { 0.f };

    int  lastMidiNote_ { -1 };
    int  noteHoldCount_{ 0  };
    static constexpr int HOLD_FRAMES = 3;

    // Median filter
    static constexpr int MED = 7;
    std::deque<float> medBuf_;

    // Device manager
    juce::AudioDeviceManager micMgr_;
    juce::String currentDevice_;
    bool deviceOpen_ { false };
    double sampleRate_ { 44100.0 };

    const juce::String name_   { "Tsengo Voice Synth" };
    const juce::String preset_ { "Default" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TsengoProcessor)
};
