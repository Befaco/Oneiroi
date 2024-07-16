#pragma once

#include "Commons.h"
#include "Oscillator.h"

class EnvelopeFollowerMod : public OscillatorTemplate<EnvelopeFollowerMod>
{
private:
    PatchCtrls* patchCtrls_;
    PatchState* patchState_;

    float s_;

public:
    static constexpr float begin_phase = 0;
    static constexpr float end_phase = 1;

    EnvelopeFollowerMod(PatchCtrls* patchCtrls, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchState_ = patchState;
        s_ = 0;
    }
    ~EnvelopeFollowerMod() {}

    static EnvelopeFollowerMod* create(PatchCtrls* patchCtrls, PatchState* patchState)
    {
        return new EnvelopeFollowerMod(patchCtrls, patchState);
    }

    static void destroy(EnvelopeFollowerMod* obj)
    {
        delete obj;
    }

    float getSample()
    {
        return s_;
    }

    void setFrequency(float freq)
    {

    }

    float generate() override
    {
        // Smooth the envelope.
        float l = MapExpo(patchCtrls_->modSpeed, 0.f, 1.f, 0.998f, 0.85f);
        s_ = s_* l + Map(patchState_->inputLevel.getRms(), 0, 0.6f, -0.5f, 0.5f) * (1.f - l);

        return s_;
    }
};