#ifndef NATIVE_AUDIO_DRIVER_H
#define NATIVE_AUDIO_DRIVER_H

#include <Arduino.h>

class NativeAudioDriver {
public:
    static NativeAudioDriver& getInstance() {
        static NativeAudioDriver instance;
        return instance;
    }

    bool begin(int bclk = 42, int lrck = 2, int dout = 41, int sampleRate = 44100);
    void playMP3(const char* filePath);
    void playStream(const char* url);
    void stop();
    bool isPlaying() const { return playing; }
    bool isStream() const { return _isStream; }
    const String& getCurrentTarget() const { return currentFilePath; }

private:
    NativeAudioDriver() = default;
    ~NativeAudioDriver() = default;

    bool initialized = false;
    volatile bool playing = false;
    bool _isStream = false;
    TaskHandle_t audioTaskHandle = nullptr;
    String currentFilePath = "";
    int _bclk = 42, _lrck = 2, _dout = 41;

    static void audioTask(void* param);
    static void streamAudioTask(void* param);
};

#endif // NATIVE_AUDIO_DRIVER_H
