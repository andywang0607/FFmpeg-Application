#include <QtCore/QCoreApplication>
#include <iostream>
#include <string>
#include <QThread>
#include "MediaEncodeFactory.h"
#include "RtmpModule.h"
#include "AudioRecordFactory.h"
#include "VideoCaptureFactory.h"

using namespace std;

// nginx-rtmp url
static const string outUrl = "rtmp://127.0.0.1:443/live/home";

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    int ret = 0;
    int sampleRate = 44100;
    int channels = 2;
    int sampleByte = 2;
    int nbSample = 1024;

    // Get camera image
    VideoCaptureFactory *videoCaptureInstance = VideoCaptureFactory::Get(0);
    if (!videoCaptureInstance->Init(0))
    {
        cout << "open camera failed!" << endl;
        getchar();
        return -1;
    }
    cout << "open camera success!" << endl;
    videoCaptureInstance->Start();

    // Record audio
    AudioRecordFactory *audioRecorder = AudioRecordFactory::Get(QT_AUDIO, 0);
    audioRecorder->sampleRate = sampleRate;
    audioRecorder->channels = channels;
    audioRecorder->sampleByte = sampleByte;
    audioRecorder->nbSamples = nbSample;
    if (!audioRecorder->Init())
    {
        cout << "XAudioRecord Init failed!" << endl;
        getchar();
        return -1;
    }
    audioRecorder->Start();

    // Encode
    MediaEncodeFactory *mediaEncodeInstance = MediaEncodeFactory::Get(0);

    mediaEncodeInstance->inWidth = videoCaptureInstance->width;
    mediaEncodeInstance->inHeight = videoCaptureInstance->height;
    mediaEncodeInstance->outWidth = videoCaptureInstance->width;
    mediaEncodeInstance->outHeight = videoCaptureInstance->height;
    if (!mediaEncodeInstance->InitScale())
    {
        getchar();
        return -1;
    }

    // Audio resampling
    mediaEncodeInstance->channels = channels;
    mediaEncodeInstance->nbSample = nbSample;
    mediaEncodeInstance->sampleRate = sampleRate;
    mediaEncodeInstance->inSampleFmt = SAMPLEFMT::X_S16;
    mediaEncodeInstance->outSampleFmt = SAMPLEFMT::X_FLATP;
    if (!mediaEncodeInstance->InitResample())
    {
        getchar();
        return -1;
    }
    // Init audio codec
    if (!mediaEncodeInstance->InitAudioCode())
    {
        getchar();
        return -1;
    }

    // Init video codec
    if (!mediaEncodeInstance->InitVideoCodec())
    {
        getchar();
        return -1;
    }

    // RTMP
    RTMPModule *rtmpInstance = RTMPModule::Get();
    if (!rtmpInstance->Init(outUrl.c_str()))
    {
        getchar();
        return -1;
    }

    // Add video stream
    int vindex = 0;
    vindex = rtmpInstance->AddStream(mediaEncodeInstance->videoCodecContext);
    if (vindex < 0)
    {
        getchar();
        return -1;
    }

    // Add audio stream
    int aindex = rtmpInstance->AddStream(mediaEncodeInstance->audioCodecContext);
    if (aindex < 0)
    {
        getchar();
        return -1;
    }

    // RTMP IO
    if (!rtmpInstance->SendHead())
    {
        getchar();
        return -1;
    }
    
    while (true)
    {
        // Get frame data
        Data audioData = audioRecorder->Pop();
        Data videoData = videoCaptureInstance->Pop();
        if (audioData.size <= 0 && videoData.size <= 0)
        {
            QThread::msleep(1);
            continue;
        }

        // Audio processing
        if (audioData.size > 0)
        {
            // Resample
            AVFrame *pcm = mediaEncodeInstance->Resample(audioData.data);
            audioData.Release();
            AVPacket *pkt = mediaEncodeInstance->EncodeAudio(pcm);
            if (pkt)
            {
                // Upstreaming
                if (rtmpInstance->SendFrame(pkt, aindex))
                {
                    cout << "#" << flush;
                }
            }
        }

        // Video processing
        if (videoData.size > 0)
        {
            AVFrame *yuv = mediaEncodeInstance->RGBToYUV(videoData.data);
            videoData.Release();
            AVPacket *pkt = mediaEncodeInstance->EncodeVideo(yuv);
            if (pkt)
            {
                // Upstreaming
                if (rtmpInstance->SendFrame(pkt, vindex))
                {
                    cout << "@" << flush;
                }
            }
        }
    }
    getchar();
    return a.exec();
}
