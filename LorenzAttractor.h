#pragma once

#include "Commons.h"
#include "Oscillator.h"

/**
 * @brief Lorenz attractor.
 *        https://en.wikipedia.org/wiki/Lorenz_system
 *        With adaptations taken from https://github.com/belangeo/pyo/blob/master/src/objects/oscilmodule.c
 *
 */
class LorenzAttractor : public OscillatorTemplate<LorenzAttractor>
{
private:
    float sampleRate_;
    float x_, y_, z_, a_, b_, c_, t_, s_;
    float xAtt_, yAtt_, zAtt_, max_, min_;

public:
    static constexpr float begin_phase = 0;
    static constexpr float end_phase = 1;

    LorenzAttractor(float sampleRate) : sampleRate_{sampleRate}
    {
        x_ = y_ = z_ = 1.f;
        //xAtt_ = 0.044f;
        xAtt_ = 1.f;
        yAtt_ = 0.0328f;
        zAtt_ = 0.0078125f;
        max_ = 0; min_ = 0;

        setType(LorenzAttractor::Type::TYPE_TORUS);
        setFrequency(1.f);
    }
    ~LorenzAttractor() {}

    static LorenzAttractor* create(float sr)
    {
        return new LorenzAttractor(sr);
    }

    static void destroy(LorenzAttractor* obj)
    {
        delete obj;
    }

    enum class Type
    {
        TYPE_DEFAULT,
        TYPE_STABLE,
        TYPE_ELLIPSE,
        TYPE_TORUS, // Unstable
        TYPE_LAST,
    };

    void setXAtt(float att)
    {
        xAtt_ = att;
    }

    void setYAtt(float att)
    {
        yAtt_ = att;
    }

    /**
     * @param a 1.f/100.f
     */
    void SetA(float a)
    {
        a_ = a;
    }

    /**
     * @param b For small values, the system is stable and evolves to one of
     *        two fixed point attractors. When > 24.74f, the fixed points
     *        become repulsors and the trajectory is repelled by them in a
     *        very complex way.
     */
    void SetB(float b)
    {
        b_ = b;
    }

    /**
     * @param c Chaos, 1.f/10.f, optimal values in range 0.5f/3.f
     */
    void setChaos(float c)
    {
        c_ = c;
    }

    void setType(LorenzAttractor::Type type)
    {
        switch (type)
        {
        case LorenzAttractor::Type::TYPE_DEFAULT:
            a_ = 10.f;
            b_ = 28.f;
            c_ = 8.f / 3.f;
            break;
        case LorenzAttractor::Type::TYPE_STABLE:
            a_ = 10.f;
            b_ = 17.f;
            c_ = 8.f / 3.f;
            break;
        case LorenzAttractor::Type::TYPE_ELLIPSE:
            a_ = 10.f;
            b_ = 20.f;
            c_ = 8.f / 3.f;
            break;
        case LorenzAttractor::Type::TYPE_TORUS:
            a_ = 10.f;
            b_ = 99.96f;
            c_ = 8.f / 3.f;
            break;

        default:
            a_ = 10.f;
            b_ = 28.f;
            c_ = 8.f / 3.f;
            break;
        }
    }

    float getSample()
    {
        return s_;
    }

    void setFrequency(float freq)
    {
        freq *= 0.5f;
        t_ = 1.f / (sampleRate_ / Clamp(freq, 0.001, 10.f));
    }

    void process()
    {
        float xt = x_ + t_ * a_ * (y_ - x_);
        float yt = y_ + t_ * (x_ * (b_ - z_) - y_);
        float zt = z_ + t_ * (x_ * y_ - c_ * z_);

        x_ = xt;
        y_ = yt;
        z_ = zt;
    }

    float generate() override
    {
        process();

        max_ = Max(max_, x_);
        min_ = Min(min_, x_);

        s_ = Map(x_, min_, max_, -xAtt_, xAtt_);

        return s_;
    }

    void generate(FloatArray xOut, FloatArray yOut)
    {
        for (size_t i = 0; i < xOut.getSize(); i++)
        {
            process();

            xOut[i] = Map(x_, -20.f, 50.f, -xAtt_, xAtt_);
            yOut[i] = Map(y_, -20.f, 50.f, -yAtt_, yAtt_);
        }
    }
};