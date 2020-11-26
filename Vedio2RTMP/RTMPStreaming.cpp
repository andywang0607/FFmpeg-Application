#include "pch.h"
#include <iostream>
#include <chrono>
#include <thread>
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#include "libavcodec/avcodec.h"
}
using namespace std;

int show_error_msg(int errNum)
{
    char buf[1024] = { 0 };
    av_strerror(errNum, buf, sizeof(buf));
    cout << buf << endl;
    getchar();
    return -1;
}

static double r2d(AVRational r)
{
    return r.num == 0 || r.den == 0 ? 0. : (double)r.num / (double)r.den;
}

int get_file_stream_info(AVFormatContext* &in, const char* file_path) 
{
    if (!file_path) {
        cout << "get_file_stream_info input path null " << endl;
        return -1;
    }
    // Open an input stream and read the header
    int re = avformat_open_input(&in, file_path, 0, 0);
    if (re != 0)
    {
        return show_error_msg(re);
    }
    cout << "open file " << file_path << " Success." << endl;

    // get info of stream
    re = avformat_find_stream_info(in, 0);
    if (re != 0)
    {
        return show_error_msg(re);
    }
    av_dump_format(in, 0, file_path, 0);
    return 0;
}

int create_out_avformat_context(AVFormatContext* &in, const char* format_name, const char* out) {
    // Allocate an AVFormatContext for an output format
    int res = avformat_alloc_output_context2(&in, 0, format_name, out);
    if (!in)
    {
        return show_error_msg(res);
    }
    cout << "octx create success!" << endl;
    return 0;
}

int create_out_stream(AVFormatContext* &in, AVFormatContext* &out) {
    // Output stream setting
    // Traverse AVStream of input
    for (int i = 0; i < in->nb_streams; i++)
    {
        // Create output stream
        AVCodec *codec_in = avcodec_find_encoder(in->streams[i]->codecpar->codec_id);
        AVStream *out_stream = avformat_new_stream(out, codec_in);
        if (!out_stream)
        {
            return show_error_msg(0);
        }
        cout << "avformat_new_stream success!" << endl;
        // Copy the contents of src to dst
        int res = avcodec_parameters_copy(out_stream->codecpar, in->streams[i]->codecpar);
        if (0 != res)
        {
            return show_error_msg(res);
        }
        codec_in->codec_tags = 0;
    }
    return 0;
}

int init_avio_context(AVFormatContext* &out, const char* file_name)
{
    // Create and initialize a AVIOContext for accessing the resource indicated by url.
    int res = avio_open(&out->pb, file_name, AVIO_FLAG_WRITE);
    if (!out->pb)
    {
        return show_error_msg(res);
    }

    // Allocate the stream private data and write the stream header to an output media file.
    res = avformat_write_header(out, 0);
    if (res < 0)
    {
        return show_error_msg(res);
    }
    cout << "avformat_write_header " << res << endl;
    return 0;
}

int muxing(AVFormatContext* &in, AVFormatContext* &out) {
    AVPacket pkt;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now(); //av_gettime();
    while (true)
    {
        int res = av_read_frame(in, &pkt);
        if (res != 0)
        {
            break;
        }
        cout << pkt.pts << " " << flush;
        // pts to dts transformation
        AVRational itime = in->streams[pkt.stream_index]->time_base;
        AVRational otime = out->streams[pkt.stream_index]->time_base;
        pkt.pts = av_rescale_q_rnd(pkt.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
        pkt.dts = av_rescale_q_rnd(pkt.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
        pkt.duration = av_rescale_q_rnd(pkt.duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_NEAR_INF));
        pkt.pos = -1;

        // Speed Control
        if (in->streams[pkt.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            AVRational tb = in->streams[pkt.stream_index]->time_base;
            // Past time
            long long now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTime).count();
            long long dts = 0;
            dts = pkt.dts * (1000 * 1000 * r2d(tb));
            if (dts > now)
                std::this_thread::sleep_for(std::chrono::microseconds(dts - now));
        }

        res = av_interleaved_write_frame(out, &pkt);
        if (res < 0)
        {
            return show_error_msg(res);
        }
    }
    return 0;
}


int main(int argc, char *argv[])
{

    const char *inUrl = "../video//test.flv";                          // input file
    const char *outUrl = "rtmp://127.0.0.1:443/live/home";      // ouput url
    const char* output_format = "flv";                          // output format

#pragma region Get Information from Input file
    AVFormatContext *ictx = NULL;
    if (0 != get_file_stream_info(ictx, inUrl))
        return -1;
#pragma endregion

#pragma region OutputStream
    AVFormatContext *octx = NULL;
    
    if(0 != create_out_avformat_context(octx, output_format, outUrl))
        return -1;

    if (0 != create_out_stream(ictx, octx)) 
        return -1;

    av_dump_format(octx, 0, outUrl, 1);
#pragma endregion


#pragma region RTMP
    if (0 != init_avio_context(octx, outUrl))
        return -1;
    
    if (0 != muxing(ictx, octx))
        return -1;
#pragma endregion
    cout << "file to rtmp test" << endl;

    return 0;
}

