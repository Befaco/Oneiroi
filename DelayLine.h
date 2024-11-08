#pragma once

#include "Commons.h"
#include "Interpolator.h"
#include "DcBlockingFilter.h"
#include <stdint.h>

class DelayLine
{
private:
    FloatArray buffer_;
    uint32_t size_, writeIndex_, delay_;
    DcBlockingFilter* dc_;

public:
    DelayLine(uint32_t size)
    {
        size_ = size;
        buffer_ = FloatArray::create(size_);
        dc_ = DcBlockingFilter::create();
        delay_ = size_ - 1;
        writeIndex_ = 0;
    }
    ~DelayLine()
    {
        FloatArray::destroy(buffer_);
        DcBlockingFilter::destroy(dc_);
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
        uint32_t idx1 = (uint32_t)index1;
        float frac1 = index1 - idx1;
        float a0 = readAt(idx1);
        float a1 = readAt(idx1 + 1);

        float v = Interpolator::linear(a0, a1, frac1);
        if (x == 0)
        {
            return v;
        }

        uint32_t idx2 = (uint32_t)index2;
        float frac2 = index2 - idx2;
        float b0 = readAt(idx2);
        float b1 = readAt(idx2 + 1);

        return v * (1.f - x) + Interpolator::linear(b0, b1, frac2) * x;
    }

    inline void write(float value, int stride = 1)
    {
        buffer_[writeIndex_] = value; //dc_->process(value);
        writeIndex_ += stride;
        if  (writeIndex_ >= size_)
        {
            writeIndex_ -= size_;
        }
    }
};