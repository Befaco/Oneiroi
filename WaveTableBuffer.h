#pragma once

#include "Commons.h"

class WaveTableBuffer
{
private:
    FloatArray* buffer_;
    int writeHead_;
    const int32_t kLooperTotalBufferLength;
public:
    WaveTableBuffer(FloatArray* buffer, float sampleRate) : kLooperTotalBufferLength(sampleRate * kLooperTotalBufferLengthSeconds)

    {
        buffer_ = buffer;
        writeHead_ = 0;
    }
    ~WaveTableBuffer() {}

    static WaveTableBuffer* create(FloatArray* buffer, float sampleRate)
    {
        return new WaveTableBuffer(buffer, sampleRate);
    }

    static void destroy(WaveTableBuffer* obj)
    {
        delete obj;
    }

    inline float ReadAt(int position)
    {
        while (position >= kLooperTotalBufferLength)
        {
            position -= kLooperTotalBufferLength;
        }
        while (position < 0)
        {
            position += kLooperTotalBufferLength;
        }

        return buffer_->getElement(position);
    }

    // Interleaved reading.
    inline void ReadLinear(float p1, float p2, float x, float &left, float &right)
    {
        float l0, l1;
        float r0, r1;

        uint32_t i1 = uint32_t(p1);
        uint32_t i2 = uint32_t(p2);
        float f1 = p1 - i1;
        float f2 = p2 - i2;

        i1 *= 2;
        i2 *= 2;

        float x0 = 1.f - x;

        l0 = ReadAt(i1) * x0 + ReadAt(i2) * x;
        l1 = ReadAt(i1 + 2) * x0 + ReadAt(i2 + 2) * x;
        left = l0 + (l1 - l0) * f1;

        r0 = ReadAt(i1 + 1) * x0 + ReadAt(i2 + 1) * x;
        r1 = ReadAt(i1 + 3) * x0 + ReadAt(i2 + 3) * x;
        right = r0 + (r1 - r0) * f2;
    }
};
