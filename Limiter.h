#pragma once

#include "Commons.h"

class Limiter
{
private:
    float peak_;

public:
    Limiter(float peak)
    {
        peak_ = peak;
    }
    ~Limiter() { }

    static Limiter* create(float peak = 0.5f)
    {
        return new Limiter(peak);
    }

    static void destroy(Limiter* obj)
    {
        delete obj;
    }

    // Ported and adapted from Emilie Gillet's Limiter in Rings.
    void Process(AudioBuffer& input, AudioBuffer& output, float preGain = 1.f)
    {
        for (size_t i = 0; i < output.getSize(); i++)
        {
            float l_pre = input.getSamples(LEFT_CHANNEL).getElement(i) * preGain;
            float r_pre = input.getSamples(RIGHT_CHANNEL).getElement(i) * preGain;

            float l_peak = fabs(l_pre);
            float r_peak = fabs(r_pre);
            float s_peak = fabs(r_pre - l_pre);
            float peak = Max(Max(l_peak, r_peak), s_peak);
            SLOPE(peak_, peak, 0.05f, 0.00002f);
            // Clamp to 8Vpp, clipping softly towards 10Vpp
            float gain = (peak_ <= 1.0f ? 1.0f : 1.0f / peak_);

            output.getSamples(LEFT_CHANNEL).setElement(i, SoftLimit(l_pre * gain * 0.8f));
            output.getSamples(RIGHT_CHANNEL).setElement(i, SoftLimit(r_pre * gain * 0.8f));
        }
    }

    void ProcessSoft(AudioBuffer& input, AudioBuffer& output)
    {
        for (size_t i = 0; i < output.getSize(); i++)
        {
            output.getSamples(LEFT_CHANNEL).setElement(i, SoftLimit(input.getSamples(LEFT_CHANNEL).getElement(i)));
            output.getSamples(RIGHT_CHANNEL).setElement(i, SoftLimit(input.getSamples(RIGHT_CHANNEL).getElement(i)));
        }
    }
};
