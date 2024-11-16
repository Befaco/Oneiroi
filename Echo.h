#pragma once

#include "Commons.h"
#include "DelayLine.h"
#include "EnvFollower.h"
#include "SineOscillator.h"
#include "ParameterInterpolator.h"
#include "DjFilter.h"
#include "Compressor.h"
#include <stdint.h>

enum EchoTap
{
    TAP_LEFT_A,
    TAP_LEFT_B,
    TAP_RIGHT_A,
    TAP_RIGHT_B,
};

class Echo
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    DelayLine* lines_[kEchoTaps];
    DjFilter* filter_;
    EnvFollower* ef_[2];
    Compressor* comp_[2];

    HysteresisQuantizer densityQuantizer_;

    int clockRatiosIndex_;
    float echoDensity_, oldDensity_;

    float levels_[kEchoTaps], outs_[kEchoTaps];
    float tapsTimes_[kEchoTaps], newTapsTimes_[kEchoTaps], maxTapsTimes_[kEchoTaps];
    float repeats_, filterValue_;
    float xi_;

    bool externalClock_;
    bool infinite_;

    void SetTapTime(int idx, float time)
    {
        newTapsTimes_[idx] = Clamp(time, kEchoMinLengthSamples * kEchoTapsRatios[idx], (kEchoMaxLengthSamples - 1) * kEchoTapsRatios[idx]);
    }

    void SetMaxTapTime(int idx, float time)
    {
        maxTapsTimes_[idx] = time;
        SetTapTime(idx, Max(echoDensity_ * maxTapsTimes_[idx], kEchoMinLengthSamples));
    }

    void SetLevel(int idx, float value)
    {
        levels_[idx] = value;
    }

    void SetFilter(float value)
    {
        filterValue_ = value;
        filter_->SetFilter(value);
    }

    void SetRepeats(float value)
    {
        repeats_ = value;

        float r = repeats_;

        infinite_ = false;

        // Infinite feedback.
        if (r > kEchoInfiniteFeedbackThreshold)
        {
            r = kEchoInfiniteFeedbackLevel;
            infinite_ = true;
        }
        else if (r > 0.99f)
        {
            r = 1.f;
        }

        SetLevel(TAP_LEFT_A, r * kEchoTapsFeedbacks[TAP_LEFT_A]);
        SetLevel(TAP_LEFT_B, r * kEchoTapsFeedbacks[TAP_LEFT_B]);
        SetLevel(TAP_RIGHT_A, r * kEchoTapsFeedbacks[TAP_RIGHT_A]);
        SetLevel(TAP_RIGHT_B, r * kEchoTapsFeedbacks[TAP_RIGHT_B]);

        float thrs = Map(repeats_, 0.f, 1.f, kEchoCompThresMin, kEchoCompThresMax);
        comp_[LEFT_CHANNEL]->setThreshold(thrs);
        comp_[RIGHT_CHANNEL]->setThreshold(thrs);
    }

    void SetDensity(float value)
    {
        if (ClockSource::CLOCK_SOURCE_EXTERNAL == patchState_->clockSource)
        {
            int newIndex = densityQuantizer_.Process(value);
            if (newIndex == clockRatiosIndex_ && externalClock_)
            {
                return;
            }
            clockRatiosIndex_ = newIndex;

            float d = kModClockRatios[clockRatiosIndex_] * patchState_->clockSamples * kEchoExternalClockMultiplier;
            for (size_t i = 0; i < kEchoTaps; i++)
            {
                SetTapTime(i, d * kEchoTapsRatios[i]);
            }

            // Reset max tap time the next time (...) the clock switches to internal.
            if (!externalClock_)
            {
                externalClock_ = true;
            }
        }
        else
        {
            if (externalClock_)
            {
                int32_t t = kEchoMaxLengthSamples - 1;
                for (size_t i = 0; i < kEchoTaps; i++)
                {
                    SetMaxTapTime(i, t * kEchoTapsRatios[i]);
                }
                externalClock_ = false;
            }

            echoDensity_ = value;

            float d = MapExpo(echoDensity_, 0.f, 1.f, kEchoMinLengthSamples, patchState_->clockSamples * kEchoInternalClockMultiplier);
            size_t s = kEchoFadeSamples;
            ParameterInterpolator densityParam(&oldDensity_, d, s);

            for (size_t i = 0; i < kEchoTaps; i++)
            {
                SetTapTime(i, densityParam.Next() * kEchoTapsRatios[i]);
            }
        }
    }

public:
    Echo(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        for (size_t i = 0; i < kEchoTaps; i++)
        {
            lines_[i] = DelayLine::create(kEchoMaxLengthSamples);
            tapsTimes_[i] = kEchoMaxLengthSamples - 1;
            SetMaxTapTime(i, tapsTimes_[i] * kEchoTapsRatios[i]);
            levels_[i] = 0;
            outs_[i] = 0;
        }

        echoDensity_ = 1.f;
        clockRatiosIndex_ = 0;

        xi_ = 1.f / patchState_->blockSize;

        externalClock_ = false;
        infinite_ = false;

        filter_ = DjFilter::create(patchState_->sampleRate);

        for (size_t i = 0; i < 2; i++)
        {
            comp_[i] = Compressor::create(patchState_->sampleRate);
            comp_[i]->setThreshold(-16);
            ef_[i] = EnvFollower::create();
        }

        densityQuantizer_.Init(kClockUnityRatioIndex, 0.15f, false);
    }
    ~Echo()
    {
        for (size_t i = 0; i < kEchoTaps; i++)
        {
            DelayLine::destroy(lines_[i]);
        }
        DjFilter::destroy(filter_);
        for (size_t i = 0; i < 2; i++)
        {
            Compressor::destroy(comp_[i]);
            EnvFollower::destroy(ef_[i]);
        }
    }

    static Echo* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Echo(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Echo* obj)
    {
        delete obj;
    }

    void process(AudioBuffer &input, AudioBuffer &output)
    {
        size_t size = output.getSize();
        FloatArray leftIn = input.getSamples(LEFT_CHANNEL);
        FloatArray rightIn = input.getSamples(RIGHT_CHANNEL);
        FloatArray leftOut = output.getSamples(LEFT_CHANNEL);
        FloatArray rightOut = output.getSamples(RIGHT_CHANNEL);

        SetFilter(patchCtrls_->echoFilter);

        float d = Modulate(patchCtrls_->echoDensity, patchCtrls_->echoDensityModAmount, patchState_->modValue, patchCtrls_->echoDensityCvAmount, patchCvs_->echoDensity, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        if (externalClock_)
        {
            SetDensity(d);
        }

        float r = Modulate(patchCtrls_->echoRepeats, patchCtrls_->echoRepeatsModAmount, patchState_->modValue, patchCtrls_->echoRepeatsCvAmount, patchCvs_->echoRepeats, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetRepeats(r);

        float x = 0;

        for (int i = 0; i < size; i++)
        {
            // Using crossfade between two different tap times when the clock is
            // external and a filtered density param for when the clock is
            // internal (for pitch shifting effect).
            if (externalClock_)
            {
                outs_[TAP_LEFT_A] = lines_[TAP_LEFT_A]->read(tapsTimes_[TAP_LEFT_A], newTapsTimes_[TAP_LEFT_A], x); // A
                outs_[TAP_LEFT_B] = lines_[TAP_LEFT_B]->read(tapsTimes_[TAP_LEFT_B], newTapsTimes_[TAP_LEFT_B], x); // B
                outs_[TAP_RIGHT_A] = lines_[TAP_RIGHT_A]->read(tapsTimes_[TAP_RIGHT_A], newTapsTimes_[TAP_RIGHT_A], x); // A
                outs_[TAP_RIGHT_B] = lines_[TAP_RIGHT_B]->read(tapsTimes_[TAP_RIGHT_B], newTapsTimes_[TAP_RIGHT_B], x); // B

                x += xi_;
            }
            else
            {
                SetDensity(d);
                outs_[TAP_LEFT_A] = lines_[TAP_LEFT_A]->read(newTapsTimes_[TAP_LEFT_A]); // A
                outs_[TAP_LEFT_B] = lines_[TAP_LEFT_B]->read(newTapsTimes_[TAP_LEFT_B]); // B
                outs_[TAP_RIGHT_A] = lines_[TAP_RIGHT_A]->read(newTapsTimes_[TAP_RIGHT_A]); // A
                outs_[TAP_RIGHT_B] = lines_[TAP_RIGHT_B]->read(newTapsTimes_[TAP_RIGHT_B]); // B
            }

            float leftFb = HardClip(outs_[TAP_LEFT_A] * levels_[TAP_LEFT_A] + outs_[TAP_RIGHT_A] * levels_[TAP_RIGHT_A]);
            float rightFb = HardClip(outs_[TAP_LEFT_B] * levels_[TAP_LEFT_B] + outs_[TAP_RIGHT_B] * levels_[TAP_RIGHT_B]);

            if (infinite_)
            {
                leftFb *= 1.08f - ef_[LEFT_CHANNEL]->process(leftFb);
                rightFb *= 1.08f - ef_[RIGHT_CHANNEL]->process(rightFb);
            }

            float lIn = Clamp(leftIn[i], -3.f, 3.f);
            float rIn = Clamp(rightIn[i], -3.f, 3.f);

            float leftFilter;
            float rightFilter;

            filter_->Process(lIn, rIn, leftFilter, rightFilter);

            leftFb += leftFilter;
            rightFb += rightFilter;

            lines_[TAP_LEFT_A]->write(leftFb);
            lines_[TAP_LEFT_B]->write(leftFb);
            lines_[TAP_RIGHT_A]->write(rightFb);
            lines_[TAP_RIGHT_B]->write(rightFb);

            float left = Mix2(outs_[TAP_LEFT_A], outs_[TAP_LEFT_B]);
            float right = Mix2(outs_[TAP_RIGHT_A], outs_[TAP_RIGHT_B]);

            left = comp_[LEFT_CHANNEL]->process(left) * kEchoMakeupGain;
            right = comp_[RIGHT_CHANNEL]->process(right) * kEchoMakeupGain;

            leftOut[i] = CheapEqualPowerCrossFade(lIn, left, patchCtrls_->echoVol, 1.8f);
            rightOut[i] = CheapEqualPowerCrossFade(rIn, right, patchCtrls_->echoVol, 1.8f);
        }

        if (externalClock_)
        {
            for (size_t j = 0; j < kEchoTaps; j++)
            {
                tapsTimes_[j] = newTapsTimes_[j];
            }
        }
    }
};