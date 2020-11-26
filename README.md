# FFmpeg-Application
Implement some application with FFmpeg SDK
 - Vedio2RTMP: Upstreaming a local flv file according to RTMP protocol
 
## Environment
- OS: Win10
- Compiler: Visual studio 2017
- FFmpeg: 4.3.1 (2020-10-01 release)

## Vedio2RTMP
Upstreaming a local flv file according to RTMP protocol and show the streaming in web page
### Step
- Get information from input file
  - avformat_open_input: Open an input stream and read the header
  - avformat_find_stream_info: Read packets of a media file to get stream information
- AVFormatContext for output
  - avformat_alloc_output_context2: Allocate an AVFormatContext for an output format
  - avcodec_find_encoder: Find a registered encoder with a matching codec ID.
  - avformat_new_stream: Add a new stream to a media file
  - avcodec_parameters_copy: Copy the contents of src to dst. Any allocated fields in dst are freed and replaced with newly allocated duplicates of the corresponding fields in src
- RTMP muxing
  - avio_open: Create and initialize a AVIOContext for accessing the resource indicated by url
  - avformat_write_header: Allocate the stream private data and write the stream header to an output media file
  - av_read_frame: Return the next frame of a stream
  - pts to dts transformation and speed control
  - av_interleaved_write_frame: Write a packet to an output media file ensuring correct interleaving
- Show streaming in web page
  - Apply swfs/StrobeMediaPlayback.swf provide by Adobe
  - pluginspage='http://www.adobe.com/go/getflashplayer' 
  - flashvars='&src=rtmp://127.0.0.1:443/live/home&autoHideControlBar=true&streamType=live&autoPlay=true&verbose=true'

## Result
![Execute](RTMPStreaming/result/RTMPStreaming.gif)
  
