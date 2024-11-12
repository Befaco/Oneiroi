#pragma once

#include "Commons.h"
#include "StateVariableFilter.h"
#include "BiquadFilter.h"
#include "ChaosNoise.h"
#include "DcBlockingFilter.h"
#include "EnvFollower.h"

enum FilterMode
{
    LP,
    BP,
    HP,
    CF,
};

enum FilterPosition
{
    POSITION_1,
    POSITION_2,
    POSITION_3,
    POSITION_4,
};

class Allpass
{
private:
    FloatArray line_;
    int s_, w_;
    float d_, c_, sr_;

public:
    Allpass(float sampleRate, int size)
    {
        sr_ = sampleRate;
        s_ = size;
        line_ = FloatArray::create(size);
        w_ = 0;
        d_ = 1.f;
        c_ = 0.7f;
    }
    ~Allpass()
    {
        FloatArray::destroy(line_);
    }

    static Allpass* create(float sampleRate, int size)
    {
        return new Allpass(sampleRate, size);
    }

    static void destroy(Allpass* obj)
    {
        delete obj;
    }

    void SetDelay(float d)
    {
        d_ = d;
    }

    void SetC(float c)
    {
        c_ = c;
    }

    inline float readAt(int index)
    {
        int r = w_ - index;
        if (r < 0)
        {
            r += s_;
        }

        return line_.getElement(r);
    }

    float Process(float in)
    {
        size_t idx = (size_t)d_;
        float y0 = readAt(idx);
        float y1 = readAt(idx + 1);
        float frac = d_ - idx;

        float out = Interpolator::linear(y0, y1, frac) + (-c_ * in);

        line_.setElement(w_, in + (c_ * out));

        w_ = (w_ + 1) % s_;

        return out;
    }

    float ProcessFixed(float in)
    {
        float out = 0;
        float delayedSample = line_.getElement(w_);

        out += c_ * delayedSample;
        out += in - c_ * out;

        line_.setElement(w_, out);
        w_ = (w_ + 1) % s_;

        return out;
    }
};

class CombFilter
{
private:
    Allpass* poles_[4];
    EnvFollower* ef_;
    float sampleRate_, reso_, out_;

public:
    CombFilter(float sampleRate)
    {
        sampleRate_ = sampleRate;
        poles_[0] = Allpass::create(sampleRate, 2); // Fixed
        poles_[1] = Allpass::create(sampleRate, 9600); // Variable
        poles_[2] = Allpass::create(sampleRate, 2); // Fixed
        poles_[3] = Allpass::create(sampleRate, 9600); // Variable
        ef_ = EnvFollower::create();
        reso_ = 0;
        out_ = 0;
    }
    ~CombFilter()
    {
        for (size_t i = 0; i < 4; i++)
        {
            Allpass::destroy(poles_[i]);
        }
        EnvFollower::destroy(ef_);
    }

    static CombFilter* create(float sampleRate)
    {
        return new CombFilter(sampleRate);
    }

    static void destroy(CombFilter* obj)
    {
        delete obj;
    }

    void SetFrequency(float freq)
    {
        float d = sampleRate_ / freq;
        //poles_[0]->SetDelay(d);
        poles_[1]->SetDelay(d);
        //poles_[2]->SetDelay(d);
        poles_[3]->SetDelay(d + d);
    }

    void SetResonance(float reso)
    {
        reso_ = reso;
    }

    float Process(float in)
    {
        float i = in + reso_ * out_;
        i *= 1.f - ef_->process(i);

        float o = poles_[0]->ProcessFixed(i);
        o = poles_[1]->Process(o);
        o = poles_[2]->ProcessFixed(o);
        o = poles_[3]->Process(o);

        out_ = o;

        return out_;
    }
};

class Filter
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;
    StateVariableFilter* filters_[2];
    BiquadFilter* bpf_[2];
    CombFilter* combs_[2];
    ChaosNoise noise_;
    FilterMode mode_, lastMode_;
    DcBlockingFilter* dc_[2];
    EnvFollower* ef_[2];

    float drive_ = 0.f;
    float freq_ = 0.f;
    float reso_ = 0.f, resoValue_ = 0.f;
    float amp_ = 0.f;
    float filterGain_ = 0.f;
    // float dryWet_ = 0.f;
    float noiseLevel_ = 0.f;
    // float feedback_ = 0.f;

    void SetMode(float value)
    {
        FilterMode mode;

        if (value >= 0.75f)
        {
            mode = FilterMode::CF;
        }
        else if (value >= 0.5f)
        {
            mode = FilterMode::HP;
        }
        else if (value >= 0.25f)
        {
            mode = FilterMode::BP;
        }
        else
        {
            mode = FilterMode::LP;
        }

        if (mode == mode_)
        {
            return;
        }

        mode_ = mode;
    }

    void SetFreq(float value)
    {
        filterGain_ = kFilterLpGainMin;

        float cutoff = Clamp(MapLog(value, 0.f, 1.f, 10.f, 22000.f), 10.f, 22000.f);

        switch (mode_)
        {
        case FilterMode::LP:
            {
                filters_[LEFT_CHANNEL]->setLowPass(cutoff, reso_);
                filters_[RIGHT_CHANNEL]->setLowPass(cutoff, reso_);
                // Shut the filter off when the frequency is really low.
                float g = MapExpo(resoValue_, 0.f, 1.f, kFilterLpGainMax, kFilterLpGainMin);
                filterGain_ = cutoff <= 15.f ? Map(cutoff, 10.f, 15.f, 0.f, g) : g;
                break;
            }
        case FilterMode::BP:
            bpf_[LEFT_CHANNEL]->setBandPass(cutoff, reso_);
            bpf_[RIGHT_CHANNEL]->setBandPass(cutoff, reso_);
            filterGain_ = MapExpo(resoValue_, 0.f, 1.f, kFilterBpGainMin, kFilterBpGainMax);
            break;
        case FilterMode::HP:
            {
                filters_[LEFT_CHANNEL]->setHighPass(cutoff, reso_);
                filters_[RIGHT_CHANNEL]->setHighPass(cutoff, reso_);
                // Shut the filter off when the frequency is really high.
                float g = MapExpo(resoValue_, 0.f, 1.f, kFilterHpGainMax, kFilterHpGainMin);
                filterGain_ = cutoff >= 20000.f ? Map(cutoff, 15000, 20000, g, 0.f) : g;
                break;
            }
        case FilterMode::CF:
            float f = Clamp(Map(value, 0.f, 1.f, 100.f, 15000.f), 100.f, 15000.f);
            float r = Clamp(VariableCrossFade(0.f, 0.8f, resoValue_, 0.9f), 0.f, 0.8f);
            combs_[LEFT_CHANNEL]->SetFrequency(f);
            combs_[LEFT_CHANNEL]->SetResonance(r);
            combs_[RIGHT_CHANNEL]->SetFrequency(f);
            combs_[RIGHT_CHANNEL]->SetResonance(r);
            filterGain_ = MapExpo(resoValue_, 0.f, 1.f, kFilterCombGainMax, kFilterCombGainMin);
            break;
        }
        noise_.SetFreq(cutoff);
    }

    void SetReso(float value)
    {
        resoValue_ = Clamp(value);
        reso_ = VariableCrossFade(0.5f, 10.f, value, 0.9f);
        drive_ = VariableCrossFade(0.f, 0.02f, value, 0.15f, 0.9f);
        noiseLevel_ = VariableCrossFade(0.f, 0.1f, value, 0.15f, 0.9f);
    }

public:
    Filter(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        noise_.Init(patchState_->sampleRate);
        noise_.SetChaos(kFilterChaosNoise);

        for (size_t i = 0; i < 2; i++)
        {
            filters_[i] = StateVariableFilter::create(patchState_->sampleRate);
            bpf_[i] = BiquadFilter::create(patchState_->sampleRate);
            combs_[i] = CombFilter::create(patchState_->sampleRate);
            dc_[i] = DcBlockingFilter::create();
            ef_[i] = EnvFollower::create();
        }

        mode_ = lastMode_ = FilterMode::LP;
        freq_ = 22000.f;
        amp_ = Db2A(120);
    }
    ~Filter()
    {
        for (size_t i = 0; i < 2; i++)
        {
            StateVariableFilter::destroy(filters_[i]);
            BiquadFilter::destroy(bpf_[i]);
            CombFilter::destroy(combs_[i]);
            DcBlockingFilter::destroy(dc_[i]);
            EnvFollower::destroy(ef_[i]);
        }
    }

    static Filter* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Filter(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Filter* obj)
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

        SetMode(patchCtrls_->filterMode);
        if (mode_ != lastMode_)
        {
            lastMode_ = mode_;
            patchState_->filterModeFlag = true;
        }
        else
        {
            patchState_->filterModeFlag = false;
        }

        float r = Modulate(patchCtrls_->filterResonance, patchCtrls_->filterResonanceModAmount, patchState_->modValue, patchCtrls_->filterResonanceCvAmount, patchCvs_->filterResonance, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetReso(r);

        float c = Modulate(patchCtrls_->filterCutoff, patchCtrls_->filterCutoffModAmount, patchState_->modValue, patchCtrls_->filterCutoffCvAmount, patchCvs_->filterCutoff, -1.f, 1.f, patchState_->modAttenuverters, patchState_->cvAttenuverters);
        SetFreq(c);

        for (size_t i = 0; i < size; i++)
        {
            float n = noise_.Process() * noiseLevel_;

            float lIn = Clamp(leftIn[i], -3.f, 3.f);
            float rIn = Clamp(rightIn[i], -3.f, 3.f);

            float ls = SoftClip(lIn * amp_ + n);
            float rs = SoftClip(rIn * amp_ + n);

            float lf = LinearCrossFade(lIn + n, ls, drive_);
            float rf = LinearCrossFade(rIn + n, rs, drive_);

            float lo, ro;
            if (FilterMode::CF == mode_)
            {
                lo = HardClip(combs_[LEFT_CHANNEL]->Process(lf) * filterGain_);
                ro = HardClip(combs_[RIGHT_CHANNEL]->Process(rf) * filterGain_);
                lo = dc_[LEFT_CHANNEL]->process(lo);
                ro = dc_[RIGHT_CHANNEL]->process(ro);
            }
            else if (FilterMode::BP == mode_)
            {
                lo = bpf_[LEFT_CHANNEL]->process(lf) * filterGain_;
                ro = bpf_[RIGHT_CHANNEL]->process(rf) * filterGain_;
                lo *= 1.f - ef_[LEFT_CHANNEL]->process(lo);
                ro *= 1.f - ef_[RIGHT_CHANNEL]->process(ro);
            }
            else
            {
                lo = filters_[LEFT_CHANNEL]->process(lf) * filterGain_;
                ro = filters_[RIGHT_CHANNEL]->process(rf) * filterGain_;
                lo *= 1.f - ef_[LEFT_CHANNEL]->process(lo);
                ro *= 1.f - ef_[RIGHT_CHANNEL]->process(ro);
            }

            leftOut[i] = CheapEqualPowerCrossFade(lIn, lo * kFilterMakeupGain, patchCtrls_->filterVol);
            rightOut[i] = CheapEqualPowerCrossFade(rIn, ro * kFilterMakeupGain, patchCtrls_->filterVol);
        }
    }
};