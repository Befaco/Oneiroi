#pragma once

#include "Commons.h"
#include "DcBlockingFilter.h"

class WaveTableBuffer
{
private:
    FloatArray buffer_;
    DcBlockingFilter* dc_[2];
    int currentTable_, writeHead_;

    float* clearBlock_;

public:
    WaveTableBuffer()
    {
        buffer_ = FloatArray::create(kWaveTableBufferLength);
        for (size_t i = 0; i < 2; i++)
        {
            dc_[i] = DcBlockingFilter::create();
        }

        clearBlock_ = buffer_.getData();

        Reset();
    };
    ~WaveTableBuffer()
    {
        FloatArray::destroy(buffer_);
        for (size_t i = 0; i < 2; i++)
        {
            DcBlockingFilter::destroy(dc_[i]);
        }
    };

    static WaveTableBuffer* create()
    {
        return new WaveTableBuffer();
    }

    static void destroy(WaveTableBuffer* obj)
    {
        delete obj;
    }

    FloatArray *GetBuffer()
    {
        return &buffer_;
    }

    void Init(FloatArray buffer)
    {
        buffer_.copyFrom(buffer.subArray(0, kWaveTableBufferLength));

        // Multiply by 2 because the noise in the looper buffer is recorded at
        // half volume.
        buffer_.multiply(2.f);
    }

    bool Clear()
    {
        if (clearBlock_ == buffer_.getData() + kWaveTableBufferLength)
        {
            clearBlock_ =  buffer_.getData();

            return true;
        }

        memset(clearBlock_, 0, kWaveTableClearBlockTypeSize);
        clearBlock_ += kWaveTableClearBlockSize;

        return false;
    }

    void Reset()
    {
        currentTable_ = 0;
        writeHead_ = 0;
    }

    float ReadAt(int position)
    {
        if (position < 0) position += kWaveTableBufferLength;
        if (position >= kWaveTableBufferLength) position -= kWaveTableBufferLength;

        return buffer_[position];
    }

    float ReadPos(float p)
    {
        int i = int(p);
        float f = p - i;
        i *= 2; // Interleaved

        float l0 = ReadAt(i);
        float l1 = ReadAt(i + 2);

        return l0 + (l1 - l0) * f;
        /*
        float l2 = ReadAt(i + 4);
        float l3 = ReadAt(i + 6);

        float c = (l2 - l0) * 0.5f;
        float v = l1 - l2;
        float w = c + v;
        float a = w + v + (l3 - l1) * 0.5f;
        float b_neg = w + a;

        return (((a * f) - b_neg) * f + c) * f + l1;
        */
    }

    void Read(float p1, float p2, float x, float &left, float &right)
    {
        float left1 = ReadPos(p1);
        float right1 = ReadPos(p1 + 1);
        float left2 = ReadPos(p2);
        float right2 = ReadPos(p2 + 1);

        float ix = 1.f - x;

        left = left1 * ix + left2 * x;
        right = right1 * ix + right2 * x;
    }

    inline void WriteAt(int position, float left, float right)
    {
        while (position >= kWaveTableBufferLength)
        {
            position -= kWaveTableBufferLength;
        }
        while (position < 0)
        {
            position += kWaveTableBufferLength;
        }

        buffer_.setElement(position, left);
        buffer_.setElement(position + 1, right);
    }

    void Write(float left, float right)
    {
        float l = dc_[LEFT_CHANNEL]->process(left);
        float r = dc_[RIGHT_CHANNEL]->process(right);
        buffer_.setElement(writeHead_, l);
        buffer_.setElement(writeHead_ + 1, r);
        writeHead_ += 2; // Interleaved
        if (writeHead_ >= kWaveTableBufferLength)
        {
            writeHead_ -= kWaveTableBufferLength;
        }
    }

    inline void WriteLinear(float p, float left, float right)
    {
        uint32_t i = uint32_t(p);
        float f = p - i;

        i *= 2;

        //WriteAt(i, left * (1.f - f));
        //WriteAt(i + 2, left * f);

        //WriteAt(i, right *  (1.f - f));
        //WriteAt(i + 3, right * f);
    }
};
