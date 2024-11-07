#pragma once

#include "Commons.h"
#include "WaveTableBuffer.h"
#include "StereoSineOscillator.h"
#include "StereoSuperSaw.h"
#include "StereoWaveTableOscillator.h"
#include "Ambience.h"
#include "Filter.h"
#include "Resonator.h"
#include "Echo.h"
#include "Looper.h"
#include "Schmitt.h"
#include "TGate.h"
#include "EnvFollower.h"
#include "DcBlockingFilter.h"
#include "SmoothValue.h"
#include "Modulation.h"
#include "Limiter.h"

class Oneiroi
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    StereoSineOscillator* sine_;
    StereoSuperSaw* saw_;
    StereoWaveTableOscillator* wt_;
    WaveTableBuffer* wtBuffer_;
    Filter* filter_;
    Resonator* resonator_;
    Echo* echo_;
    Ambience* ambience_;
    Looper* looper_;
    Limiter* limiter_;

    Modulation* modulation_;

    AudioBuffer* input_;
    AudioBuffer* resample_;
    AudioBuffer* osc1Out_;
    AudioBuffer* osc2Out_;

    StereoDcBlockingFilter inputDcFilter_;
    EnvFollower* inEnvFollower_[2];

    FilterPosition filterPosition_, lastFilterPosition_;

public:
    Oneiroi(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        looper_ = Looper::create(patchCtrls_, patchCvs_, patchState_);
        wtBuffer_ = WaveTableBuffer::create(looper_->GetBuffer());

        sine_ = StereoSineOscillator::create(patchCtrls_, patchCvs_, patchState_);
        saw_ = StereoSuperSaw::create(patchCtrls_, patchCvs_, patchState_);
        wt_ = StereoWaveTableOscillator::create(patchCtrls_, patchCvs_, patchState_, wtBuffer_);

        filter_ = Filter::create(patchCtrls_, patchCvs_, patchState_);
        resonator_ = Resonator::create(patchCtrls_, patchCvs_, patchState_);
        echo_ = Echo::create(patchCtrls_, patchCvs_, patchState_);
        ambience_ = Ambience::create(patchCtrls_, patchCvs_, patchState_);

        modulation_ = Modulation::create(patchCtrls_, patchCvs_, patchState_);

        limiter_ = Limiter::create();

        input_ = AudioBuffer::create(2, patchState_->blockSize);
        resample_ = AudioBuffer::create(2, patchState_->blockSize);
        osc1Out_ = AudioBuffer::create(2, patchState_->blockSize);
        osc2Out_ = AudioBuffer::create(2, patchState_->blockSize);

        for (size_t i = 0; i < 2; i++)
        {
            inEnvFollower_[i] = EnvFollower::create();
            inEnvFollower_[i]->setLambda(0.9f);
        }
    }
    ~Oneiroi()
    {
        AudioBuffer::destroy(input_);
        AudioBuffer::destroy(resample_);
        AudioBuffer::destroy(osc1Out_);
        AudioBuffer::destroy(osc2Out_);
        WaveTableBuffer::destroy(wtBuffer_);
        Looper::destroy(looper_);
        StereoSineOscillator::destroy(sine_);
        StereoSuperSaw::destroy(saw_);
        StereoWaveTableOscillator::destroy(wt_);
        Filter::destroy(filter_);
        Resonator::destroy(resonator_);
        Echo::destroy(echo_);
        Ambience::destroy(ambience_);
        Modulation::destroy(modulation_);
        Limiter::destroy(limiter_);

        for (size_t i = 0; i < 2; i++)
        {
            EnvFollower::destroy(inEnvFollower_[i]);
        }
    }

    static Oneiroi* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Oneiroi(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Oneiroi* obj)
    {
        delete obj;
    }

    inline void Process(AudioBuffer &buffer)
    {
        FloatArray left = buffer.getSamples(LEFT_CHANNEL);
        FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

        inputDcFilter_.process(buffer, buffer);

        const int size = buffer.getSize();

        // Input leds.
        for (size_t i = 0; i < size; i++)
        {
            float l;
            if (patchCtrls_->looperResampling)
            {
                l = Mix2(inEnvFollower_[0]->process(resample_->getSamples(LEFT_CHANNEL)[i]), inEnvFollower_[1]->process(resample_->getSamples(RIGHT_CHANNEL)[i])) * kLooperResampleLedAtt;
            }
            else
            {
                l = Mix2(inEnvFollower_[0]->process(left[i]), inEnvFollower_[1]->process(right[i]));
            }
            patchState_->inputLevel[i] = l;
        }

        modulation_->Process();

        input_->copyFrom(buffer);
        input_->multiply(patchCtrls_->inputVol);

        if (patchCtrls_->looperResampling)
        {
            looper_->Process(*resample_, buffer);
        }
        else
        {
            looper_->Process(buffer, buffer);
        }
        buffer.add(*input_);

        sine_->Process(*osc1Out_);
        buffer.add(*osc1Out_);
        patchCtrls_->oscUseWavetable > 0.5f ? wt_->Process(*osc2Out_) : saw_->Process(*osc2Out_);
        buffer.add(*osc2Out_);

        buffer.multiply(kSourcesMakeupGain);

        if (patchCtrls_->filterPosition < 0.25f)
        {
            filterPosition_ = FilterPosition::POSITION_1;
        }
        else if (patchCtrls_->filterPosition >= 0.25f && patchCtrls_->filterPosition < 0.5f)
        {
            filterPosition_ = FilterPosition::POSITION_2;
        }
        else if (patchCtrls_->filterPosition >= 0.5f && patchCtrls_->filterPosition < 0.75f)
        {
            filterPosition_ = FilterPosition::POSITION_3;
        }
        else if (patchCtrls_->filterPosition >= 0.75f)
        {
            filterPosition_ = FilterPosition::POSITION_4;
        }

        if (filterPosition_ != lastFilterPosition_)
        {
            lastFilterPosition_ = filterPosition_;
            patchState_->filterPositionFlag = true;
        }
        else
        {
            patchState_->filterPositionFlag = false;
        }

        if (FilterPosition::POSITION_1 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }
        resonator_->process(buffer, buffer);
        if (FilterPosition::POSITION_2 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }
        echo_->process(buffer, buffer);
        if (FilterPosition::POSITION_3 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }
        ambience_->process(buffer, buffer);
        if (FilterPosition::POSITION_4 == filterPosition_)
        {
            filter_->process(buffer, buffer);
        }

        buffer.multiply(kOutputMakeupGain);
        limiter_->ProcessSoft(buffer, buffer);
        buffer.multiply(patchState_->outLevel);

        resample_->copyFrom(buffer);
    }
};

