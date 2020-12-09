# FFmpeg-Application
Just some small application for testing FFMPEG
 - FlashVedioPlayer: Show streaming in web page
 - Vedio2RTMP: Upstreaming a local flv file according to RTMP protocol
 - WebCam2RTMP: Get image from webcam and upstreaming according to RTMP protocol
 - Audio2RTMP: Get audio from microphone and upstreaming according to RTMP protocol
 - MultiMedia2RTMP: Upstreaming video and audio simultaneously and finish synchronized 

## Environment
- OS: Win10
- Compiler: Visual studio 2017
- FFmpeg: 4.3.1 (2020-10-01 release)

## FlashVedioPlayer
- Show streaming in web page
  - Apply swfs/StrobeMediaPlayback.swf provide by Adobe
  - pluginspage='http://www.adobe.com/go/getflashplayer' 
  - flashvars='&src=rtmp://127.0.0.1:443/live/home&autoHideControlBar=true&streamType=live&autoPlay=true&verbose=true'

## Vedio2RTMP
Upstreaming a local flv file according to RTMP protocol and show the streaming in web page
### Implement the following function
- Get information from input file
  - Open an input stream and read the header
  - Read packets of a media file to get stream information
- Muxing
  - Allocate an AVFormatContext for an output format
  - Add a new stream to a media file
  - Copy paramter from codec
- Codec 
  - avcodec_find_encoder: Find a registered encoder with a matching codec ID.
  - Encode
- RTMP IO
  - Create and initialize a AVIOContext for accessing the resource indicated by url
  - Allocate the stream private data and write the stream header to an output media file
  - pts to dts transformation and speed control
  - Write a packet to an output media file ensuring correct interleaving

### Result
![Execute](Vedio2RTMP/result/RTMPStreaming.gif)


## Webcam2RTMP
Get image from webcam with opencv and upstreaming according to RTMP protocol

### Implement the following function
- Open camera
  - cv::VedioCapture
- RGB to YUV
  - Init swsContext
  - Init struct of yuv data
  - Config to yuv
  - Allocate buffur for yuv
- Codec
  - Find Codec
  - Create context for codec
  - Config parameter for codec
  - Open codec context 
  - Encode
- Muxing
  - Create IO context
  - Add vedio stream
  - Copy paramter from codec
- RTMP IO
  - Open RTMP network IO
  - Write the stream header to an output media file


## Audio2RTMP
Get audio from microphone  with Qt and upstreaming according to RTMP protocol

### Implement the following function
- Record audio
- Audio resample
  - Init context for resample
  - Audio ouput allocate
  - Resample
- Codec
  - Init audio codec
  - Int context for codec
  - Config audio context 
  - Open audio codec
  - pts calculation
  - Encode
- Muxing
  - Init Context
  - Add a new stream to a media file.
  - Copy parameter from codec
- RTMP IO
  - Open rtmp IO
  - Allocate the stream private data and write the stream header to an output media file
  - Upstreaming

## MultiMedia2RTMP

Implement AudioRecord, VideoCapture, MediaEncode Factory and RTMP module. Upstreaming video and audio simultaneously and finish synchronized.

- AudioRecord Factory
  - Finish a concrete factory with Qt
- VideoCapture Factory
  - Finish a concrete factory with opencv
- MediaEncode Factory
  - Finish a concrete factory with FFMPEG
- RTMP module
  -  Implement muxing and rtmp function with FFMPEG