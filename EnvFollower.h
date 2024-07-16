#pragma once

#include "SignalProcessor.h"

// Version of EnvelopeFollower with configurable lambda.
class EnvFollower : public SignalProcessor
{
private:
    float lambda_;
    float y_;

public:
    EnvFollower()
    {
        lambda_ = 0.995f;
        y_ = 0;
    }
    ~EnvFollower() {}

    void setLambda(float lambda)
    {
        lambda_ = lambda;
    }

    static EnvFollower* create()
    {
        return new EnvFollower();
    }
    static void destroy(EnvFollower* obj)
    {
        delete obj;
    }

    float process(float x)
    {
        float v = fabs(HardClip(x));

        y_ = y_ * lambda_ + v * (1.0f - lambda_);

        return Clamp(y_);
    }
};
