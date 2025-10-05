#pragma once

#include "Commons.h"
#include "DelayLine.h"
#include "BiquadFilter.h"
#include "EnvFollower.h"
#include "DcBlockingFilter.h"

class Pole
{
public:
    Pole(float sampleRate)
    {
        sampleRate_ = sampleRate;
        msr_ = sampleRate_ / 1000.f;

        for (size_t i = 0; i < 2; i++)
        {
            delays_[i] = DelayLine::create(kResoBufferSize);
            lpfs_[i] = BiquadFilter::create(sampleRate_);
            dc_[i] = DcBlockingFilter::create();
            ef_[i] = EnvFollower::create();
        }

        reso_ = FilterStage::BUTTERWORTH_Q;
        offset_ = 0;
        feedback_ = 0;
        filter_ = 0;
        detune_ = 0;
    }
    ~Pole()
    {
        for (size_t i = 0; i < 2; i++)
        {
            DelayLine::destroy(delays_[i]);
            BiquadFilter::destroy(lpfs_[i]);
            DcBlockingFilter::destroy(dc_[i]);
            EnvFollower::destroy(ef_[i]);
        }
    }

    static Pole* create(float sampleRate)
    {
        return new Pole(sampleRate);
    }

    static void destroy(Pole* pole)
    {
        delete pole;
    }

    float GetSemiOffset()
    {
        return offset_;
    }

    /**
     * @param offset Semitones, -24 to 24
     */
    void SetSemiOffset(float offset)
    {
        offset_ = offset * -0.5f + 17.667f;
        SetNote();
    }

    void SetFilter(float filter)
    {
        filter_ = filter;
        SetFreq();
    }

    void SetFeedback(float feedback)
    {
        feedback_ = feedback;

        // Infinite feedback.
        if (feedback_ > kResoInfiniteFeedbackThreshold)
        {
            feedback_ = kResoInfiniteFeedbackLevel;
        }
        else if (feedback_ > 0.99f)
        {
            feedback_ = 1.f;
        }
    }

    void SetDissonance(float detune)
    {
        detune_ = detune;
        SetNote();
    }

    void SetReso(float reso)
    {
        reso_ = reso;
        SetFreq();
    }

    // Process just one of the two channels.
    float Process(float in, int channel)
    {
        float out = lpfs_[channel]->process(outs_[channel]) * feedback_;

        float mix = HardClip(dc_[channel]->process(in + out));

        // Handle infinite feedback.
        if (feedback_ == kResoInfiniteFeedbackLevel)
        {
            mix *= 1.095f - ef_[channel]->process(mix);
        }

        delays_[channel]->write(mix);
        outs_[channel] = delays_[channel]->read(delayTimes_[channel]);

        return out;
    }

    void Process(float leftIn, float rightIn, float &leftOut, float &rightOut)
    {
        leftOut = lpfs_[LEFT_CHANNEL]->process(outs_[LEFT_CHANNEL]) * feedback_;
        rightOut = lpfs_[RIGHT_CHANNEL]->process(outs_[RIGHT_CHANNEL]) * feedback_;

        float leftMix = HardClip(dc_[LEFT_CHANNEL]->process(leftIn + leftOut));
        float rightMix = HardClip(dc_[RIGHT_CHANNEL]->process(rightIn + rightOut));

        // Handle infinite feedback.
        if (feedback_ == kResoInfiniteFeedbackLevel)
        {
            leftMix *= 1.095f - ef_[LEFT_CHANNEL]->process(leftMix);
            rightMix *= 1.095f - ef_[RIGHT_CHANNEL]->process(rightMix);
        }

        delays_[LEFT_CHANNEL]->write(leftMix);
        delays_[RIGHT_CHANNEL]->write(rightMix);

        outs_[LEFT_CHANNEL] = delays_[LEFT_CHANNEL]->read(delayTimes_[LEFT_CHANNEL]);
        outs_[RIGHT_CHANNEL] = delays_[RIGHT_CHANNEL]->read(delayTimes_[RIGHT_CHANNEL]);
    }

private:
    DelayLine *delays_[2];
    BiquadFilter *lpfs_[2];
    EnvFollower *ef_[2];
    DcBlockingFilter* dc_[2];
    float delayTimes_[2], outs_[2];

    float sampleRate_, msr_;
    float lf_, rf_;
    float reso_;
    float offset_;
    float feedback_;
    float filter_;
    float detune_;

    void SetFreq()
    {
        lpfs_[LEFT_CHANNEL]->setLowPass(M2F(lf_) + filter_, reso_);
        lpfs_[RIGHT_CHANNEL]->setLowPass(M2F(rf_) + filter_, reso_);
    }

    void SetNote()
    {
        lf_ = offset_ + detune_;
        rf_ = offset_ - detune_;

        delayTimes_[LEFT_CHANNEL] = Clamp(msr_ * Db2A(lf_), 0, kResoBufferSize);
        delayTimes_[RIGHT_CHANNEL] = Clamp(msr_ * Db2A(rf_), 0, kResoBufferSize);

        SetFreq();
    }
};

/**
 * @brief This is taken from my Reaktor ensemble Aerosynth.
 *        https://www.native-instruments.com/de/reaktor-community/reaktor-user-library/entry/show/3431/
 *
 */
class Resonator
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;
    Pole* poles_[3];

    EnvFollower *ef_[2];

    float amp_;
    float dryWet_;
    float range_;
    float tune_;
    float oldTuning_;
    int ranges_[3];

    int task_;

    /**
     * @param idx 0 - 3
     * @param offset -24/24
     */
    void SetSemiOffset(int idx, float offset)
    {
        if (idx == 0)
        {
            poles_[0]->SetSemiOffset(offset);
        }
        else if (idx == 1)
        {
            poles_[1]->SetSemiOffset(offset + poles_[1]->GetSemiOffset());
        }
        else if (idx == 2)
        {
            poles_[2]->SetSemiOffset(offset + poles_[2]->GetSemiOffset());
        }
    }

    void SetTune(float value)
    {
        tune_ = value;

        // Spread the updates in time.
        task_ = (task_ + 1) % 6;
        if (task_ == 0)
        {
            SetSemiOffset(0, Map(tune_, 0.f, 1.f, -24, ranges_[0]));
        }
        if (tune_ < 0.5f && task_ == 1)
        {
            SetSemiOffset(1, Map(tune_, 0.f, 0.5f, -12, ranges_[1]));
        }
        if (tune_ < 0.3f && task_ == 2)
        {
            SetSemiOffset(2, Map(tune_, 0.f, 0.3f, -6, ranges_[2]));
        }
        if (tune_ >= 0.3f && tune_ < 0.7f && task_ == 3)
        {
            SetSemiOffset(2, Map(tune_, 0.3f, 0.7f, -6, ranges_[2]));
        }
        if (tune_ >= 0.5f && task_ == 4)
        {
            SetSemiOffset(1, Map(tune_, 0.5f, 1.f, -12, ranges_[1]));
        }
        if (tune_ >= 0.7f && task_ == 5)
        {
            SetSemiOffset(2, Map(tune_, 0.7f, 1.f, -6, ranges_[2]));
        }
    }

    void SetFeedback(float value, bool init = false)
    {
        float feedback = Map(value, 0.f, 1.f, 0.85f, 1.f);
        float reso = Map(value, 0.f, 1.f, 0.5f, 0.6f);
        float filter = Map(value, 0.f, 1.f, 5000.f, 10000.f);
        amp_ = Map(value, 0.f, 1.f, kResoGainMax, kResoGainMin) * 0.577f;
        for (int i = 0; i < 3; i++)
        {
            poles_[i]->SetFeedback(feedback);
            poles_[i]->SetReso(reso);
            poles_[i]->SetFilter(filter);
        }
    }

    /*
    void SetRange(float range)
    {
        range_ = Map(range, 0.f, kOne, 0.1f, 1.f);
    }
    */

    void SetDissonance(float value)
    {
        ranges_[0] = Map(value, 0.f, 1.f, 24, 16);
        ranges_[1] = Map(value, 0.f, 1.f, 12, 7);
        ranges_[2] = Map(value, 0.f, 1.f, 6, 13);

        poles_[0]->SetDissonance(value);
        poles_[1]->SetDissonance(value * 2.f);
        poles_[2]->SetDissonance(value * 3.f);
    }

public:
    Resonator(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        for (int i = 0; i < 3; i++)
        {
            poles_[i] = Pole::create(patchState_->sampleRate);
        }

        for (size_t i = 0; i < 2; i++)
        {
            ef_[i] = EnvFollower::create();
        }

        amp_ = 1.f;
        range_ = 1.f;
        task_ = 0;

        SetDissonance(0);
        SetTune(0);
        SetFeedback(0);
    }
    ~Resonator()
    {
        for (int i = 0; i < 3; i++)
        {
            Pole::destroy(poles_[i]);
        }
        for (size_t i = 0; i < 2; i++)
        {
            EnvFollower::destroy(ef_[i]);
        }
    }

    static Resonator* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Resonator(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Resonator* obj)
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

        SetDissonance(patchCtrls_->resonatorDissonance);

        float t = Modulate(patchCtrls_->resonatorTune, patchCtrls_->resonatorTuneModAmount, patchState_->modValue, patchCtrls_->resonatorTuneCvAmount, patchCvs_->resonatorTune, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        ParameterInterpolator tuningParam(&oldTuning_, t, size);

        float f = Modulate(patchCtrls_->resonatorFeedback, patchCtrls_->resonatorFeedbackModAmount, patchState_->modValue, patchCtrls_->resonatorFeedbackCvAmount, patchCvs_->resonatorFeedback, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetFeedback(f);

        for (size_t i = 0; i < size; i++)
        {
            SetTune(tuningParam.Next());

            float lIn = Clamp(leftIn[i], -3.f, 3.f);
            float rIn = Clamp(rightIn[i], -3.f, 3.f);

            float left = poles_[1]->Process(lIn, LEFT_CHANNEL);
            float right = poles_[2]->Process(rIn, RIGHT_CHANNEL);

            float oLeft = left * 0.75f + right * 0.25f;
            float oRight = left * 0.25f + right * 0.75f;

            left = 0;
            right = 0;
            poles_[0]->Process(lIn, rIn, left, right);
            oLeft += left;
            oRight += right;

            oLeft *= 1.f - ef_[LEFT_CHANNEL]->process(oLeft);
            oRight *= 1.f - ef_[RIGHT_CHANNEL]->process(oRight);

            oLeft *= amp_;
            oRight *= amp_;

            leftOut[i] = CheapEqualPowerCrossFade(lIn, oLeft * kResoMakeupGain, patchCtrls_->resonatorVol, 1.4f);
            rightOut[i] = CheapEqualPowerCrossFade(rIn, oRight * kResoMakeupGain, patchCtrls_->resonatorVol, 1.4f);
        }
    }
};