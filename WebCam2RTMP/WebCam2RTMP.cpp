// WebCam2RTMP.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <future>
extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <opencv2/highgui.hpp>
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "opencv_world.lib")

using namespace std;
using namespace cv;

// nginx-rtmp url
static const string outUrl = "rtmp://127.0.0.1:443/live/home";

int main()
{
    // regist network for RTMP
    avformat_network_init();

    VideoCapture cam;
    Mat frame;
    namedWindow("Camera Image");

    // Context for pixel and format transformation
    SwsContext *swsContext = nullptr;

    // Struct of output data (decode data)
    AVFrame *yuv = nullptr;

    // Context for codec
    AVCodecContext *avCodecContext = nullptr;

    // Format IO context
    AVFormatContext *avFormatContext = nullptr;

    try {
        // Open camera
        cam.open(0, CAP_DSHOW);
        if (!cam.isOpened()) {
            throw exception("Webcam open failed!");
        }
        cam.set(CV_CAP_PROP_FPS, 30);
        int imgWidth = cam.get(CAP_PROP_FRAME_WIDTH);
        int imgHeight = cam.get(CAP_PROP_FRAME_HEIGHT);
        int fps = cam.get(CAP_PROP_FPS);

        // Init swsContext
        swsContext = sws_getCachedContext(swsContext,
            imgWidth, imgHeight, AV_PIX_FMT_BGR24,
            imgWidth, imgHeight, AV_PIX_FMT_YUV420P,
            SWS_FAST_BILINEAR,
            nullptr, nullptr, nullptr);
        if (!swsContext) {
            throw exception("sws_getCachedContext failed!");
        }

        // Init struct of output data
        yuv = av_frame_alloc();
        if (!yuv) {
            throw exception("av_frame_alloc failed!");
        }

        // Config to yuv
        yuv->format = AV_PIX_FMT_YUV420P;
        yuv->width = imgWidth;
        yuv->height = imgHeight;
        yuv->pts = 0;

        // Allocate buffur for yuv
        int ret = av_frame_get_buffer(yuv, 0);
        if (ret != 0) {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }

        // Find Codec
        AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            throw exception("Can`t find h264 encoder!");
        }

        // Create context for codec
        avCodecContext = avcodec_alloc_context3(codec);
        if (!avCodecContext) {
            throw exception("avcodec_alloc_context3 failed!");
        }

        // Config parameter for codec
        avCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //全局参数
        avCodecContext->codec_id = codec->id;
        avCodecContext->thread_count = 8;

        avCodecContext->bit_rate = 50 * 1024 * 8;//压缩后每秒视频的bit位大小 50kB
        avCodecContext->width = imgWidth;
        avCodecContext->height = imgHeight;
        avCodecContext->time_base = { 1, fps };
        avCodecContext->framerate = { fps, 1 };

        avCodecContext->gop_size = 50;
        avCodecContext->max_b_frames = 0;
        avCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;

        //  Open codec context 
        std::future<int> fuRes = std::async(std::launch::async, avcodec_open2, avCodecContext, nullptr, nullptr);
        ret = fuRes.get();
        if (ret != 0) {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }

        // Create IO context
        ret = avformat_alloc_output_context2(&avFormatContext, nullptr, "flv", outUrl.c_str());
        if (ret != 0) {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }

        // Add vedio stream
        AVStream *avStream = avformat_new_stream(avFormatContext, nullptr);
        if (!avStream) {
            throw exception("avformat_new_stream failed");
        }
        avStream->codecpar->codec_tag = 0;

        // Copy paramter from codec
        avcodec_parameters_from_context(avStream->codecpar, avCodecContext);
        av_dump_format(avFormatContext, 0, outUrl.c_str(), 1);

        // Open RTMP network IO
        ret = avio_open(&avFormatContext->pb, outUrl.c_str(), AVIO_FLAG_WRITE);
        if (ret != 0) {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }

        // write the stream header to an output media file
        ret = avformat_write_header(avFormatContext, nullptr);
        if (ret != 0) {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }

        AVPacket pack;
        memset(&pack, 0, sizeof(pack));
        int vpts = 0;

        while (true) {
            if (!cam.grab()) {
                continue;
            }
            if (!cam.retrieve(frame)) {
                continue;
            }
            imshow("Camera Image", frame);
            char key = waitKey(1);
            if (key == 'c') break;
            ///rgb to yuv
            // input data
            uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
            indata[0] = frame.data;
            int insize[AV_NUM_DATA_POINTERS] = { 0 };
            //data of column
            insize[0] = frame.cols * frame.elemSize();
            int h = sws_scale(swsContext, indata, insize, 0, frame.rows, //src data
                yuv->data, yuv->linesize);
            if (h <= 0)
                continue;

            ///h264 encode
            yuv->pts = vpts;
            vpts++;
            ret = avcodec_send_frame(avCodecContext, yuv);
            if (ret != 0)
                continue;

            ret = avcodec_receive_packet(avCodecContext, &pack);
            if (ret != 0)
                continue;

            // Upstream
            pack.pts = av_rescale_q(pack.pts, avCodecContext->time_base, avStream->time_base);
            pack.dts = av_rescale_q(pack.dts, avCodecContext->time_base, avStream->time_base);
            pack.duration = av_rescale_q(pack.duration, avStream->time_base, avStream->time_base);
            ret = av_interleaved_write_frame(avFormatContext, &pack);
        }
    }
    catch (exception &ex) {
        if (cam.isOpened())
            cam.release();
        if (swsContext)
        {
            sws_freeContext(swsContext);
            swsContext = nullptr;
        }

        if (avCodecContext)
        {
            avcodec_free_context(&avCodecContext);
        }
        cerr << ex.what() << endl;
    }
    if (cam.isOpened())
        cam.release();
    if (swsContext)
    {
        sws_freeContext(swsContext);
        swsContext = nullptr;
    }

    if (avCodecContext)
    {
        avcodec_free_context(&avCodecContext);
    }
    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
