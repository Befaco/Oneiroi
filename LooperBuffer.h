#pragma once

#include "Commons.h"
#include "Interpolator.h"
#include "EnvFollower.h"
#include "DcBlockingFilter.h"
#include <algorithm>

enum PlaybackDirection
{
    PLAYBACK_STALLED,
    PLAYBACK_FORWARD,
    PLAYBACK_BACKWARDS = -1
};

class LooperBuffer
{
private:
    FloatArray buffer_;

    int32_t writeHead_;

    float* clearBlock_;

    DcBlockingFilter* dc_[2];

    const int32_t kLooperTotalBufferLength;
    const int32_t kLooperClearBlockSize = kLooperTotalBufferLength / kLooperClearBlocks;
    const int32_t kLooperClearBlockTypeSize = kLooperClearBlockSize * 4; // Float
public:
    LooperBuffer(float sampleRate) : 
        kLooperTotalBufferLength(sampleRate * kLooperTotalBufferLengthSeconds)
    {
        buffer_ = FloatArray::create(kLooperTotalBufferLength);
        buffer_.noise();
        buffer_.multiply(0.5f); // Tame the noise a bit

        clearBlock_ = buffer_.getData();

        writeHead_ = 0;

        for (size_t i = 0; i < 2; i++)
        {
            dc_[i] = DcBlockingFilter::create();
        }
    }
    ~LooperBuffer()
    {
        for (size_t i = 0; i < 2; i++)
        {
            DcBlockingFilter::destroy(dc_[i]);
        }
    }

    static LooperBuffer* create(float sampleRate)
    {
        return new LooperBuffer(sampleRate);
    }

    static void destroy(LooperBuffer* obj)
    {
        delete obj;
    }

    FloatArray* GetBuffer()
    {
        return &buffer_;
    }

    bool Clear()
    {
        // VCV change
        memset(buffer_.getData(), 0, kLooperTotalBufferLength * sizeof(float));
        return true;

        if (clearBlock_ == buffer_.getData() + kLooperTotalBufferLength)
        {
            clearBlock_ =  buffer_.getData();

            return true;
        }

        memset(clearBlock_, 0, kLooperClearBlockTypeSize);
        clearBlock_ += kLooperClearBlockSize;

        return false;
    }

    inline void WriteAt(int position, float value)
    {
        while (position >= kLooperTotalBufferLength)
        {
            position -= kLooperTotalBufferLength;
        }
        while (position < 0)
        {
            position += kLooperTotalBufferLength;
        }

        buffer_[position] = dc_[position % 2]->process(value);
    }

    inline void WriteLinear(float p, float left, float right, PlaybackDirection direction = PLAYBACK_FORWARD)
    {
        uint32_t i = uint32_t(p);
        float f = p - i;

        i *= 2;

        WriteAt(i, left * (1.f - f));
        WriteAt(i + 2 * direction, left * f);

        WriteAt(i + direction, right *  (1.f - f));
        WriteAt(i + 3 * direction, right * f);
    }

    inline void Write(float p, float left, float right)
    {
        uint32_t i = uint32_t(p);

        i *= 2;

        WriteAt(i, left);
        WriteAt(i + 1, right);
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

        return buffer_[position];
    }

    // Interleaved reading.
    inline void ReadLinear(float p1, float p2, float x, float &left, float &right, PlaybackDirection direction = PLAYBACK_FORWARD)
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
        float d2 = 2 * direction;
        float d3 = 3 * direction;

        l0 = ReadAt(i1) * x0 + ReadAt(i2) * x;
        l1 = ReadAt(i1 + d2) * x0 + ReadAt(i2 + d2) * x;
        left = l0 + direction * (l1 - l0) * f1;

        r0 = ReadAt(i1 + direction) * x0 + ReadAt(i2 + direction) * x;
        r1 = ReadAt(i1 + d3) * x0 + ReadAt(i2 + d3) * x;
        right = r0 + direction * (r1 - r0) * f2;
    }
};