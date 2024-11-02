#pragma once

#include "Commons.h"
#include "SineOscillator.h"
#include "Schmitt.h"

class StereoSineOscillator
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    SineOscillator* oscs_[2];

    Schmitt trigger_;

    float oldFreqs_[2] = {};

    bool fadeOut_, fadeIn_;
    float sine1Volume_, sine2Volume_;

    void FadeOut()
    {
        sine2Volume_ -= kOscSineFadeInc;
        if (sine2Volume_ <= 0)
        {
            sine2Volume_ = 0;
            fadeOut_ = false;
            fadeIn_ = true;
        }
    }

    void FadeIn()
    {
        sine2Volume_ += kOscSineFadeInc;
        if (sine2Volume_ >= 0.5f)
        {
            sine2Volume_ = 0.5f;
            fadeIn_ = false;
        }
    }

public:
    StereoSineOscillator(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* PatchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = PatchState;

        for (size_t i = 0; i < 2; i++)
        {
            oscs_[i] = SineOscillator::create(patchState_->sampleRate);
        }

        fadeOut_ = false;
        fadeIn_ = false;
        sine1Volume_ = 0.5f;
        sine2Volume_ = 0.5f;
    }
    ~StereoSineOscillator()
    {
        for (size_t i = 0; i < 2; i++)
        {
            SineOscillator::destroy(oscs_[i]);
        }
    }

    static StereoSineOscillator* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* PatchState)
    {
        return new StereoSineOscillator(patchCtrls, patchCvs, PatchState);
    }

    static void destroy(StereoSineOscillator* obj)
    {
        delete obj;
    }

    void Process(AudioBuffer &output)
    {
        size_t size = output.getSize();

        float u;
        if (patchCtrls_->oscUnison < 0)
        {
            u = Map(patchCtrls_->oscUnison, -1.f, 0.f, 0.5f, 1.f);
        }
        else
        {
            u = Map(patchCtrls_->oscUnison, 0.f, 1.f, 1.f, 2.f);
        }

        float f[2];
        f[0] = Clamp(patchCtrls_->oscPitch, kOscFreqMin, kOscFreqMax);
        f[1] = Clamp(f[0] * u, kOscFreqMin, kOscFreqMax);
        ParameterInterpolator freqParams[2] = {ParameterInterpolator(&oldFreqs_[0], f[0], size), ParameterInterpolator(&oldFreqs_[1], f[1], size)};

        for (size_t i = 0; i < size; i++)
        {
            for (size_t j = 0; j < 2; j++)
            {
                oscs_[j]->setFrequency(freqParams[j].Next());
            }

            if (trigger_.Process(patchState_->oscUnisonCenterFlag) && !fadeOut_)
            {
                fadeOut_ = true;
            }
            if (fadeOut_)
            {
                FadeOut();
            }
            else if (fadeIn_)
            {
                oscs_[1]->setPhase(oscs_[0]->getPhase());
                FadeIn();
            }

            float out = oscs_[0]->generate() * sine1Volume_ + oscs_[1]->generate() * sine2Volume_;

            out *= patchCtrls_->osc1Vol * kOScSineGain;

            output.getSamples(LEFT_CHANNEL).setElement(i, out);
            output.getSamples(RIGHT_CHANNEL).setElement(i, out);
        }
    }
};