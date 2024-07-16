#pragma once

#include "Commons.h"
#include "WaveTableBuffer.h"
//#include "Compressor.h"
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
    //Compressor* compressors_[2];

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

        offsetQuantizer_.Init(kWaveTableNofTables, 0.25f, false);

        oldOffset_ = 0;
        xi_ = 1.f / patchState_->blockSize;

        /*
        for (size_t i = 0; i < 2; i++)
        {
            compressors_[i] = Compressor::create(patchState_->sampleRate);
            compressors_[i]->setRatio(10.f);
            compressors_[i]->setAttack(1.f);
            compressors_[i]->setRelease(100.f);
            compressors_[i]->setThreshold(-20.f);
        }
        */
    }
    ~StereoWaveTableOscillator()
    {
        /*
        for (size_t i = 0; i < 2; i++)
        {
            Compressor::destroy(compressors_[i]);
        }
        */
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

        float f = Modulate(patchCtrls_->oscPitch + patchCtrls_->oscPitch * u, patchCtrls_->oscPitchModAmount, patchState_->modValue, 0, 0, kOscFreqMin, kOscFreqMax);
        ParameterInterpolator freqParam(&oldFreq_, f, size);

        float o = Modulate(patchCtrls_->oscDetune, patchCtrls_->oscDetuneModAmount, patchState_->modValue, patchCtrls_->oscDetuneCvAmount, patchCvs_->oscDetune);

        float x = 0;

        for (size_t i = 0; i < size; i++)
        {
            phase_ += freqParam.Next() * incR_;
            if (phase_ >= kWaveTableLength)
            {
                phase_ -= kWaveTableLength;
            }

            float p1 = (offsetQuantizer_.Process(oldOffset_) * kWaveTableLength) + phase_;
            float p2 = (offsetQuantizer_.Process(o) * kWaveTableLength) + phase_;
            float left;
            float right;
            wtBuffer_->Read(p1, p2, x, left, right);

            x += xi_;
            if (x >= size)
            {
                x = 0;
            }

            //left = compressors_[LEFT_CHANNEL]->process(left);
            //right = compressors_[RIGHT_CHANNEL]->process(right);

            output.getSamples(LEFT_CHANNEL)[i] = left * patchCtrls_->osc2Vol * kOScWaveTableGain;
            output.getSamples(RIGHT_CHANNEL)[i] = right * patchCtrls_->osc2Vol * kOScWaveTableGain;
        }

        oldOffset_ = o;
    }
};