#include "VideoCaptureFactory.h"
#include <opencv2/highgui.hpp>
#include <iostream>

#pragma comment(lib, "opencv_world.lib")

using namespace std;
using namespace cv;

class CVideoCapture :public VideoCaptureFactory
{
public:
    VideoCapture cam;

    void run()
    {
        Mat frame;
        while (!isExit)
        {
            if (!cam.read(frame))
            {
                msleep(1);
                continue;
            }
            if (frame.empty())
            {
                msleep(1);
                continue;
            }
            Data d((char*)frame.data, frame.cols*frame.rows*frame.elemSize());
            Push(d);
        }
    }

    bool Init(int camIndex = 0)
    {
        cam.open(camIndex);
        if (!cam.isOpened())
        {
            cout << "cam open failed!" << endl;
            return false;
        }
        cout << camIndex << " cam open success" << endl;
        cam.set(CV_CAP_PROP_FPS, 30);
        width = cam.get(CAP_PROP_FRAME_WIDTH);
        height = cam.get(CAP_PROP_FRAME_HEIGHT);
        fps = cam.get(CAP_PROP_FPS);
        if (fps == 0) fps = 30;
        return true;
    }

    void Stop()
    {
        DataManager::Stop();
        if (cam.isOpened())
        {
            cam.release();
        }
    }
};

VideoCaptureFactory::VideoCaptureFactory()
{
}


VideoCaptureFactory::~VideoCaptureFactory()
{
}

VideoCaptureFactory *VideoCaptureFactory::Get(unsigned char index)
{
    static CVideoCapture instances[255];
    return &instances[index];
}