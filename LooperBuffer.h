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

    const uint32_t kLooperTotalBufferLength;
    const uint32_t kLooperClearBlockSize = kLooperTotalBufferLength / kLooperClearBlocks;
    const uint32_t kLooperClearBlockTypeSize = kLooperClearBlockSize * 4; // Float
    const uint32_t kLooperFadeSamples;
    const float kLooperFadeSamplesR;
    int writeFadeIndex_;
    bool writeFadeIn_, writeFadeOut_;

public:
    LooperBuffer(float sampleRate) : 
        kLooperTotalBufferLength(sampleRate * kLooperTotalBufferLengthSeconds), 
        kLooperFadeSamples(sampleRate * kLooperFadeSeconds),
        kLooperFadeSamplesR(1.f / kLooperFadeSamples)
    {
        buffer_ = FloatArray::create(kLooperTotalBufferLength);
        buffer_.noise();
        buffer_.multiply(kLooperNoiseLevel); // Tame the noise a bit

        clearBlock_ = buffer_.getData();

        writeHead_ = 0;
        writeFadeIndex_ = 0;
        writeFadeIn_ = false;
        writeFadeOut_ = false;

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

    inline void WriteAt(uint32_t position, float value, float level = 1.f)
    {
        while (position >= kLooperTotalBufferLength)
        {
            position -= kLooperTotalBufferLength;
        }
        while (position < 0)
        {
            position += kLooperTotalBufferLength;
        }

        if (level == 1.f)
        {
            buffer_[position] = dc_[position % 2]->process(value);
        }
        else
        {
            buffer_[position] = dc_[position % 2]->process(value) * level + buffer_[position] * (1.f - level);
        }
    }

    inline void Write(uint32_t i, float left, float right, float level = 1.f)
    {
        i *= 2;

        if (writeFadeIn_ || writeFadeOut_)
        {
            level = writeFadeIndex_ * kLooperFadeSamplesR;
            if (writeFadeOut_)
            {
                level = 1.f - level;
            }
            writeFadeIndex_++;
            if (writeFadeIndex_ >= kLooperFadeSamples)
            {
                if (writeFadeIn_)
                {
                    level = 1.f;
                    writeFadeIn_ = false;
                }
                else
                {
                    level = 0.f;
                    writeFadeOut_ = false;
                }
            }
        }

        WriteAt(i, left, level);
        WriteAt(i + 1, right, level);
    }

    inline bool IsFadingOutWrite()
    {
        return writeFadeOut_;
    }

    inline void FadeInWrite()
    {
        writeFadeIn_ = true;
        writeFadeOut_ = false;
        writeFadeIndex_ = 0;
    }

    inline void FadeOutWrite()
    {
        writeFadeOut_ = true;
        writeFadeIn_ = false;
        writeFadeIndex_ = 0;
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
    inline void Read(float p1, float p2, float x, float &left, float &right, PlaybackDirection direction = PLAYBACK_FORWARD)
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