#include "MediaEncodeFactory.h"
extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}
#include <iostream>
#include <future>

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib,"swresample.lib")

using namespace std;

#if defined WIN32 || defined _WIN32
#include <windows.h>
#endif
// Get number of cpu
static int GetCpuNum()
{
#if defined WIN32 || defined _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);

    return (int)sysinfo.dwNumberOfProcessors;
#elif defined __linux__
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#elif defined __APPLE__
    int numCPU = 0;
    int mib[4];
    size_t len = sizeof(numCPU);

    // set the mib for hw.ncpu
    mib[0] = CTL_HW;
    mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;

                           // get the number of CPUs from the system
    sysctl(mib, 2, &numCPU, &len, NULL, 0);

    if (numCPU < 1)
    {
        mib[1] = HW_NCPU;
        sysctl(mib, 2, &numCPU, &len, NULL, 0);

        if (numCPU < 1)
            numCPU = 1;
    }
    return (int)numCPU;
#else
    return 1;
#endif
}

class CMediaEncode :public MediaEncodeFactory
{
public:
    void Close()
    {
        if (swsContext)
        {
            sws_freeContext(swsContext);
            swsContext = nullptr;
        }
        if (swrContext)
        {
            swr_free(&swrContext);
        }
        if (yuv)
        {
            av_frame_free(&yuv);
        }
        if (videoCodecContext)
        {
            avcodec_free_context(&videoCodecContext);
        }

        if (pcm)
        {
            av_frame_free(&pcm);
        }

        vpts = 0;
        av_packet_unref(&apack);
        apts = 0;
        av_packet_unref(&vpack);
    }

    bool InitAudioCode()
    {
        if (!(audioCodecContext = CreateCodec(AV_CODEC_ID_AAC)))
        {
            return false;
        }
        audioCodecContext->bit_rate = 40000;
        audioCodecContext->sample_rate = sampleRate;
        audioCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
        audioCodecContext->channels = channels;
        audioCodecContext->channel_layout = av_get_default_channel_layout(channels);
        return OpenCodec(&audioCodecContext);
    }

    bool InitVideoCodec()
    {
        if (!(videoCodecContext = CreateCodec(AV_CODEC_ID_H264)))
        {
            return false;
        }
        videoCodecContext->bit_rate = 50 * 1024 * 8;
        videoCodecContext->width = outWidth;
        videoCodecContext->height = outHeight;
        videoCodecContext->time_base = { 1,fps };
        videoCodecContext->framerate = { fps,1 };

        videoCodecContext->gop_size = 50;
        videoCodecContext->max_b_frames = 0;
        videoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
        return OpenCodec(&videoCodecContext);
    }
    AVPacket * EncodeAudio(AVFrame* frame)
    {
        pcm->pts = apts;
        apts += av_rescale_q(pcm->nb_samples, { 1,sampleRate }, audioCodecContext->time_base);
        int ret = avcodec_send_frame(audioCodecContext, pcm);
        if (ret != 0)
            return nullptr;
        av_packet_unref(&apack);
        ret = avcodec_receive_packet(audioCodecContext, &apack);
        if (ret != 0)
            return nullptr;
        return &apack;
    }

    AVPacket * EncodeVideo(AVFrame* frame)
    {
        av_packet_unref(&vpack);
        // h264 encoding
        frame->pts = vpts;
        vpts++;
        int ret = avcodec_send_frame(videoCodecContext, frame);
        if (ret != 0)
            return nullptr;

        ret = avcodec_receive_packet(videoCodecContext, &vpack);
        if (ret != 0 || vpack.size <= 0)
            return nullptr;
        return &vpack;
    }
    bool InitScale()
    {
        // Init swsContext
        swsContext = sws_getCachedContext(swsContext,
            inWidth, inHeight, AV_PIX_FMT_BGR24,	 
            outWidth, outHeight, AV_PIX_FMT_YUV420P,
            SWS_BICUBIC,
            0, 0, 0
        );
        if (!swsContext)
        {
            cout << "sws_getCachedContext failed!";
            return false;
        }
        // Init struct of output data
        yuv = av_frame_alloc();
        yuv->format = AV_PIX_FMT_YUV420P;
        yuv->width = inWidth;
        yuv->height = inHeight;
        yuv->pts = 0;

        // Allocate buffur for yuv
        int ret = av_frame_get_buffer(yuv, 32);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }
        return true;
    }

    AVFrame* RGBToYUV(char *rgb)
    {
        //Input data
        uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
        //indata[0] bgrbgrbgr
        //plane indata[0] bbbbb indata[1]ggggg indata[2]rrrrr 
        indata[0] = (uint8_t*)rgb;
        int insize[AV_NUM_DATA_POINTERS] = { 0 };

        insize[0] = inWidth * inPixSize;

        int h = sws_scale(swsContext, indata, insize, 0, inHeight,
            yuv->data, yuv->linesize);
        if (h <= 0)
        {
            return nullptr;
        }
        return yuv;
    }

    bool InitResample()
    {
        swrContext = nullptr;
        swrContext = swr_alloc_set_opts(swrContext,
            av_get_default_channel_layout(channels), (AVSampleFormat)outSampleFmt, sampleRate, //Output format
            av_get_default_channel_layout(channels), (AVSampleFormat)inSampleFmt, sampleRate, 0, 0); //Input format
        if (!swrContext)
        {
            cout << "swr_alloc_set_opts failed!";
            return false;
        }
        int ret = swr_init(swrContext);
        if (ret != 0)
        {
            char err[1024] = { 0 };
            av_strerror(ret, err, sizeof(err) - 1);
            cout << err << endl;
            return false;
        }

        // Audio ouput allocate
        pcm = av_frame_alloc();
        pcm->format = outSampleFmt;
        pcm->channels = channels;
        pcm->channel_layout = av_get_default_channel_layout(channels);
        pcm->nb_samples = nbSample; // Sample number per frame in one channel
        ret = av_frame_get_buffer(pcm, 0); // allocation for pcm
        if (ret != 0)
        {
            char err[1024] = { 0 };
            av_strerror(ret, err, sizeof(err) - 1);
            cout << err << endl;
            return false;
        }
        return true;
    }
    AVFrame *Resample(char *data)
    {
        const uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
        indata[0] = (uint8_t *)data;
        int len = swr_convert(swrContext, pcm->data, pcm->nb_samples, //Output parameter
            indata, pcm->nb_samples
        );
        if (len <= 0)
        {
            return nullptr;
        }
        return pcm;
    }
private:
    bool OpenCodec(AVCodecContext **c)
    {
        std::future<int> fuRes = std::async(std::launch::async, avcodec_open2, *c, nullptr, nullptr);
        int ret = fuRes.get();
        if (ret != 0)
        {
            char err[1024] = { 0 };
            av_strerror(ret, err, sizeof(err) - 1);
            cout << err << endl;
            avcodec_free_context(c);
            return false;
        }
        cout << "avcodec_open2 success!" << endl;
        return true;
    }

    AVCodecContext* CreateCodec(AVCodecID cid)
    {
        AVCodec *codec = avcodec_find_encoder(cid);
        if (!codec)
        {
            cout << "avcodec_find_encoder  failed!" << endl;
            return nullptr;
        }
        AVCodecContext* c = avcodec_alloc_context3(codec);
        if (!c)
        {
            cout << "avcodec_alloc_context3  failed!" << endl;
            return nullptr;
        }
        cout << "avcodec_alloc_context3 success!" << endl;

        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        c->thread_count = GetCpuNum();
        return c;
    }
    // Context for size and format transformation
    SwsContext *swsContext = nullptr;
    // Context for resample
    SwrContext *swrContext = nullptr;
    // YUV output
    AVFrame *yuv = nullptr;
    // Resample output
    AVFrame *pcm = nullptr;
    // Video data of a frame
    AVPacket vpack = { 0 };
    // Audio data of a frame
    AVPacket apack = { 0 };
    int vpts = 0;
    int apts = 0;
};

MediaEncodeFactory::MediaEncodeFactory()
{
}


MediaEncodeFactory::~MediaEncodeFactory()
{
}

MediaEncodeFactory * MediaEncodeFactory::Get(unsigned char index)
{
    static CMediaEncode instance[255];
    return &instance[index];
}