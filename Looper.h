#pragma once

#include "Commons.h"
#include "LooperBuffer.h"
#include "WaveTableBuffer.h"
#include "SquareWaveOscillator.h"
#include "Schmitt.h"
#include "DjFilter.h"
#include "Limiter.h"
#include "EnvFollower.h"
#include <stdint.h>
#include <cmath>

//#define USE_WRITE_VARISPEED

class Looper
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;
    WaveTableBuffer *wtBuffer_;
    LooperBuffer* buffer_;
    DjFilter* filter_;
    Limiter* limiter_;
    EnvFollower* ef_[2];

    AudioBuffer* sosOut_;
    AudioBuffer* filterOut_;

    PlaybackDirection direction_;

    float bufferPhase_, wPhase_;
    float phase_, newPhase_;
    float speed_, newSpeed_;
    float speedValue_;
    float inputGain_, feedback_, fadeVolume_, speedVolume_;
    float filterValue_;
    float xi_;

    bool triggered_;
    bool boc_;
    bool cleared_;
    bool looping_;

    bool fadeOut_;
    bool fadeIn_;

    uint32_t length_, start_, end_;
    uint32_t newLength_, newStart_, newEnd_;

    Schmitt trigger_;

    Lut<int, 32> startLUT_;
    Lut<int, 128> lengthLUT_;

    const int kLooperFadeSamples;
    const int kLooperFadeSamplesTwice;
    const float kLooperFadeInc;

    void FadeOut()
    {
        fadeVolume_ -= kLooperFadeInc;
        if (fadeVolume_ <= 0)
        {
            fadeVolume_ = 0;
            fadeOut_ = false;
            fadeIn_ = true;
        }
    }

    void FadeIn()
    {
        fadeVolume_ += kLooperFadeInc;
        if (fadeVolume_ >= 1)
        {
            fadeVolume_ = 1;
            fadeIn_ = false;
        }
    }

    void MapSpeed()
    {
        speedValue_ = CenterMap(patchCtrls_->looperSpeed, -2.f, 2.f, patchState_->speedZero);

        // Deadband around 0x speed.
        if (speedValue_ >= -0.1f && speedValue_ <= 0.1f)
        {
            patchState_->looperSpeedLockFlag = true;
        }
        else if (speedValue_ > 0.1f)
        {
            // Deadband around 1x speed.
            if (speedValue_ >= 0.95f && speedValue_ <= 1.05f)
            {
                patchState_->looperSpeedLockFlag = true;
            }
            else
            {
                patchState_->looperSpeedLockFlag = false;
            }
        }
        else
        {
            // Deadband around -1x speed.
            if (speedValue_ >= -1.05f && speedValue_ <= -0.95f)
            {
                patchState_->looperSpeedLockFlag = true;
            }
            else
            {
                patchState_->looperSpeedLockFlag = false;
            }
        }
    }

    void SetSpeed(float value)
    {
        /*
        if (ClockSource::CLOCK_SOURCE_EXTERNAL == patchState_->clockSource)
        {
            // Quantized to quarters of speed.
            value = Quantize(value, 16);
        }
        */

        // Lower the volume as the speed approaches 0.
        speedVolume_ = 1.f;
        if (value >= -0.2f && value <= 0.2f)
        {
            speedVolume_ = MapExpo(std::abs(value), 0.f, 0.2f, 0.f, 1.f);
        }

        // Deadband around 0x speed.
        if (value >= -0.1f && value <= 0.1f)
        {
            direction_ = PlaybackDirection::PLAYBACK_STALLED;
            value = 0;
        }
        else if (value > 0.1f)
        {
            direction_ = PlaybackDirection::PLAYBACK_FORWARD;
            // Deadband around 1x speed.
            if (value >= 0.95f && value <= 1.05f)
            {
                value = 1.f;
            }
        }
        else
        {
            direction_ = PlaybackDirection::PLAYBACK_BACKWARDS;
            // Deadband around -1x speed.
            if (value >= -1.05f && value <= -0.95f)
            {
                value = -1.f;
            }
        }

        newSpeed_ = value;
    }

    void SetEnd()
    {
        newEnd_ = newStart_ + newLength_;
        if (newEnd_ >= kLooperChannelBufferLength)
        {
            newEnd_ -= kLooperChannelBufferLength;
        }
        //looping_ = (newStart_ > 0) && (newLength_ < kLooperChannelBufferLength -1);
        looping_ = newLength_ < kLooperChannelBufferLength -1;
    }

    void SetStart(float value)
    {
        if (value > 0.99f)
        {
            value = 1.f;
        }
        newStart_ = startLUT_.Quantized(value);
        SetEnd();
    }

    void SetLength(float value)
    {
        if (value > 0.99f)
        {
            value = 1.f;
        }
        newLength_ = lengthLUT_.Quantized(value);
        SetEnd();
    }

    void SetFilter(float value)
    {
        filterValue_ = fabs(value * 2.f - 1.f);
        filter_->SetFilter(value);
    }

    inline void WriteRead(AudioBuffer& input, AudioBuffer& output)
    {
        size_t size = input.getSize();

        if (triggered_)
        {
            phase_ = 0.0f;
            newPhase_ = 0.0f;
            triggered_ = false;
            fadeVolume_ = 0;
            fadeIn_ = true;
        }

        boc_ = false;

        float x = 0;

        for (size_t i = 0; i < size; i++)
        {
            if (patchCtrls_->looperRecording)
            {
                filter_->Process(input.getSamples(LEFT_CHANNEL)[i], input.getSamples(RIGHT_CHANNEL)[i], filterOut_->getSamples(LEFT_CHANNEL)[i], filterOut_->getSamples(RIGHT_CHANNEL)[i]);

                float left = HardClip(sosOut_->getSamples(LEFT_CHANNEL)[i] * patchCtrls_->looperSos + filterOut_->getSamples(LEFT_CHANNEL)[i]);
                float right = HardClip(sosOut_->getSamples(RIGHT_CHANNEL)[i] * patchCtrls_->looperSos + filterOut_->getSamples(RIGHT_CHANNEL)[i]);

                // this leads to decay even when SOS mode is 100%
                left *= 1.f - ef_[LEFT_CHANNEL]->process(left);
                right *= 1.f - ef_[RIGHT_CHANNEL]->process(right);

#ifdef USE_WRITE_VARISPEED
                buffer_->WriteLinear(wPhase_, left, right, direction_);
                wPhase_ += speed_;
#else
                buffer_->Write(wPhase_, left, right);
                wPhase_ += 1;
#endif

                if (wPhase_ >= kLooperChannelBufferLength)
                {
                    wPhase_ -= kLooperChannelBufferLength;
                }

                wtBuffer_->Write(left, right);
            }

            float left;
            float right;

            // While processing the block, cross-fade the read values of the
            // current position (fade out) and the new position - that accounts
            // for the new start point and the new phase (fade in).
            buffer_->ReadLinear(start_ + phase_, newStart_ + newPhase_, x, left, right, direction_);

            x += xi_;
            if (x >= size)
            {
                x = 0;
            }

            left *= fadeVolume_;
            right *= fadeVolume_;

            sosOut_->getSamples(LEFT_CHANNEL)[i] = left;
            sosOut_->getSamples(RIGHT_CHANNEL)[i] = right;

            output.getSamples(LEFT_CHANNEL)[i] = left * speedVolume_;
            output.getSamples(RIGHT_CHANNEL)[i] = right * speedVolume_;

            phase_ += speed_;
            newPhase_ += newSpeed_;
            if (phase_ >= length_)
            {
                phase_ -= length_;
            }
            if (newPhase_ >= newLength_)
            {
                newPhase_ -= newLength_;
            }
            if (phase_ < 0)
            {
                phase_ += length_;
            }
            if (newPhase_ < 0)
            {
                newPhase_ += newLength_;
            }

            bufferPhase_++;
            if (bufferPhase_ >= kLooperChannelBufferLength)
            {
                boc_ = true;
                bufferPhase_ -= kLooperChannelBufferLength;
            }

            if (ClockSource::CLOCK_SOURCE_EXTERNAL == patchState_->clockSource)
            {
                looping_ = true;
            }

            // Start fading out when reaching the end of the loop.
            if (PlaybackDirection::PLAYBACK_STALLED != direction_ && looping_ && !fadeOut_ && length_ > kLooperFadeSamplesTwice)
            {
                if ((PlaybackDirection::PLAYBACK_FORWARD == direction_ && phase_ >= length_ - kLooperFadeSamples) ||
                    (PlaybackDirection::PLAYBACK_BACKWARDS == direction_ && phase_ <= start_ + kLooperFadeSamples))
                {
                    fadeOut_ = true;
                }
            }

            if (fadeOut_)
            {
                FadeOut();
            }
            else if (fadeIn_)
            {
                FadeIn();
            }
        }

        phase_ = newPhase_;
        length_ = newLength_;
        start_ = newStart_;
        end_ = newEnd_;
        speed_ = newSpeed_;
    }

public:
    Looper(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState, WaveTableBuffer *wtBuffer) :
        // VCV change: moved LUT constructors and other constants here to be sample rate dependent
        startLUT_ (0, kLooperChannelBufferLength - 1),
        lengthLUT_(kLooperLoopLengthMinSeconds * patchState->sampleRate, kLooperChannelBufferLength, Lut<int, 128>::Type::LUT_TYPE_EXPO),
        kLooperFadeSamples(kLooperFade * patchState->sampleRate),
        kLooperFadeSamplesTwice(kLooperFadeTwice * patchState->sampleRate),  
        kLooperFadeInc(kLooperFadeInc_ * patchState->sampleRate)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        buffer_ = LooperBuffer::create();
        filter_ = DjFilter::create(patchState_->sampleRate);
        sosOut_ = AudioBuffer::create(2, patchState_->blockSize);
        filterOut_ = AudioBuffer::create(2, patchState_->blockSize);
        limiter_ = Limiter::create();

        wtBuffer_ = wtBuffer;
        wtBuffer_->Init(buffer_->GetBuffer());

        inputGain_ = 1.f;
        feedback_ = 0;
        fadeVolume_ = 1.f;
        speedVolume_ = 1.f;
        speedValue_ = 0;
        speed_ = newSpeed_ = 0;
        phase_ = newPhase_ = 0;
        wPhase_ = bufferPhase_ = 0;
        length_ = newLength_ = kLooperChannelBufferLength;
        start_ = newStart_ = 0;
        end_ = newEnd_ = kLooperChannelBufferLength - 1;
        filterValue_ = 0;
        xi_ = 1.f / patchState_->blockSize;
        boc_ = true;
        cleared_ = false;
        looping_ = false;

        for (size_t i = 0; i < 2; i++)
        {
            ef_[i] = EnvFollower::create();
        }
    }
    ~Looper()
    {
        LooperBuffer::destroy(buffer_);
        DjFilter::destroy(filter_);
        AudioBuffer::destroy(sosOut_);
        AudioBuffer::destroy(filterOut_);
        Limiter::destroy(limiter_);

        for (size_t i = 0; i < 2; i++)
        {
            EnvFollower::destroy(ef_[i]);
        }
    }

    static Looper* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState, WaveTableBuffer *wtBuffer)
    {
        return new Looper(patchCtrls, patchCvs, patchState, wtBuffer);
    }

    static void destroy(Looper* obj)
    {
        delete obj;
    }

    void Process(AudioBuffer &input, AudioBuffer &output)
    {
        if (patchCtrls_->looperResampling)
        {
            input.multiply(kResampleGain);
        }

        if (ClockSource::CLOCK_SOURCE_EXTERNAL == patchState_->clockSource && (trigger_.Process(patchState_->clockReset || patchState_->clockTick)))
        {
            triggered_ = true;
        }
        else if (ClockSource::CLOCK_SOURCE_INTERNAL == patchState_->clockSource)
        {
            // When the clock is internal, synchronize it with the looper's
            // begin of cycle.
            patchState_->tempo->trigger(boc_);
        }

        MapSpeed();
        float s = Modulate(speedValue_, patchCtrls_->looperSpeedModAmount, patchState_->modValue, patchCtrls_->looperSpeedCvAmount, patchCvs_->looperSpeed, -2.f, 2.f);
        SetSpeed(s);

        float t = Modulate(patchCtrls_->looperStart, patchCtrls_->looperStartModAmount, patchState_->modValue, patchCtrls_->looperStartCvAmount, patchCvs_->looperStart);
        SetStart(t);

        float l = Modulate(patchCtrls_->looperLength, patchCtrls_->looperLengthModAmount, patchState_->modValue, patchCtrls_->looperLengthCvAmount, patchCvs_->looperLength);
        SetLength(l);

        SetFilter(patchCtrls_->looperFilter);

        if (patchState_->clearLooperFlag)
        {
            output.clear();
            patchState_->clearLooperFlag = false;
            cleared_ = true;
        }
        else if (cleared_)
        {
            output.clear();
            bool doneLooper = buffer_->Clear();
            bool doneWt = wtBuffer_->Clear();
            if (doneLooper && doneWt)
            {
                cleared_ = false;
            }
        }
        else
        {
            WriteRead(input, output);
            output.multiply(patchCtrls_->looperVol);
            limiter_->ProcessSoft(output, output);
        }
    }

    LooperBuffer* GetBuffer() { return buffer_; }
    WaveTableBuffer* GetWtBuffer() { return wtBuffer_; }
};
