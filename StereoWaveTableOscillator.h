#pragma once

#include "Commons.h"
#include "WaveTableBuffer.h"
#include "BiquadFilter.h"
#include "EnvFollower.h"
#include <stdlib.h>
#include <stdint.h>
#include <cmath>

class StereoWaveTableOscillator
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    WaveTableBuffer* wtBuffer_;
    BiquadFilter* filters_[2];
    EnvFollower* ef_[2];

    HysteresisQuantizer offsetQuantizer_;

    float amp_;
    float oldFreq_;
    float oldOffset_;
    float phase_;
    float incR_;
    float xi_;

public:
    StereoWaveTableOscillator(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState, WaveTableBuffer* wtBuffer)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;
        wtBuffer_ = wtBuffer;

        phase_ = 0;
        incR_ = kWaveTableLength / patchState_->sampleRate;

        offsetQuantizer_.Init(kWaveTableNofTables, 0.f, false);

        oldOffset_ = 0;
        xi_ = 1.f / patchState_->blockSize;

        for (size_t i = 0; i < 2; i++)
        {
            filters_[i] = BiquadFilter::create(patchState_->sampleRate);
            filters_[i]->setLowShelf(2000, 1);
            ef_[i] = EnvFollower::create();
        }
    }
    ~StereoWaveTableOscillator()
    {
        for (size_t i = 0; i < 2; i++)
        {
            BiquadFilter::destroy(filters_[i]);
            EnvFollower::destroy(ef_[i]);
        }
    }

    static StereoWaveTableOscillator* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState, WaveTableBuffer* wtBuffer)
    {
        return new StereoWaveTableOscillator(patchCtrls, patchCvs, patchState, wtBuffer);
    }

    static void destroy(StereoWaveTableOscillator* osc)
    {
        delete osc;
    }

    void Process(AudioBuffer &output)
    {
        size_t size = output.getSize();

        float u = patchCtrls_->oscUnison;
        if (patchCtrls_->oscUnison < 0)
        {
            u *= 0.5f;
        }

        float f = Modulate(patchCtrls_->oscPitch + patchCtrls_->oscPitch * u, patchCtrls_->oscPitchModAmount, patchState_->modValue, 0, 0, kOscFreqMin, kOscFreqMax, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        // Avoid frequency jittering translating to jumps in the wavetable.
        if (fabs(oldFreq_ - f) < 1.f)
        {
            f = oldFreq_;
        }
        ParameterInterpolator freqParam(&oldFreq_, f, size);

        float o = Modulate(patchCtrls_->oscDetune, patchCtrls_->oscDetuneModAmount, patchState_->modValue, patchCtrls_->oscDetuneCvAmount, patchCvs_->oscDetune, 0, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        ParameterInterpolator offsetParam(&oldOffset_, o, size);

        for (size_t i = 0; i < size; i++)
        {
            phase_ += freqParam.Next() * incR_;
            if (phase_ >= kWaveTableLength)
            {
                phase_ -= kWaveTableLength;
            }

            float p = offsetParam.Next();
            int q = offsetQuantizer_.Process(p);
            float p1 = q * kWaveTableStepLength + phase_;
            float p2 = p1 + kWaveTableStepLength;
            float x = Clamp((p - kWaveTableNofTablesR * q) / kWaveTableNofTablesR);

            float left;
            float right;
            wtBuffer_->ReadLinear(p1, p2, x, left, right);

            left *= Map(ef_[LEFT_CHANNEL]->process(left), 0.f, 0.3f, kOScWaveTablePreGain, 1.f);
            right *= Map(ef_[RIGHT_CHANNEL]->process(right), 0.f, 0.3f, kOScWaveTablePreGain, 1.f);

            left = SoftClip(left);
            right = SoftClip(right);

            left = filters_[LEFT_CHANNEL]->process(left);
            right = filters_[RIGHT_CHANNEL]->process(right);

            output.getSamples(LEFT_CHANNEL)[i] = left * patchCtrls_->osc2Vol * kOScWaveTableGain;
            output.getSamples(RIGHT_CHANNEL)[i] = right * patchCtrls_->osc2Vol * kOScWaveTableGain;
        }
    }
};