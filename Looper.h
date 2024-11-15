#pragma once

#include "Commons.h"
#include "LooperBuffer.h"
#include "WaveTableBuffer.h"
#include "SquareWaveOscillator.h"
#include "Schmitt.h"
#include "DjFilter.h"
#include "Limiter.h"
#include "EnvFollower.h"
#include "ParameterInterpolator.h"
#include <stdint.h>
#include <cmath>

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

    float wPhase_;
    float phase_;
    float speed_;
    float speedValue_;
    float inputGain_, feedback_, loopFadeVolume_, triggerFadeVolume_, speedVolume_;
    float filterValue_;
    float oldStartValue_, oldLengthValue_, oldSpeedValue_;

    bool triggered_;
    bool boc_;
    bool cleared_;
    bool looping_;
    bool loopCrossFade_, triggerFadeOut_, triggerFadeIn_;
    bool recording_;

    float loopCrossFadePhase_;

    int triggerFadeIndex_;

    uint32_t bufferPhase_;
    uint32_t length_, start_, end_;

    Schmitt trigger_;

    Lut<uint32_t, 128> startLUT_{0, kLooperChannelBufferLength - 1};
    Lut<uint32_t, 128> lengthLUT_{kLooperLoopLengthMin, kLooperChannelBufferLength, Lut<uint32_t, 128>::Type::LUT_TYPE_EXPO};

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

        speed_ = value;
    }

    void SetEnd()
    {
        end_ = start_ + length_;
        if (end_ >= kLooperChannelBufferLength)
        {
            end_ -= kLooperChannelBufferLength;
        }
        looping_ = length_ < kLooperChannelBufferLength;
    }

    void SetStart(float value)
    {
        if (value > 0.99f)
        {
            value = 1.f;
        }
        start_ = startLUT_.Quantized(value);
        SetEnd();
    }

    void SetLength(float value)
    {
        if (value > 0.99f)
        {
            value = 1.f;
        }
        length_ = lengthLUT_.Quantized(value);
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
            triggered_ = false;
            if (length_ >= kLooperTriggerFadeSamples * 2)
            {
                // Fade only if we have enough space.
                triggerFadeOut_ = true;
            }
            else
            {
                // Otherwise just reset the phase.
                phase_ = 0.0f;
            }
        }

        boc_ = false;

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

                buffer_->Write(wPhase_, left, right);

                // Stop recording only when fade out is complete.
                if (!buffer_->IsRecording() && !patchCtrls_->looperRecording)
                {
                    recording_ = false;
                }

                wPhase_++;
                if (wPhase_ >= kLooperChannelBufferLength)
                {
                    wPhase_ -= kLooperChannelBufferLength;
                }
            }

            float left = 0;
            float right = 0;

            if (direction_ != PlaybackDirection::PLAYBACK_STALLED)
            {
                buffer_->Read(start_ + phase_, left, right, direction_);

                if (loopCrossFade_)
                {
                    float leftTail;
                    float rightTail;

                    int32_t start;
                    if (PlaybackDirection::PLAYBACK_FORWARD == direction_)
                    {
                        start = start_ - kLooperFadeSamples;
                        if (start < 0)
                        {
                            start += kLooperChannelBufferLength;
                        }
                        buffer_->Read(start + loopCrossFadePhase_, leftTail, rightTail, direction_);
                    }
                    else
                    {
                        start = end_ + kLooperFadeSamples;
                        if (start >= kLooperChannelBufferLength)
                        {
                            start -= kLooperChannelBufferLength;
                        }
                        buffer_->Read(start - loopCrossFadePhase_, leftTail, rightTail, direction_);
                    }

                    loopFadeVolume_ = loopCrossFadePhase_ * kLooperFadeSamplesR;
                    loopCrossFadePhase_ += fabs(speed_);
                    if (loopCrossFadePhase_ >= kLooperFadeSamples)
                    {
                        loopCrossFadePhase_ = 0;
                        loopFadeVolume_ = 1.f;
                        loopCrossFade_ = false;
                    }

                    left = leftTail * loopFadeVolume_ + left * (1.f - loopFadeVolume_);
                    right = rightTail * loopFadeVolume_ + right * (1.f - loopFadeVolume_);
                }

                if (triggerFadeOut_ || triggerFadeIn_)
                {
                    triggerFadeVolume_ = triggerFadeIndex_ * kLooperTriggerFadeSamplesR;
                    if (triggerFadeOut_)
                    {
                        triggerFadeVolume_ = 1.f - triggerFadeVolume_;
                    }
                    triggerFadeIndex_++;
                    if (triggerFadeIndex_ >= kLooperTriggerFadeSamples)
                    {
                        triggerFadeIndex_ = 0;
                        if (triggerFadeOut_)
                        {
                            // Reset phase when fade out is complete.
                            phase_ = 0.0f;
                            triggerFadeVolume_ = 0.f;
                            triggerFadeOut_ = false;
                            triggerFadeIn_ = true;
                        }
                        else
                        {
                            triggerFadeVolume_ = 1.f;
                            triggerFadeIn_ = false;
                        }
                    }
                    left *= triggerFadeVolume_;
                    right *= triggerFadeVolume_;
                }

                phase_ += speed_;
                if (PlaybackDirection::PLAYBACK_FORWARD == direction_)
                {
                    if (phase_ >= length_)
                    {
                        phase_ -= length_;
                    }
                    if (looping_ && !loopCrossFade_ && length_ > kLooperFadeSamples && phase_ >= length_ - kLooperFadeSamples)
                    {
                        loopCrossFade_ = true;
                    }
                }
                else
                {
                    if (phase_ < 0)
                    {
                        phase_ += length_;
                    }
                    if (looping_ && !loopCrossFade_ && length_ > kLooperFadeSamples && phase_ <= kLooperFadeSamples)
                    {
                        loopCrossFade_ = true;
                    }
                }
            }

            sosOut_->getSamples(LEFT_CHANNEL)[i] = left;
            sosOut_->getSamples(RIGHT_CHANNEL)[i] = right;

            output.getSamples(LEFT_CHANNEL)[i] = left * speedVolume_;
            output.getSamples(RIGHT_CHANNEL)[i] = right * speedVolume_;

            bufferPhase_++;
            if (bufferPhase_ == kLooperChannelBufferLength)
            {
                boc_ = true;
                bufferPhase_ = 0;
            }
        }
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

        direction_ = PlaybackDirection::PLAYBACK_FORWARD;

        inputGain_ = 1.f;
        feedback_ = 0;
        loopFadeVolume_ = 1.f;
        speedVolume_ = 1.f;
        speedValue_ = 0;
        speed_ = 1.f;
        phase_ = 0;
        wPhase_ = bufferPhase_ = 0;
        length_ = kLooperChannelBufferLength;
        start_ = 0;
        end_ = kLooperChannelBufferLength - 1;
        filterValue_ = 0;
        loopCrossFadePhase_ = 0;
        boc_ = true;
        cleared_ = false;
        loopCrossFade_ = false;
        looping_ = false;
        recording_ = false;
        triggerFadeIndex_ = 0;
        triggerFadeVolume_ = 0;
        triggerFadeOut_ = false;
        triggerFadeIn_ = false;
        oldStartValue_ = 0;
        oldLengthValue_ = 1.f;

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
        ParameterInterpolator speedParam(&oldSpeedValue_, speedValue_, kLooperInterpolationBlocks);
        float rs = Modulate(speedParam.Next(), patchCtrls_->looperSpeedModAmount, patchState_->modValue, patchCtrls_->looperSpeedCvAmount, patchCvs_->looperSpeed, -2.f, 2.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetSpeed(rs);

        ParameterInterpolator startParam(&oldStartValue_, patchCtrls_->looperStart, kLooperInterpolationBlocks);
        float t = Modulate(startParam.Next(), patchCtrls_->looperStartModAmount, patchState_->modValue, patchCtrls_->looperStartCvAmount, patchCvs_->looperStart, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetStart(t);

        ParameterInterpolator lengthParam(&oldLengthValue_, patchCtrls_->looperLength, kLooperInterpolationBlocks);
        float l = Modulate(lengthParam.Next(), patchCtrls_->looperLengthModAmount, patchState_->modValue, patchCtrls_->looperLengthCvAmount, patchCvs_->looperLength, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetLength(l);

        SetFilter(patchCtrls_->looperFilter);

        if (patchCtrls_->looperRecording && !recording_)
        {
            recording_ = true;
            buffer_->StartRecording();
        }
        else if (!patchCtrls_->looperRecording && recording_)
        {
            buffer_->StopRecording();
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
