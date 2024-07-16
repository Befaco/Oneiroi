#pragma once

#include "Commons.h"
#include "MorphingOscillator.h"
#include "NoiseOscillator.h"
#include "LorenzAttractor.h"
#include "EnvelopeFollowerMod.h"
#include "Schmitt.h"

enum ModulationSource
{
    MOD_SOURCE_LFO,
    MOD_SOURCE_INPUT,
};

enum Shape { LORENZ, SINE, RAMP, INVERTED_RAMP, SQUARE, SH, EF, NOF_SHAPES };

class Modulation
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    ModulationSource source_;
    MorphingOscillator* lfo_;

    Schmitt resetTrigger_, speedResetTrigger_, typeResetTrigger_;

    float prevType_;
    float prevFreq_;
    float phase_;

    bool reset_;
    bool speedReset_;
    bool typeReset_;
    bool freqReset_;

public:
    Modulation(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        source_ = ModulationSource::MOD_SOURCE_LFO;

        lfo_ = MorphingOscillator::create(NOF_SHAPES, patchState_->blockSize);
        lfo_->setOscillator(LORENZ, LorenzAttractor::create(patchState_->blockRate));
        lfo_->setOscillator(SINE, PhaseShiftOscillator<SineOscillator>::create(0, patchState_->blockRate));
        lfo_->setOscillator(INVERTED_RAMP, PhaseShiftOscillator<InvertedRampOscillator>::create(0, patchState_->blockRate));
        lfo_->setOscillator(RAMP, PhaseShiftOscillator<RampOscillator>::create(0, patchState_->blockRate));
        lfo_->setOscillator(SQUARE, PhaseShiftOscillator<SquareWaveOscillator>::create(0, patchState_->blockRate));
        lfo_->setOscillator(SH, NoiseOscillator::create(patchState_->blockRate));
        lfo_->setOscillator(EF, EnvelopeFollowerMod::create(patchCtrls_, patchState_));
        lfo_->setFrequency(kInternalClockFreq);
        lfo_->morph(0.f);

        prevType_ = 0;
        prevFreq_ = 0;
        phase_ = 0;

        reset_ = false;
        speedReset_ = false;
        typeReset_ = false;
        freqReset_ = false;
    }
    ~Modulation()
    {
        MorphingOscillator::destroy(lfo_);
    }

    static Modulation* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Modulation(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Modulation* obj)
    {
        delete obj;
    }

    void Process()
    {
        // 0 - 0.1667 - 0.3333 - 0.5 - 0.6667 - 0.8333 - 1
        if (patchCtrls_->modType <= 0.05f)
        {
            // Chaos.
            patchCtrls_->modType = 0;
            patchState_->modTypeLockFlag = true;
            typeReset_ = true;
        }
        else if (patchCtrls_->modType >= 0.1167f && patchCtrls_->modType < 0.2167f)
        {
            // Sine.
            patchCtrls_->modType = 0.1667f;
            patchState_->modTypeLockFlag = true;
            typeReset_ = true;
        }
        else if (patchCtrls_->modType >= 0.2833f && patchCtrls_->modType < 0.3833f)
        {
            // Ramp.
            patchCtrls_->modType = 0.3333f;
            patchState_->modTypeLockFlag = true;
            typeReset_ = true;
        }
        else if (patchCtrls_->modType >= 0.45f && patchCtrls_->modType < 0.55f)
        {
            // Saw.
            patchCtrls_->modType = 0.5f;
            patchState_->modTypeLockFlag = true;
            typeReset_ = true;
        }
        else if (patchCtrls_->modType >= 0.6167f && patchCtrls_->modType < 0.7167f)
        {
            // Square.
            patchCtrls_->modType = 0.6667f;
            patchState_->modTypeLockFlag = true;
            typeReset_ = true;
        }
        else if (patchCtrls_->modType >= 0.7833f && patchCtrls_->modType < 0.8833f)
        {
            // Random.
            patchCtrls_->modType = 0.8333f;
            patchState_->modTypeLockFlag = true;
            typeReset_ = true;
        }
        else if (patchCtrls_->modType >= 0.95f)
        {
            // Envelope.
            patchCtrls_->modType = 1.f;
            patchState_->modTypeLockFlag = true;
            typeReset_ = true;
        }
        else
        {
            patchState_->modTypeLockFlag = false;
        }

        lfo_->morph(patchCtrls_->modType);
        lfo_->setFrequency(prevFreq_);

        float s = patchCtrls_->modSpeed;

        // Speed deadband around 12 o'clock.
        if (s >= 0.47f && s <= 0.53f)
        {
            s = 0.5f;
            patchState_->modSpeedLockFlag = true;
            speedReset_ = true;
        }
        else
        {
            patchState_->modSpeedLockFlag = false;
            speedReset_ = false;
        }

        int i = QuantizeInt(s, kClockNofRatios);
        float f = Clamp(patchState_->tempo->getFrequency() * kModClockRatios[i], kClockFreqMin, kClockFreqMax);
        if (fabsf(f - prevFreq_) > 0.005f)
        {
            lfo_->setFrequency(f);
            prevFreq_ = f;
        }

        // Keep slow LFOs from drifting.
        if (patchState_->clockTick)
        {
            if (s >= 0.5f)
            {
                reset_ = true;
            }
            else if (s < 0.5f)
            {
                phase_++;
                if (phase_ == kRModClockRatios[i])
                {
                    reset_ = true;
                    phase_ = 0;
                }
            }
        }

        if (patchState_->clockReset)
        {
            reset_ = true;
        }

        // Wait for a clock tick in case we need to reset the LFO.
        //if (reset_ && patchState_->clockTick)
        if (resetTrigger_.Process(reset_) || speedResetTrigger_.Process(speedReset_) || typeResetTrigger_.Process(typeReset_))
        {
            lfo_->reset();
            reset_ = false;
        }

        float l = MapExpo(patchCtrls_->modLevel);
        patchState_->modValue = l > 0.02f ? lfo_->generate() * l : 0;
    }
};
