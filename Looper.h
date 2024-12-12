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
    float filterValue_;
    float oldStartValue_, oldLengthValue_, oldSpeedValue_;
    float inputGain_, feedback_, triggerFadeVolume_, speedVolume_;
    float length_, start_;
    float newLength_, newStart_;
    float fadePhase_, fadeSamples_, fadeSamplesR_;
    float fadeThreshold_;

    bool triggered_;
    bool boc_;
    bool cleared_;
    bool fade_, startFade_, triggerFadeOut_, triggerFadeIn_;

    int triggerFadeIndex_;

    int32_t bufferPhase_;

    Schmitt trigger_;

    const int32_t kLooperChannelBufferLength;

    Lut<uint32_t, 128> startLUT_;
    Lut<uint32_t, 128> lengthLUT_;

    // VCV specific
    int32_t kLooperFadeSamples, kLooperTriggerFadeSamples, kLooperLoopLengthMinSamples;
    float kLooperTriggerFadeSamplesR;
    bool retriggerLoopOnClock_ = true;

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
            speedVolume_ = Map(fabs(value), 0.1f, 0.2f, 0.f, 1.f);
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

    void SetStart(float value)
    {
        if (fade_)
        {
            return;
        }

        if (value > 0.99f)
        {
            value = 1.f;
        }

        uint32_t v = startLUT_.Quantized(value);
        if (v != start_)
        {
            newStart_ = v;

            // Always fade, because the start point offsets the phase.
            fade_ = true;
            startFade_ = true;
        }
    }

    bool lengthFade_ = false;

    void SetLength(float value)
    {
        if (fade_)
        {
            return;
        }

        if (value > 0.99f)
        {
            value = 1.f;
        }

        uint32_t v = lengthLUT_.Quantized(value);
        if (v != length_)
        {
            newLength_ = v;

            fadeThreshold_ = Min(newLength_ * 0.1f, kLooperFadeSamples);

            // Fade is only necessary if phase goes outside of the loop.
            if (phase_ > newLength_)
            {
                fadeSamples_ = Clamp(phase_ - newLength_, kLooperLoopLengthMinSamples * 0.1f, kLooperFadeSamples);
                fadeSamplesR_ = 1.f / fadeSamples_ * fabs(speed_);
                fade_ = true;
                lengthFade_ = true;
            }
        }
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
                phase_ = 0.f;
            }
        }

        boc_ = false;

        for (size_t i = 0; i < size; i++)
        {
            if (buffer_->IsRecording())
            {
                float left, right;
                filter_->Process(input.getSamples(LEFT_CHANNEL)[i], input.getSamples(RIGHT_CHANNEL)[i], left, right);

                left = HardClip(sosOut_->getSamples(LEFT_CHANNEL)[i] * patchCtrls_->looperSos + left);
                right = HardClip(sosOut_->getSamples(RIGHT_CHANNEL)[i] * patchCtrls_->looperSos + right);

                // this leads to decay even when SOS mode is 100%
                left *= 1.f - ef_[LEFT_CHANNEL]->process(left);
                right *= 1.f - ef_[RIGHT_CHANNEL]->process(right);

                buffer_->Write(wPhase_, left, right);

                wPhase_++;
                if (wPhase_ >= kLooperChannelBufferLength)
                {
                    wPhase_ -= kLooperChannelBufferLength;
                }
            }

            float left = 0;
            float right = 0;

            buffer_->Read(start_ + phase_, left, right, direction_);

            if (fade_)
            {
                float start = newStart_;
                if (!startFade_ && !lengthFade_)
                {
                    start -= newLength_ * direction_;
                }
                if (lengthFade_ && PlaybackDirection::PLAYBACK_BACKWARDS == direction_)
                {
                    start += newLength_ - (length_ - phase_);
                }
                else
                {
                    start += phase_;
                }

                float fadeLeft;
                float fadeRight;
                buffer_->Read(start, fadeLeft, fadeRight, direction_);

                if (fadePhase_ < 1.f)
                {
                    left = CheapEqualPowerCrossFade(left, fadeLeft, fadePhase_);
                    right = CheapEqualPowerCrossFade(right, fadeRight, fadePhase_);
                    fadePhase_ += fadeSamplesR_;
                }
                else
                {
                    if (!startFade_ && !lengthFade_)
                    {
                        phase_ = PlaybackDirection::PLAYBACK_FORWARD == direction_ ? Max(phase_ - newLength_, 0) : newLength_;
                    }
                    if (lengthFade_ && PlaybackDirection::PLAYBACK_BACKWARDS == direction_)
                    {
                        phase_ = newLength_ - (length_ - phase_);
                    }

                    fadePhase_ = 0;
                    start_ = newStart_;
                    length_ = newLength_;
                    left = fadeLeft;
                    right = fadeRight;
                    fade_ = false;
                    startFade_ = false;
                    lengthFade_ = false;
                }
            }
            else
            {
                start_ = newStart_;
                length_ = newLength_;
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
                        phase_ = 0.f;
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

            if (!fade_)
            {
                if ((PlaybackDirection::PLAYBACK_FORWARD == direction_ && phase_ >= length_ - fadeThreshold_) ||
                    (PlaybackDirection::PLAYBACK_BACKWARDS == direction_ && phase_ <= fadeThreshold_))
                {
                    fadeSamples_ = PlaybackDirection::PLAYBACK_FORWARD == direction_ ? length_ - phase_ : phase_;
                    fadeSamples_ = Clamp(fadeSamples_, kLooperLoopLengthMinSamples * 0.1f, kLooperFadeSamples);
                    fadeSamplesR_ = 1.f / fadeSamples_ * fabs(speed_);
                    fade_ = true;
                }
            }

            phase_ += speed_;

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
    Looper(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState) :
        kLooperChannelBufferLength(kLooperChannelBufferLengthSeconds * patchState->sampleRate),
        // VCV change: moved LUT constructors and other constants here to be sample rate dependent
        startLUT_ (0, kLooperChannelBufferLength - 1),
        lengthLUT_(kLooperLoopLengthMinSeconds * patchState->sampleRate, kLooperChannelBufferLength, Lut<uint32_t, 128>::Type::LUT_TYPE_EXPO),
        kLooperFadeSamples(kLooperFadeSeconds * patchState->sampleRate),
        kLooperTriggerFadeSamples(kLooperTriggerFadeSeconds * patchState->sampleRate),
        kLooperLoopLengthMinSamples(kLooperLoopLengthMinSeconds * patchState->sampleRate),
        kLooperTriggerFadeSamplesR(1.f / kLooperTriggerFadeSamples)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        buffer_ = LooperBuffer::create(patchState_->sampleRate);
        filter_ = DjFilter::create(patchState_->sampleRate);
        sosOut_ = AudioBuffer::create(2, patchState_->blockSize);
        limiter_ = Limiter::create();

        direction_ = PlaybackDirection::PLAYBACK_FORWARD;

        inputGain_ = 1.f;
        feedback_ = 0;
        speedVolume_ = 1.f;
        oldSpeedValue_ = speedValue_ = 0.7f;
        speed_ = 1.f;
        phase_ = 0;
        wPhase_ = bufferPhase_ = 0;
        length_ = newLength_ = kLooperChannelBufferLength;
        start_ = newStart_ = 0;
        filterValue_ = 0;
        fadePhase_ = 0;
        fadeThreshold_ = kLooperFadeSamples;
        fadeSamples_ = kLooperFadeSamples;
        fadeSamplesR_ = 1.f / fadeSamples_;
        startFade_ = false;
        boc_ = true;
        cleared_ = false;
        fade_ = false;
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
            if (retriggerLoopOnClock_) {
                triggered_ = true;
            }
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

        if (StartupPhase::STARTUP_DONE != patchState_->startupPhase)
        {
            return;
        }

        if (patchCtrls_->looperRecording && !buffer_->IsRecording())
        {
            buffer_->StartRecording();
        }
        else if (!patchCtrls_->looperRecording && buffer_->IsRecording())
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

        WriteRead(input, output);

        if (PlaybackDirection::PLAYBACK_STALLED == direction_)
        {
            output.clear();
        }
        else
        {
            output.multiply(patchCtrls_->looperVol * kLooperMakeupGain);
            limiter_->ProcessSoft(output, output);
        }
    }

    void SetRetriggerLoopOnClock(bool retriggerOnClock) {
        retriggerLoopOnClock_ = retriggerOnClock;
    }
};
