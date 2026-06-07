#pragma once

class Lamp {
public:
    virtual void begin()  = 0;
    virtual void setAan() = 0;
    virtual void setUit() = 0;
    virtual ~Lamp() {}
};
