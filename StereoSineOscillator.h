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

    float oldFreqs_[2];

    bool fadeOut_, fadeIn_;
    float fadeVolume_;

    void FadeOut()
    {
        fadeVolume_ -= kOscSineFadeInc;
        if (fadeVolume_ <= 0)
        {
            fadeVolume_ = 0;
            fadeOut_ = false;
            fadeIn_ = true;
        }
    }

    void FadeIn()
    {
        fadeVolume_ += kOscSineFadeInc;
        if (fadeVolume_ >= 1)
        {
            fadeVolume_ = 1;
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
        fadeVolume_ = 0;
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

        float u = patchCtrls_->oscUnison;
        if (patchCtrls_->oscUnison < 0)
        {
            u *= 0.5f;
        }
        //float p = fabsf(patchCtrls_->oscUnison);

        float f[2];
        f[0] = patchCtrls_->oscPitch;
        f[1] = patchCtrls_->oscPitch + patchCtrls_->oscPitch * u;
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

            //float out = oscs_[0]->generate() * (1.f - p) + oscs_[1]->generate() * p;
            float out = oscs_[0]->generate() + oscs_[1]->generate() * fadeVolume_;

            out *= patchCtrls_->osc1Vol * kOScSineGain;

            output.getSamples(LEFT_CHANNEL).setElement(i, out);
            output.getSamples(RIGHT_CHANNEL).setElement(i, out);
        }
    }
};