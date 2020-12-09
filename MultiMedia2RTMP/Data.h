#pragma once
class Data
{
public:

    Data();
    Data(char *data, int size, long long pts =0);
    virtual ~Data();

    void Release();
    char *data = 0;
    int size = 0;
    long long pts = 0;
};
long long GetCurTime();
