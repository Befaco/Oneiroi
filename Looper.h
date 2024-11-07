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
    LooperBuffer* buffer_;
    DjFilter* filter_;
    Limiter* limiter_;
    EnvFollower* ef_[2];

    AudioBuffer* sosOut_;

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
    bool crossFade_;
    bool recording_;

    int fadeIndex_, newFadeIndex_;

    uint32_t length_, start_, end_;
    uint32_t newLength_, newStart_, newEnd_;

    Schmitt trigger_;

    Lut<int, 64> startLUT_{0, kLooperChannelBufferLength - 1};
    Lut<int, 128> lengthLUT_{kLooperLoopLengthMin, kLooperChannelBufferLength, Lut<int, 128>::Type::LUT_TYPE_EXPO};

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

            if (!crossFade_ && length_ > kLooperFadeSamples)
            {
                fadeIndex_ = 0;
                newFadeIndex_ = 0;
                fadeVolume_ = 1.f;
                crossFade_ = true;
            }
        }

        boc_ = false;

        float x = 0;

        for (size_t i = 0; i < size; i++)
        {
            if (ClockSource::CLOCK_SOURCE_EXTERNAL == patchState_->clockSource)
            {
                looping_ = true;
            }

            if (recording_)
            {
                float left, right;
                filter_->Process(input.getSamples(LEFT_CHANNEL)[i], input.getSamples(RIGHT_CHANNEL)[i], left, right);

                left = HardClip(sosOut_->getSamples(LEFT_CHANNEL)[i] * patchCtrls_->looperSos + left);
                right = HardClip(sosOut_->getSamples(RIGHT_CHANNEL)[i] * patchCtrls_->looperSos + right);

                left *= 1.f - ef_[LEFT_CHANNEL]->process(left);
                right *= 1.f - ef_[RIGHT_CHANNEL]->process(right);

#ifdef USE_WRITE_VARISPEED
                buffer_->WriteLinear(wPhase_, left, right, direction_);
                wPhase_ += speed_;
#else
                buffer_->Write(wPhase_, left, right);

                // Stop recording only when fading out is complete.
                if (!buffer_->IsFadingOutWrite() && !patchCtrls_->looperRecording)
                {
                    recording_ = false;
                }
                wPhase_ += 1;
#endif

                if (wPhase_ >= kLooperChannelBufferLength)
                {
                    wPhase_ -= kLooperChannelBufferLength;
                }
            }

            float left = 0;
            float right = 0;

            if (direction_ != PlaybackDirection::PLAYBACK_STALLED)
            {
                // While processing the block, cross-fade the read values of the
                // current position (fade out) and the new position - that accounts
                // for the new start point and the new phase (fade in).
                buffer_->ReadLinear(start_ + phase_, newStart_ + newPhase_, x, left, right, direction_);

                if (crossFade_)
                {
                    float leftTail;
                    float rightTail;

                    if (PlaybackDirection::PLAYBACK_FORWARD == direction_)
                    {
                        buffer_->ReadLinear(end_ + fadeIndex_, newEnd_ + newFadeIndex_, x, leftTail, rightTail, direction_);
                    }
                    else if (PlaybackDirection::PLAYBACK_BACKWARDS == direction_)
                    {
                        buffer_->ReadLinear(start_ - fadeIndex_, newStart_ - newFadeIndex_, x, leftTail, rightTail, direction_);
                    }

                    left = leftTail * fadeVolume_ + left * (1.f - fadeVolume_);
                    right = rightTail * fadeVolume_ + right * (1.f - fadeVolume_);

                    fadeIndex_ += fabs(speed_);
                    fadeVolume_ = fadeIndex_ / kLooperFadeSamples;
                    if (fadeIndex_ >= kLooperFadeSamples)
                    {
                        fadeIndex_ = 0;
                        fadeVolume_ = 1.f;
                        crossFade_ = false;
                    }

                    newFadeIndex_ += fabs(newSpeed_);
                    if (newFadeIndex_ >= kLooperFadeSamples)
                    {
                        newFadeIndex_ = 0;
                    }

                    left = leftTail * fadeVolume_ + left * (1.f - fadeVolume_);
                    right = rightTail * fadeVolume_ + right * (1.f - fadeVolume_);
                }

                phase_ += speed_;
                if (phase_ >= length_)
                {
                    phase_ -= length_;
                    if (looping_ && !crossFade_ && length_ > kLooperFadeSamples)
                    {
                        crossFade_ = true;
                    }
                }
                if (phase_ < 0)
                {
                    phase_ += length_;
                    if (looping_ && !crossFade_ && length_ > kLooperFadeSamples)
                    {
                        crossFade_ = true;
                    }
                }

                newPhase_ += newSpeed_;
                if (newPhase_ >= newLength_)
                {
                    newPhase_ -= newLength_;
                }
                if (newPhase_ < 0)
                {
                    newPhase_ += newLength_;
                }

                x += xi_;
                if (x >= size)
                {
                    x = 0;
                }
            }

            sosOut_->getSamples(LEFT_CHANNEL)[i] = left;
            sosOut_->getSamples(RIGHT_CHANNEL)[i] = right;

            output.getSamples(LEFT_CHANNEL)[i] = left * speedVolume_;
            output.getSamples(RIGHT_CHANNEL)[i] = right * speedVolume_;

            bufferPhase_++;
            if (bufferPhase_ >= kLooperChannelBufferLength)
            {
                boc_ = true;
                bufferPhase_ -= kLooperChannelBufferLength;
            }
        }

        phase_ = newPhase_;
        length_ = newLength_;
        start_ = newStart_;
        end_ = newEnd_;
        speed_ = newSpeed_;
        fadeIndex_ = newFadeIndex_;
    }

public:
    Looper(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        buffer_ = LooperBuffer::create();
        filter_ = DjFilter::create(patchState_->sampleRate);
        sosOut_ = AudioBuffer::create(2, patchState_->blockSize);
        limiter_ = Limiter::create();

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
        fadeIndex_ = 0;
        boc_ = true;
        cleared_ = false;
        crossFade_ = false;
        looping_ = false;
        recording_ = false;

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
        Limiter::destroy(limiter_);

        for (size_t i = 0; i < 2; i++)
        {
            EnvFollower::destroy(ef_[i]);
        }
    }

    static Looper* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Looper(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Looper* obj)
    {
        delete obj;
    }

    FloatArray *GetBuffer()
    {
        return buffer_->GetBuffer();
    }

    void Process(AudioBuffer &input, AudioBuffer &output)
    {
        input.multiply(patchCtrls_->looperResampling ? kLooperResampleGain : kLooperInputGain);

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
        float s = Modulate(speedValue_, patchCtrls_->looperSpeedModAmount, patchState_->modValue, patchCtrls_->looperSpeedCvAmount, patchCvs_->looperSpeed, -2.f, 2.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetSpeed(s);

        float t = Modulate(patchCtrls_->looperStart, patchCtrls_->looperStartModAmount, patchState_->modValue, patchCtrls_->looperStartCvAmount, patchCvs_->looperStart, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetStart(t);

        float l = Modulate(patchCtrls_->looperLength, patchCtrls_->looperLengthModAmount, patchState_->modValue, patchCtrls_->looperLengthCvAmount, patchCvs_->looperLength, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetLength(l);

        SetFilter(patchCtrls_->looperFilter);

        if (patchCtrls_->looperRecording && !recording_)
        {
            recording_ = true;
            buffer_->FadeInWrite();
        }
        else if (!patchCtrls_->looperRecording && recording_ && !buffer_->IsFadingOutWrite())
        {
            recording_ = false;
            buffer_->FadeOutWrite();
        }

        if (patchState_->clearLooperFlag)
        {
            output.clear();
            patchState_->clearLooperFlag = false;
            cleared_ = true;
        }
        else if (cleared_)
        {
            output.clear();
            if (buffer_->Clear())
            {
                cleared_ = false;
            }
        }
        else
        {
            WriteRead(input, output);
            output.multiply(patchCtrls_->looperVol * kLooperMakeupGain);
            limiter_->ProcessSoft(output, output);
        }
    }
};
