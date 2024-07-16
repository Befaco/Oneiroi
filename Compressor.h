#pragma once

#include <cmath>
#include "Commons.h"

/**
 * https://git.iem.at/audioplugins/IEMPluginSuite/blob/master/resources/Compressor.h
 */
class Compressor
{
private:
    float sampleRate_;
    float threshold_;
    float ratio_, expo_;
    float attack_;
    float release_;

    // Internal variables
    float thrlin_;
    float cteAT_;
    float cteRL_;

    // State variables
    float leftS1_ = 0.f, rightS1_ = 0.f;

public:
    Compressor(float sampleRate)
    {
        sampleRate_ = sampleRate;

        setRatio(4.f);
        setAttack(1.f);
        setRelease(100.f);
        setThreshold(-10.f);
    };
    ~Compressor()
    {

    };

    static Compressor* create(float sampleRate)
    {
        return new Compressor(sampleRate);
    }

    static void destroy(Compressor* obj)
    {
        delete obj;
    }

    void setThreshold(float value)
    {
        threshold_ = value;
        thrlin_ = Db2A(threshold_);
    }

    void setRatio(float value)
    {
        ratio_ = value;
        expo_ = 1.f / ratio_ - 1.f;
    }

    void setAttack(float value)
    {
        attack_ = value;
        cteAT_ = exp (-2.f * M_PI * 1000.f / attack_ / sampleRate_);
    }

    void setRelease(float value)
    {
        release_ = value;
        cteRL_ = exp (-2.f * M_PI * 1000.f / release_ / sampleRate_);
    }

    float process(float input)
    {
        float leftSideInput = abs(input);

        // Ballistics filter and envelope generation
        float leftCte = (leftSideInput >= leftS1_ ? cteAT_ : cteRL_);
        float leftEnv = leftSideInput + leftCte * (leftS1_ - leftSideInput);
        leftS1_ = leftEnv;

        // Compressor transfer function
        float leftCv = (leftEnv <= thrlin_ ? 1.f : fast_powf(leftEnv / thrlin_, expo_));

        // Processing
        return input * leftCv;
    }

    void process(AudioBuffer &input, AudioBuffer &output)
    {
        int size = input.getSize();
        FloatArray leftIn = input.getSamples(0);
        FloatArray rightIn = input.getSamples(1);
        FloatArray leftOut = output.getSamples(0);
        FloatArray rightOut = output.getSamples(1);

        for (int i = 0; i < size; i++)
        {
            // Detector (peak)
            float leftSideInput = abs(leftIn[i]);
            float rightSideInput = abs(rightIn[i]);

            // Ballistics filter and envelope generation
            float leftCte = (leftSideInput >= leftS1_ ? cteAT_ : cteRL_);
            float leftEnv = leftSideInput + leftCte * (leftS1_ - leftSideInput);
            leftS1_ = leftEnv;

            float rightCte = (rightSideInput >= rightS1_ ? cteAT_ : cteRL_);
            float rightEnv = rightSideInput + rightCte * (rightS1_ - rightSideInput);
            rightS1_ = rightEnv;

            // Compressor transfer function
            float leftCv = (leftEnv <= thrlin_ ? 1.f : fast_powf(leftEnv / thrlin_, expo_));
            float rightCv = (rightEnv <= thrlin_ ? 1.f : fast_powf(rightEnv / thrlin_, expo_));

            // Processing
            leftOut[i] = leftIn[i] * leftCv;
            rightOut[i] = rightIn[i] * rightCv;
        }
    }
};
