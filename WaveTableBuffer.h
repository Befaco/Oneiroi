#pragma once

#include "Commons.h"
#include "Interpolator.h"

class WaveTableBuffer
{
private:
    FloatArray* buffer_;
    int writeHead_;

public:
    WaveTableBuffer(FloatArray* buffer)
    {
        buffer_ = buffer;
        writeHead_ = 0;
    }
    ~WaveTableBuffer() {}

    static WaveTableBuffer* create(FloatArray* buffer)
    {
        return new WaveTableBuffer(buffer);
    }

    static void destroy(WaveTableBuffer* obj)
    {
        delete obj;
    }

    inline float ReadLeft(uint32_t position)
    {
        while (position >= kLooperChannelBufferLength)
        {
            position -= kLooperChannelBufferLength;
        }
        while (position < 0)
        {
            position += kLooperChannelBufferLength;
        }

        return buffer_->getElement(position);
    }

    inline float ReadRight(uint32_t position)
    {
        position += kLooperChannelBufferLength;

        while (position >= kLooperTotalBufferLength)
        {
            position -= kLooperChannelBufferLength;
        }
        while (position < kLooperChannelBufferLength)
        {
            position += kLooperChannelBufferLength;
        }

        return buffer_->getElement(position);
    }

    inline void ReadLinear(float p1, float p2, float x, float &left, float &right)
    {
        float l0, l1;
        float r0, r1;

        uint32_t i1 = uint32_t(p1);
        uint32_t i2 = uint32_t(p2);
        float f1 = p1 - i1;
        float f2 = p2 - i2;

        float x0 = 1.f - x;

        left = Interpolator::linear(ReadLeft(i1), ReadLeft(i1 + 1), f1) * x0 + Interpolator::linear(ReadLeft(i2), ReadLeft(i2 + 1), f2) * x;
        right = Interpolator::linear(ReadRight(i1), ReadRight(i1 + 1), f1) * x0 + Interpolator::linear(ReadRight(i2), ReadRight(i2 + 1), f2) * x;
    }
};
