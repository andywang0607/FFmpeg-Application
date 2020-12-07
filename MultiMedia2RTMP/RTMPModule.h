#pragma once
#include <memory>
class AVCodecContext;
class AVPacket;

class RTMPModule
{
public:
    // Singleton
    static RTMPModule *Get();

    // Init context for muxing
    bool Init(const char *url);

    // Add video stream or audio stream
    // return index if success, return -1 if fail
    int AddStream(const AVCodecContext *c);

    // Open rtmp io
    bool SendHead();

    // Upstreaming
    bool SendFrame(AVPacket *pkt, int streamIndex = 0);

private:
    static RTMPModule *mInstance;
    struct impl;
    std::unique_ptr<impl> mImpl;

    // Private constructor to prevent instancing. 
    RTMPModule();
    ~RTMPModule();
};

