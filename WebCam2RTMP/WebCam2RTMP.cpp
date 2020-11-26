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

int main()
{
    // nginx-rtmp url
    const char *outUrl = "rtmp://127.0.0.1:443/live/home";

    //注册所有的编解码器
    //avcodec_register_all();

    //注册所有的封装器
    //av_register_all();

    //注册所有网络协议
    avformat_network_init();


    VideoCapture cam;
    Mat frame;
    namedWindow("video");

    //像素格式转换上下文
    SwsContext *vsc = NULL;

    //输出的数据结构
    AVFrame *yuv = NULL;

    //编码器上下文
    AVCodecContext *vc = NULL;

    //rtmp flv 封装器
    AVFormatContext *ic = NULL;


    try
    {	////////////////////////////////////////////////////////////////
        /// 1 使用opencv打开rtsp相机
        cam.open(0, cv::CAP_DSHOW);
        if (!cam.isOpened())
        {
            throw exception("cam open failed!");
        }
        cout <<  " cam open success" << endl;
        cam.set(CV_CAP_PROP_FPS, 20);
        int inWidth = cam.get(CAP_PROP_FRAME_WIDTH);
        int inHeight = cam.get(CAP_PROP_FRAME_HEIGHT);
        int fps = cam.get(CAP_PROP_FPS);
        cout << "inWidth" << inWidth << endl;
        cout << "inHeight" << inHeight << endl;
        cout << "fps" << fps << endl;
        ///2 初始化格式转换上下文
        cout << " Begin sws_getCachedContext" << endl;
        vsc = sws_getCachedContext(vsc,
            inWidth, inHeight, AV_PIX_FMT_BGR24,	 //源宽、高、像素格式
            inWidth, inHeight, AV_PIX_FMT_YUV420P,//目标宽、高、像素格式
            SWS_BICUBIC,  // 尺寸变化使用算法
            0, 0, 0
        );
        cout << "sws_getCachedContext success" << endl;
        if (!vsc)
        {
            throw exception("sws_getCachedContext failed!");
        }
        ///3 初始化输出的数据结构
        yuv = av_frame_alloc();
        if (!yuv) {
            throw exception("av_frame_alloc failed!");
        }
        cout << "av_frame_alloc success" << endl;
        yuv->format = AV_PIX_FMT_YUV420P;
        yuv->width = inWidth;
        yuv->height = inHeight;
        yuv->pts = 0;
        //分配yuv空间
        int ret = av_frame_get_buffer(yuv, 32);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }
        cout << "av_frame_get_buffer success" << endl;
        ///4 初始化编码上下文
        //a 找到编码器
        AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec)
        {
            throw exception("Can`t find h264 encoder!");
        }
        cout << "avcodec_find_encoder success" << endl;
        //b 创建编码器上下文
        vc = avcodec_alloc_context3(codec);
        if (!vc)
        {
            throw exception("avcodec_alloc_context3 failed!");
        }
        cout << "avcodec_alloc_context3 success" << endl;
        //c 配置编码器参数
        vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //全局参数
        vc->codec_id = codec->id;
        vc->thread_count = 8;

        vc->bit_rate = 50 * 1024 * 8;//压缩后每秒视频的bit位大小 50kB
        vc->width = inWidth;
        vc->height = inHeight;
        vc->time_base = { 1,fps };
        vc->framerate = { fps,1 };
        cout << "fps"  << fps << endl;
        //画面组的大小，多少帧一个关键帧
        vc->gop_size = 50;
        vc->max_b_frames = 0;
        vc->pix_fmt = AV_PIX_FMT_YUV420P;
        //d 打开编码器上下文
        std::future<int> fuRes = std::async(std::launch::async, avcodec_open2, vc, nullptr, nullptr);
        ret = fuRes.get();
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }
        cout << "avcodec_open2 success!" << endl;

        ///5 输出封装器和视频流配置
        //a 创建输出封装器上下文
        ret = avformat_alloc_output_context2(&ic, 0, "flv", outUrl);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }
        //b 添加视频流 
        AVStream *vs = avformat_new_stream(ic, NULL);
        if (!vs)
        {
            throw exception("avformat_new_stream failed");
        }
        vs->codecpar->codec_tag = 0;
        //从编码器复制参数
        avcodec_parameters_from_context(vs->codecpar, vc);
        av_dump_format(ic, 0, outUrl, 1);


        ///打开rtmp 的网络输出IO
        ret = avio_open(&ic->pb, outUrl, AVIO_FLAG_WRITE);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }

        //写入封装头
        ret = avformat_write_header(ic, NULL);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            throw exception(buf);
        }

        AVPacket pack;
        memset(&pack, 0, sizeof(pack));
        int vpts = 0;
        for (;;)
        {
            ///读取rtsp视频帧，解码视频帧
            if (!cam.grab())
            {
                continue;
            }
            ///yuv转换为rgb
            if (!cam.retrieve(frame))
            {
                continue;
            }
            imshow("video", frame);
            waitKey(1);


            ///rgb to yuv
            //输入的数据结构
            uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
            //indata[0] bgrbgrbgr
            //plane indata[0] bbbbb indata[1]ggggg indata[2]rrrrr 
            indata[0] = frame.data;
            int insize[AV_NUM_DATA_POINTERS] = { 0 };
            //一行（宽）数据的字节数
            insize[0] = frame.cols * frame.elemSize();
            int h = sws_scale(vsc, indata, insize, 0, frame.rows, //源数据
                yuv->data, yuv->linesize);
            if (h <= 0)
            {
                continue;
            }
            //cout << h << " " << flush;
            ///h264编码
            yuv->pts = vpts;
            vpts++;
            ret = avcodec_send_frame(vc, yuv);
            if (ret != 0)
                continue;

            ret = avcodec_receive_packet(vc, &pack);
            if (ret != 0 || pack.size > 0)
            {
                //cout << "*" << pack.size << flush;
            }
            else
            {
                continue;
            }
            //推流
            pack.pts = av_rescale_q(pack.pts, vc->time_base, vs->time_base);
            pack.dts = av_rescale_q(pack.dts, vc->time_base, vs->time_base);
            pack.duration = av_rescale_q(pack.duration, vc->time_base, vs->time_base);
            ret = av_interleaved_write_frame(ic, &pack);
            if (ret == 0)
            {
                cout << "#" << flush;
            }
        }

    }
    catch (exception &ex)
    {
        if (cam.isOpened())
            cam.release();
        if (vsc)
        {
            sws_freeContext(vsc);
            vsc = NULL;
        }

        if (vc)
        {
            //avio_closep(&ic->pb);
            avcodec_free_context(&vc);
        }

        cerr << ex.what() << endl;
    }
    getchar();
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
