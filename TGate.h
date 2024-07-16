#pragma once

#include <stdint.h>

/**
 * @brief A triggerable gate generator. Takes in a trigger signal, and
 *        produces a gate whose duration is measured in seconds.
 *        Ported from https://pbat.ch/sndkit/tgate/
 */
class TGate
{
public:
    TGate() {}
    ~TGate() {}

    void Init(float sampleRate)
    {
        sr_ = sampleRate;
        timer_ = 0;
        SetDuration(0.002f);
    }

    /**
     * @param dur In seconds
     */
    void SetDuration(float dur)
    {
        dur_ = dur;
    }

    float Process(float trig)
    {
        float out = 0;

        if (trig != 0)
        {
            timer_ = dur_ * sr_;
        }

        if (timer_ != 0)
        {
            out = 1.0f;
            timer_--;
        }

        return out;
    }

private:
    uint32_t timer_;
    float dur_;
    float sr_;
};