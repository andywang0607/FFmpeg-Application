#pragma once
#include "DataManager.h"

class VideoCaptureFactory : public DataManager
{
public:
    static VideoCaptureFactory *Get(unsigned char index = 0);
    virtual bool Init(int camIndex = 0) = 0;
    virtual void Stop() = 0;

    virtual ~VideoCaptureFactory();

    int width = 0;
    int height = 0;
    int fps = 0;
protected:
    VideoCaptureFactory();   
};

