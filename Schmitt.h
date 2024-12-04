#pragma once

/**
 * @brief A trigger (or gate) generator.
 *
 */
class Schmitt
{
public:
    Schmitt(bool gate = false, float thres = 0.3f) : thres_{thres}, g_{gate}
    {
        f_ = false;
        t_ = false;
        p_ = 0;
    }
    ~Schmitt() {}

    // If gate, returns true when value is equal or above the threshold, or
    // false when it's below. If trigger, returns true at the moment of value
    // reaching the threshold and then false.
    bool Process(float value)
    {
        f_ = value < p_;
        p_ = value;

        bool t = g_ ? t_ : false;
        if (!t_ && !f_ && value >= thres_)
        {
            t_ = true;
            t = true;
        }
        else if (f_ && value < thres_)
        {
            t_ = false;
        }

        return t;
    }

private:
    float thres_;
    bool g_, f_, t_;
    float p_;
};