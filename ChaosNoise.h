#pragma once

#include <stdint.h>
#include <cmath>

constexpr int32_t kHlskChaosNoisePhsMax = 0x1000000L;
constexpr int32_t kHlskChaosNoisePhsMsk = 0x0FFFFFFL;

/**
 * @brief A noise generator that uses a chaos function to produce sound.
 *        Ported from https://pbat.ch/sndkit/chaosnoise/
 */
class ChaosNoise
{
public:
    ChaosNoise() {}
    ~ChaosNoise() {}

    /**
     * @param sampleRate
     * @param init Initial value, must be between 0 and 1
     */
    void Init(float sampleRate, float init = 0.f)
    {
        y_[0] = init;
        y_[1] = 0;

        phs_ = 0;

        maxlens_ = kHlskChaosNoisePhsMax / sampleRate;

        SetChaos(1.5f);
        SetFreq(8000.f);
    }

    /**
     * @param chaos Usually between 1 and 2, but between 0.1 and 0.3 makes
     *              for an interesting oscillator :)
     */
    void SetChaos(float chaos)
    {
        chaos_ = chaos;
    }

    float GetFreq()
    {
        return freq_;
    }

    void SetFreq(float freq)
    {
        freq_ = freq;
    }

    float noise()
    {
        phs_ += floor(freq_ * maxlens_);

        if (phs_ >= kHlskChaosNoisePhsMax) {
            float y;

            phs_ &= kHlskChaosNoisePhsMsk;
            y = fabs(chaos_ * y_[0] - y_[1] - 0.05f);
            y_[1] = y_[0];
            y_[0] = y;
        }

        return y_[0];
    }

    void process(AudioBuffer &buffer)
    {
        float n = noise();
        buffer.add(n);
    }

    float Process()
    {
        return noise();
    }

private:
    float y_[2];
    float maxlens_, chaos_, freq_;
    int32_t phs_;
};
