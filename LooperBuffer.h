#pragma once

#include "Commons.h"
#include "EnvFollower.h"
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
    enum WriteStatus
    {
        WRITE_STATUS_INACTIVE,
        WRITE_STATUS_FADE_IN,
        WRITE_STATUS_FADE_OUT,
        WRITE_STATUS_ACTIVE,
    };

    FloatArray* buffer_;

    WriteStatus status_;

    uint32_t index_;
    uint32_t start_;
    uint32_t end_;

    int fadeIndex_;

    bool doFade_;

public:
    WriteHead(FloatArray* buffer)
    {
        buffer_ = buffer;

        status_ = WRITE_STATUS_INACTIVE;

        fadeIndex_ = 0;

        doFade_ = false;
    }
    ~WriteHead() {}

    static WriteHead* create(FloatArray* buffer)
    {
        return new WriteHead(buffer);
    }

    static void destroy(WriteHead* obj)
    {
        delete obj;
    }

    inline bool IsWriting()
    {
        return WRITE_STATUS_INACTIVE != status_;
    }

    inline void Start()
    {
        if (WRITE_STATUS_INACTIVE == status_)
        {
            status_ = WRITE_STATUS_FADE_IN;
            doFade_ = true;
            fadeIndex_ = 0;
        }
    }

    inline void Stop()
    {
        if (WRITE_STATUS_ACTIVE == status_)
        {
            status_ = WRITE_STATUS_FADE_OUT;
            doFade_ = true;
            fadeIndex_ = 0;
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

        if (doFade_)
        {
            float x = fadeIndex_ * kLooperFadeSamplesR;
            if (WRITE_STATUS_FADE_IN == status_)
            {
                x = 1.f - x;
            }
            fadeIndex_++;
            if (fadeIndex_ == kLooperFadeSamples)
            {
                x = WRITE_STATUS_FADE_OUT == status_;
                doFade_ = false;
                status_ = (WRITE_STATUS_FADE_IN == status_ ? WRITE_STATUS_ACTIVE : WRITE_STATUS_INACTIVE);
            }
            value = CheapEqualPowerCrossFade(value, buffer_->getElement(position), x);
        }

        if (WRITE_STATUS_INACTIVE != status_)
        {
            buffer_->setElement(position, value);
        }
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
        writeHeads_[LEFT_CHANNEL]->Write(i, left);
        writeHeads_[RIGHT_CHANNEL]->Write(i + kLooperChannelBufferLength, right);
    }

    inline bool IsRecording()
    {
        return writeHeads_[LEFT_CHANNEL]->IsWriting() && writeHeads_[RIGHT_CHANNEL]->IsWriting();
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

    inline float ReadLeft(int32_t position)
    {
        while (position >= kLooperChannelBufferLength)
        {
            position -= kLooperChannelBufferLength;
        }
        while (position < 0)
        {
            position += kLooperChannelBufferLength;
        }

        return buffer_[position];
    }

    inline float ReadRight(int32_t position)
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

        return buffer_[position];
    }

    inline void Read(float p, float &left, float &right, PlaybackDirection direction = PLAYBACK_FORWARD)
    {
        float l0, l1;
        float r0, r1;

        int32_t i = int32_t(p);

        float f = p - i;

        l0 = ReadLeft(i);
        l1 = ReadLeft(i + direction);
        left = l0 + direction * (l1 - l0) * f;

        r0 = ReadRight(i);
        r1 = ReadRight(i + direction);
        right = r0 + direction * (r1 - r0) * f;
    }
};