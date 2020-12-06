#pragma once
struct AVFrame;
struct AVPacket;
class AVCodecContext;

enum SAMPLEFMT
{
    X_S16 = 1,
    X_FLATP = 8
};

// Class for encoding vidio and audio
class MediaEncodeFactory
{
public:
    // Init context for size and format transformation
    virtual bool InitScale() = 0;

    // Init context for resample
    virtual bool InitResample() = 0;

    // Init codec for video encoding
    virtual bool InitVideoCodec() = 0;

    // Init codec for audio encoding
    virtual bool InitAudioCode() = 0;

    // Resample
    virtual AVFrame *Resample(char *data) = 0;

    // RBB to YUV transform
    virtual AVFrame* RGBToYUV(char *rgb) = 0;

    // Vedio encoding
    virtual AVPacket * EncodeVideo(AVFrame* frame) = 0;

    // Audio encoding
    virtual AVPacket * EncodeAudio(AVFrame* frame) = 0;

    static MediaEncodeFactory * Get(unsigned char index = 0);

    virtual ~MediaEncodeFactory();

    // Context for video codec
    AVCodecContext *videoCodecContext = nullptr;

    // Context for audio codec
    AVCodecContext *audioCodecContext = nullptr;

    // Input parameter
    int inWidth = 1280;
    int inHeight = 720;
    int inPixSize = 3;
    int channels = 2;
    int sampleRate = 44100;
    SAMPLEFMT inSampleFmt = X_S16;

    // Output parameter
    int outWidth = 1280;
    int outHeight = 720;
    int bitrate = 4000000;//压缩后每秒视频的bit位大小 50kB
    int fps = 25;
    int nbSample = 1024;
    SAMPLEFMT outSampleFmt = X_FLATP;
protected:
    MediaEncodeFactory();
};
