#pragma once
class Data
{
public:
    Data();
    Data(char *data, int size);
    virtual ~Data();

    void Release();
    char *data = 0;
    int size = 0;
};

