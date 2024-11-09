#pragma once

#include "Commons.h"
#include "Interpolator.h"
#include <stdint.h>

class DelayLine
{
private:
    FloatArray buffer_;
    uint32_t size_, writeIndex_, delay_;

public:
    DelayLine(uint32_t size)
    {
        size_ = size;
        buffer_ = FloatArray::create(size_);
        delay_ = size_ - 1;
        writeIndex_ = 0;
    }
    ~DelayLine()
    {
        FloatArray::destroy(buffer_);
    }

    static DelayLine* create(uint32_t size)
    {
        return new DelayLine(size);
    }

    static void destroy(DelayLine* line)
    {
        delete line;
    }

    void clear()
    {
        buffer_.clear();
    }

    void setDelay(uint32_t delay)
    {
        delay_ = delay;
    }

    inline float readAt(int index)
    {
        int i = writeIndex_ - index - 1;
        if (i < 0)
        {
            i += size_;
        }

        return Clamp(buffer_[i], -3.f, 3.f);
    }

    inline float read(float index)
    {
        size_t idx = (size_t)index;
        float y0 = readAt(idx);
        float y1 = readAt(idx + 1);
        float frac = index - idx;

        return Interpolator::linear(y0, y1, frac);
    }

    inline float read(float index1, float index2, float x)
    {
        float v = read(index1);
        if (x == 0)
        {
            return v;
        }

        return v * (1.f - x) + read(index2) * x;
    }

    inline void write(float value, int stride = 1)
    {
        buffer_[writeIndex_] = value;
        writeIndex_ += stride;
        if  (writeIndex_ >= size_)
        {
            writeIndex_ -= size_;
        }
    }
};