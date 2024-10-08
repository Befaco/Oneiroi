#pragma once

#include "Commons.h"
#include "BiquadFilter.h"

class DjFilter
{
private:
    enum FilterType
    {
        NO_FILTER,
        LP,
        HP,
    };

    BiquadFilter* lpfs_[2];
    BiquadFilter* hpfs_[2];

    FilterType filter_ = FilterType::NO_FILTER;
    float freq_;
    float lpfMix_ = 1.f;
    float hpfMix_ = 0.f;

    void UpdateFilter()
    {
        switch (filter_)
        {
        case FilterType::LP:
            lpfs_[LEFT_CHANNEL]->setLowPass(freq_, FilterStage::SALLEN_KEY_Q);
            lpfs_[RIGHT_CHANNEL]->setLowPass(freq_, FilterStage::SALLEN_KEY_Q);
            break;
        case FilterType::HP:
            hpfs_[LEFT_CHANNEL]->setHighPass(freq_, FilterStage::SALLEN_KEY_Q);
            hpfs_[RIGHT_CHANNEL]->setHighPass(freq_, FilterStage::SALLEN_KEY_Q);
            break;

        default:
            break;
        }
    }

public:
    DjFilter(float sampleRate)
    {
        for (size_t i = 0; i < 2; i++)
        {
            lpfs_[i] = BiquadFilter::create(sampleRate);
            hpfs_[i] = BiquadFilter::create(sampleRate);
        }

        filter_ = FilterType::NO_FILTER;
    }
    ~DjFilter()
    {
        for (size_t i = 0; i < 2; i++)
        {
            BiquadFilter::destroy(lpfs_[i]);
            BiquadFilter::destroy(hpfs_[i]);
        }
    }

    static DjFilter* create(float sampleRate)
    {
        return new DjFilter(sampleRate);
    }

    static void destroy(DjFilter* obj)
    {
        delete obj;
    }

    void SetFilter(float value)
    {
        if (value <= 0.45f)
        {
            filter_ = FilterType::LP;
            lpfMix_ = Map(value, 0.f, 0.45f, 0.f, 1.f);
            freq_ = Map(lpfMix_, 0.f, 1.f, 500.f, 2000.f);
            UpdateFilter();
        }
        else if (value >= 0.55f)
        {
            filter_ = FilterType::HP;
            hpfMix_ = Map(value, 0.55f, 1.f, 0.f, 1.f);
            freq_ = Map(hpfMix_, 0.f, 1.f, 1000.f, 4000.f);
            UpdateFilter();
        }
        else
        {
            filter_ = FilterType::NO_FILTER;
        }
    }

    void Process(float leftIn, float rightIn, float &leftOut, float &rightOut)
    {
        switch (filter_)
        {
        case FilterType::LP:
            leftOut = LinearCrossFade(lpfs_[LEFT_CHANNEL]->process(leftIn), leftIn, lpfMix_);
            rightOut = LinearCrossFade(lpfs_[RIGHT_CHANNEL]->process(rightIn), rightIn, lpfMix_);
            break;
        case FilterType::HP:
            leftOut = LinearCrossFade(leftIn, hpfs_[LEFT_CHANNEL]->process(leftIn), hpfMix_);
            rightOut = LinearCrossFade(rightIn, hpfs_[RIGHT_CHANNEL]->process(rightIn), hpfMix_);
            break;
        default:
            leftOut = leftIn;
            rightOut = rightIn;
            break;
        }

        leftOut = Clamp(leftOut, -3.f, 3.f);
        rightOut = Clamp(rightOut, -3.f, 3.f);
    }

    void Process(AudioBuffer &input, AudioBuffer &output)
    {
        size_t size = output.getSize();

        FloatArray leftIn = input.getSamples(LEFT_CHANNEL);
        FloatArray rightIn = input.getSamples(RIGHT_CHANNEL);
        FloatArray leftOut = output.getSamples(LEFT_CHANNEL);
        FloatArray rightOut = output.getSamples(RIGHT_CHANNEL);

        for (size_t i = 0; i < size; i++)
        {
            Process(leftIn[i], rightIn[i], leftOut[i], rightOut[i]);
        }
    }
};