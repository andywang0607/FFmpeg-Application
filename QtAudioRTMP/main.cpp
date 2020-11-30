#include <QtCore/QCoreApplication>
#include <QAudioInput>
#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    int sampleRate = 44100;
    int channels = 2;
    int sampleByte = 2;

    // QAudioFormat Config
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
    QAudioInput *input = new QAudioInput(fmt);
    // Begin record audio
    QIODevice *io = input->start();


    char buf[4096] = { 0 };
    while (true)
    {
        // Read a frame data
        if (input->bytesReady() > 4096)
        {
            cout << io->read(buf, sizeof(buf)) << " " << flush;
            continue;
        }
    }
    return a.exec();
}
