#pragma once

#include <stdlib.h>

class ParameterInterpolator
{
private:
    float* state_;
    float value_;
    float increment_;

public:
    ParameterInterpolator() {}
    ParameterInterpolator(float* state, float new_value, size_t size)
    {
        Init(state, new_value, size);
    }

    ParameterInterpolator(float* state, float new_value, float step)
    {
        state_ = state;
        value_ = *state;
        increment_ = (new_value - *state) * step;
    }

    ~ParameterInterpolator()
    {
        *state_ = value_;
    }

    inline void Init(float* state, float new_value, size_t size)
    {
        state_ = state;
        value_ = *state;
        increment_ = (new_value - *state) / static_cast<float>(size);
    }

    inline float Next()
    {
        value_ += increment_;

        return value_;
    }

    inline float subsample(float t)
    {
        return value_ + increment_ * t;
    }
};