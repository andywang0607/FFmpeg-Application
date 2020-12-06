#pragma once
#include "DataManager.h"

enum RECORDMETHOD
{
    QT_AUDIO
};

class AudioRecordFactory : public DataManager
{
public:
    // Begin recording
    virtual bool Init() = 0;

    // Stop recording
    virtual void Stop() = 0;

    // Number of channel
    int channels = 2;

    // Sampling rating
    int sampleRate = 44100;

    int sampleByte = 2;

    // Number of sampling
    int nbSamples = 1024;

    static AudioRecordFactory *Get(RECORDMETHOD method = QT_AUDIO, unsigned char index = 0);

    virtual ~AudioRecordFactory();

protected:
    AudioRecordFactory();
};
