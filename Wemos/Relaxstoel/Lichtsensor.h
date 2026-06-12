#pragma once

class Lichtsensor {
public:
    virtual void begin()     = 0;
    virtual int  getWaarde() = 0;
    virtual ~Lichtsensor() {}
};
