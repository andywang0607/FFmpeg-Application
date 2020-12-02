#include <QtCore/QCoreApplication>
#include <QAudioInput>
#include <iostream>
#include <thread>
extern "C"
{
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#pragma comment(lib,"swresample.lib")
#pragma comment(lib,"avutil.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")

using namespace std;

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    int sampleRate = 44100;
    int channels = 2;
    int sampleByte = 2;

    AVSampleFormat inSampleFormat = AV_SAMPLE_FMT_S16;
    AVSampleFormat outSampleFormat = AV_SAMPLE_FMT_FLTP;

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


    /// Audio resample
    // Init context for resample
    SwrContext *audioResampleContext = nullptr;
    audioResampleContext = swr_alloc_set_opts(audioResampleContext,
        av_get_default_channel_layout(channels), outSampleFormat, sampleRate,
        av_get_default_channel_layout(channels), inSampleFormat, sampleRate,
        0, nullptr);
    if (!audioResampleContext)
    {
        cout << "swr_alloc_set_opts failed!";
        getchar();
        return -1;
    }
    int ret = swr_init(audioResampleContext);
    if (ret != 0)
    {
        char err[1024] = { 0 };
        av_strerror(ret, err, sizeof(err) - 1);
        cout << err << endl;
        getchar();
        return -1;
    }
    cout << "Context for resample init success!" << endl;

    //  Audio ouput allocate
    AVFrame *pcm = av_frame_alloc();
    pcm->format = outSampleFormat;
    pcm->channels = channels;
    pcm->channel_layout = av_get_default_channel_layout(channels);
    pcm->nb_samples = 1024; // Sample number per frame in one channel
    ret = av_frame_get_buffer(pcm, 0); // allocation for pcm
    if (ret != 0)
    {
        char err[1024] = { 0 };
        av_strerror(ret, err, sizeof(err) - 1);
        cout << err << endl;
        getchar();
        return -1;
    } 

    /// Codec
    // Init Audio codec
    AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec)
    {
        cout << "avcodec_find_encoder AV_CODEC_ID_AAC failed!" << endl;
        getchar();
        return -1;
    }

    // Context for codec
    AVCodecContext *audioContext = avcodec_alloc_context3(codec);
    if (!audioContext)
    {
        cout << "avcodec_alloc_context3 AV_CODEC_ID_AAC failed!" << endl;
        getchar();
        return -1;
    }

    // Config audioContext 
    audioContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    audioContext->thread_count = 8;
    audioContext->bit_rate = 40000;
    audioContext->sample_rate = sampleRate;
    audioContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
    audioContext->channels = channels;
    audioContext->channel_layout = av_get_default_channel_layout(channels);

    // Open audio codec
    ret = avcodec_open2(audioContext, codec, nullptr);
    if (ret != 0)
    {
        char err[1024] = { 0 };
        av_strerror(ret, err, sizeof(err) - 1);
        cout << err << endl;
        getchar();
        return -1;
    }
   
    int readSize = pcm->nb_samples*channels*sampleByte;
    char *buf = new char[readSize];
    int apts = 0;
    AVPacket pkt = { 0 };
    while (true)
    {
        // get frame data
        if (input->bytesReady() < readSize)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        int size = 0;
        while (size != readSize)
        {
            int len = io->read(buf + size, readSize - size);
            if (len < 0) break;
            size += len;
        }
        if (size != readSize) continue;
        const uint8_t *inData[AV_NUM_DATA_POINTERS] = { 0 };
        inData[0] = (uint8_t *) buf;
        int len = swr_convert(audioResampleContext, pcm->data, pcm->nb_samples, // output parameter
            inData, pcm->nb_samples);
        cout << len << " ";

        // pts calculation
        // nb_sample/sample_rate  = number of seconds in a frame
        // timebase  pts = sec * timebase.den
        pcm->pts = apts;
        apts += av_rescale_q(pcm->nb_samples, { 1,sampleRate }, audioContext->time_base);

        // codec
        int ret = avcodec_send_frame(audioContext, pcm);
        if (ret != 0) continue;
        av_packet_unref(&pkt);
        ret = avcodec_receive_packet(audioContext, &pkt);
        if (ret != 0) continue;
    }
    delete buf;
    return a.exec();
}
