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
        int i = writeIndex_ - index;
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

    inline void write(float value, int stride = 1)
    {
        writeIndex_ += stride;
        if  (writeIndex_ >= size_)
        {
            writeIndex_ -= size_;
        }
        buffer_[writeIndex_] = value; //dc_->process(value);
    }
};