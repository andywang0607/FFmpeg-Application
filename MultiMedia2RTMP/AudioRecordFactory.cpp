#include "AudioRecordFactory.h"
#include <QAudioInput>
#include <iostream>

using namespace std;

class CAudioRecord :public AudioRecordFactory
{
public:
    void run()
    {
        // Read a frame data
        int readSize{ nbSamples * channels * sampleByte };
        char *buf = new char[readSize];
        while (!isExit)
        {
            // Get data of a frame
            if (input->bytesReady() < readSize)
            {
                QThread::msleep(1);
                continue;
            }
            int size = 0;
            while (size != readSize)
            {
                int len = io->read(buf + size, readSize - size);
                if (len < 0)
                    break;
                size += len;
            }
            if (size != readSize)
            {
                continue;
            }
            // Push data of a frame to list
            Push(Data(buf, readSize, GetCurTime()));
        }
        delete buf;
    }

    bool Init()
    {
        Stop();
        // Config QAudioFormat
        QAudioFormat fmt;
        fmt.setSampleRate(sampleRate);
        fmt.setChannelCount(channels);
        fmt.setSampleSize(sampleByte * 8);
        fmt.setCodec("audio/pcm");
        fmt.setByteOrder(QAudioFormat::LittleEndian);
        fmt.setSampleType(QAudioFormat::UnSignedInt);
        QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
        if (!info.isFormatSupported(fmt))
        {
            cout << "Audio format not support!" << endl;
            fmt = info.nearestFormat(fmt);
        }
        input = new QAudioInput(fmt);

        // Begin recording
        io = input->start();
        if (!io)
            return false;
        return true;
    }
    void Stop()
    {
        DataManager::Stop();
        if (input)
            input->stop();
        if (io)
            io->close();
        input = nullptr;
        io = nullptr;
    }
    QAudioInput *input = nullptr;
    QIODevice *io = nullptr;
};

AudioRecordFactory::AudioRecordFactory()
{
}


AudioRecordFactory::~AudioRecordFactory()
{
}

AudioRecordFactory *AudioRecordFactory::Get(RECORDMETHOD method, unsigned char index)
{
    static CAudioRecord instance[255];
    return &instance[index];
}
