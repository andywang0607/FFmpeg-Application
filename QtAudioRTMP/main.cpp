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

// nginx-rtmp url
static const string outUrl = "rtmp://127.0.0.1:443/live/home";

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    avformat_network_init();

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

    /// Muxing
    // Init Context
    AVFormatContext *avFormatContext = nullptr;
    ret = avformat_alloc_output_context2(&avFormatContext, nullptr, "flv", outUrl.c_str());
    if (ret != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        cout << buf << endl;
        getchar();
        return -1;
    }

    // Add a new stream to a media file.
    AVStream *avStream = avformat_new_stream(avFormatContext, nullptr);
    if (!avStream)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        cout << buf << endl;
        getchar();
        return -1;
    }
    avStream->codecpar->codec_tag = 0;

    // Copy parameter from codec
    avcodec_parameters_from_context(avStream->codecpar, audioContext);
    av_dump_format(avFormatContext, 0, outUrl.c_str(), 1);

    // Open rtmp IO
    ret = avio_open(&avFormatContext->pb, outUrl.c_str(), AVIO_FLAG_WRITE);
    if (ret != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        cout << buf << endl;
        getchar();
        return -1;
    }

    //Allocate the stream private data and write the stream header to an output media file.
    ret = avformat_write_header(avFormatContext, nullptr);
    if (ret != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        cout << buf << endl;
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
        inData[0] = (uint8_t *)buf;
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

        // Upstreaming
        pkt.pts = av_rescale_q(pkt.pts, audioContext->time_base, avStream->time_base);
        pkt.dts = av_rescale_q(pkt.dts, audioContext->time_base, avStream->time_base);
        pkt.duration = av_rescale_q(pkt.duration, audioContext->time_base, avStream->time_base);
        ret = av_interleaved_write_frame(avFormatContext, &pkt);
        if (ret == 0)
        {
            cout << "#" << flush;
        }
    }
    delete buf;
    return a.exec();
}
