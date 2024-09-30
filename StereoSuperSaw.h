#pragma once

#include "Commons.h"
#include "RampOscillator.h"

/**
 * @brief 7 sawtooth oscillators with detuning and mixing.
 *        Adapted from
 *        https://web.archive.org/web/20110627045129/https://www.nada.kth.se/utbildning/grukth/exjobb/rapportlistor/2010/rapporter10/szabo_adam_10131.pdf
 *
 */
class SuperSaw
{
private:
  AntialiasedRampOscillator* oscs_[7];
  float detunes_[7] = {};
  float volumes_[7] = {};
  float oldFreq_ = 0;

public:
    SuperSaw(float sampleRate)
    {
        for (size_t i = 0; i < 7; i++)
        {
            oscs_[i] = AntialiasedRampOscillator::create(sampleRate);
            detunes_[i] = 0;
            volumes_[i] = 0;
        }
    }
    ~SuperSaw()
    {
        for (size_t i = 0; i < 7; i++)
        {
            AntialiasedRampOscillator::destroy(oscs_[i]);
        }
    }

    void SetFreq(float value)
    {
        for (size_t i = 0; i < 7; i++)
        {
            oscs_[i]->setFrequency(value * detunes_[i]);
        }
    }

    void SetDetune(float value, bool minor = false)
    {
        float detune = value * 0.4f;

        if (minor)
        {
            detunes_[0] = 1 - detune * 0.877538f;
            detunes_[1] = 1 - detune * 0.66516f;
            detunes_[2] = 1 - detune * 0.318207f;
            detunes_[3] = 1;
            detunes_[4] = 1 + detune * 0.189207f;
            detunes_[5] = 1 + detune * 0.498307f;
            detunes_[6] = 1 + detune * 0.781797f;
        }
        else
        {
            detunes_[0] = 1 - detune * 0.11002313f;
            detunes_[1] = 1 - detune * 0.06288439f;
            detunes_[2] = 1 - detune * 0.01952356f;
            detunes_[3] = 1;
            detunes_[4] = 1 + detune * 0.01991221f;
            detunes_[5] = 1 + detune * 0.06216538f;
            detunes_[6] = 1 + detune * 0.10745242f;
        }

        value = Clamp(value * 0.5f, 0.005f, 0.5f);

        float y = -0.73764f * fast_powf(value, 2.f) + 1.2841f * value + 0.044372f;
        volumes_[0] = y;
        volumes_[1] = y;
        volumes_[2] = y;
        volumes_[3] = -0.55366f * value + 0.99785f;
        volumes_[4] = y;
        volumes_[5] = y;
        volumes_[6] = y;
    }

    static SuperSaw* create(float sampleRate)
    {
        return new SuperSaw(sampleRate);
    }

    static void destroy(SuperSaw* obj)
    {
        delete obj;
    }

    void Process(float freq, FloatArray output)
    {
        size_t size = output.getSize();

        ParameterInterpolator freqParam(&oldFreq_, freq * 0.5f, size);

        for (size_t i = 0; i < size; i++)
        {
            SetFreq(freqParam.Next());
            for (size_t j = 0; j < 7; j++)
            {
                output[i] += oscs_[j]->generate() * volumes_[j];
            }
        }

        output.multiply(0.378f);
    }
};

class StereoSuperSaw
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;
    SuperSaw* saws_[2];

    void SetDetune(float value, bool minor = false)
    {
        for (int i = 0; i < 2; i++)
        {
            saws_[i]->SetDetune(value);
        }
    }

public:
    StereoSuperSaw(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        for (int i = 0; i < 2; i++)
        {
            saws_[i] = SuperSaw::create(patchState_->sampleRate);
        }
    }
    ~StereoSuperSaw()
    {
        for (int i = 0; i < 2; i++)
        {
            SuperSaw::destroy(saws_[i]);
        }
    }

    static StereoSuperSaw* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new StereoSuperSaw(patchCtrls, patchCvs, patchState);
    }

    static void destroy(StereoSuperSaw* obj)
    {
        delete obj;
    }

    void Process(AudioBuffer &output)
    {
        float u = patchCtrls_->oscUnison;
        if (patchCtrls_->oscUnison < 0)
        {
            u *= 0.5f;
        }

        float f = Modulate(patchCtrls_->oscPitch + patchCtrls_->oscPitch * u, patchCtrls_->oscPitchModAmount, patchState_->modValue, 0, 0, kOscFreqMin, kOscFreqMax);

        float d = Modulate(patchCtrls_->oscDetune, patchCtrls_->oscDetuneModAmount, patchState_->modValue, patchCtrls_->oscDetuneCvAmount, patchCvs_->oscDetune);
        SetDetune(d);

        saws_[LEFT_CHANNEL]->Process(f, output.getSamples(LEFT_CHANNEL));
        saws_[RIGHT_CHANNEL]->Process(f, output.getSamples(RIGHT_CHANNEL));

        output.multiply(patchCtrls_->osc2Vol * kOScSuperSawGain);
    }
};
