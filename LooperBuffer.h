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

class WriteHead
{
private:
    enum FadeDirection
    {
        FADE_IN,
        FADE_OUT,
        FADE_NONE
    };

    FloatArray* buffer_;
    DcBlockingFilter* dc_;

    FadeDirection fadeDirection_;

    int fadeIndex_;
    int lastSign_;
    bool findZeroCrossing_;
    bool doFade_;
    bool active_;

public:
    WriteHead(FloatArray* buffer)
    {
        buffer_ = buffer;

        dc_ = DcBlockingFilter::create();

        fadeDirection_ = FADE_NONE;
        fadeIndex_ = 0;
        lastSign_ = 0;
        findZeroCrossing_ = false;
        doFade_ = false;
        active_ = false;
    }
    ~WriteHead()
    {
        DcBlockingFilter::destroy(dc_);
    }

    static WriteHead* create(FloatArray* buffer)
    {
        return new WriteHead(buffer);
    }

    static void destroy(WriteHead* obj)
    {
        delete obj;
    }

    inline bool IsActive()
    {
        return active_;
    }

    inline void Start()
    {
        if (!active_ && FADE_NONE == fadeDirection_)
        {
            fadeDirection_ = FADE_IN;
            findZeroCrossing_ = true;
        }
    }

    inline void Stop()
    {
        if (active_ && FADE_NONE == fadeDirection_)
        {
            fadeDirection_ = FADE_OUT;
            findZeroCrossing_ = true;
        }
    }

    inline void Write(uint32_t position, float value)
    {
        while (position >= kLooperTotalBufferLength)
        {
            position -= kLooperTotalBufferLength;
        }
        while (position < 0)
        {
            position += kLooperTotalBufferLength;
        }

        if (findZeroCrossing_ )
        {
            float v = buffer_->getElement(position);
            int s = Sign(v);
            if (v == 0 || s != lastSign_)
            {
                findZeroCrossing_ = false;
                doFade_ = true;
            }
            lastSign_ = s;
        }

        if (doFade_)
        {
            float level = fadeIndex_ * kLooperFadeSamplesR;
            if (FADE_OUT == fadeDirection_)
            {
                level = 1.f - level;
            }
            fadeIndex_++;
            if (fadeIndex_ == kLooperFadeSamples)
            {
                level = active_ = FADE_IN == fadeDirection_;
                fadeDirection_ = FADE_NONE;
                doFade_ = false;
                lastSign_ = 0;
                fadeIndex_ = 0;
            }
            value = value * level + buffer_->getElement(position) * (1.f - level);
        }

        buffer_->setElement(position, dc_->process(value));
    }
};

class LooperBuffer
{
private:
    FloatArray buffer_;

    float* clearBlock_;

    WriteHead* writeHeads_[2];

public:
    LooperBuffer()
    {
        buffer_ = FloatArray::create(kLooperTotalBufferLength);
        buffer_.noise();
        buffer_.multiply(kLooperNoiseLevel); // Tame the noise a bit

        clearBlock_ = buffer_.getData();

        for (size_t i = 0; i < 2; i++)
        {
            writeHeads_[i] = WriteHead::create(&buffer_);
        }
    }
    ~LooperBuffer()
    {
        for (size_t i = 0; i < 2; i++)
        {
            WriteHead::destroy(writeHeads_[i]);
        }
    }

    static LooperBuffer* create()
    {
        return new LooperBuffer();
    }

    static void destroy(LooperBuffer* obj)
    {
        delete obj;
    }

    FloatArray* GetBuffer()
    {
        return &buffer_;
    }

    inline bool Clear()
    {
        if (clearBlock_ == buffer_.getData() + kLooperTotalBufferLength)
        {
            clearBlock_ =  buffer_.getData();

            return true;
        }

        memset(clearBlock_, 0, kLooperClearBlockTypeSize);
        clearBlock_ += kLooperClearBlockSize;

        return false;
    }

    inline void Write(uint32_t i, float left, float right)
    {
        i *= 2; // Interleaved

        writeHeads_[LEFT_CHANNEL]->Write(i, left);
        writeHeads_[RIGHT_CHANNEL]->Write(i + 1, right);
    }

    inline bool IsRecording()
    {
        return writeHeads_[LEFT_CHANNEL]->IsActive() || writeHeads_[RIGHT_CHANNEL]->IsActive();
    }

    inline void StartRecording()
    {
        writeHeads_[LEFT_CHANNEL]->Start();
        writeHeads_[RIGHT_CHANNEL]->Start();
    }

    inline void StopRecording()
    {
        writeHeads_[LEFT_CHANNEL]->Stop();
        writeHeads_[RIGHT_CHANNEL]->Stop();
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
    inline void Read(float p, float &left, float &right, PlaybackDirection direction = PLAYBACK_FORWARD)
    {
        float l0, l1;
        float r0, r1;

        uint32_t i = uint32_t(p);

        float f = p - i;

        i *= 2;

        float d2 = 2 * direction;
        float d3 = 3 * direction;

        l0 = ReadAt(i);
        l1 = ReadAt(i + d2);
        left = l0 + direction * (l1 - l0) * f;

        r0 = ReadAt(i + direction);
        r1 = ReadAt(i + d3);
        right = r0 + direction * (r1 - r0) * f;
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