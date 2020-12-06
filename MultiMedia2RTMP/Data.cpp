#include "Data.h"
#include <stdlib.h>
#include <string.h>

Data::Data()
{
}

Data::Data(char * data, int size)
{
    this->data = new char[size];
    memcpy(this->data, data, size);
    this->size = size;
}


Data::~Data()
{
}

void Data::Release()
{
    if (data)
        delete data;
    data = 0;
    size = 0;
}
