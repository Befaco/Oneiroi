#pragma once

#include "Commons.h"
#include "TapTempo.h"
#include "Schmitt.h"

class Clock
{
private:
    PatchCtrls* patchCtrls_;
    PatchState* patchState_;

    Schmitt trigger_;

    uint32_t samplesSinceSyncIn_;
    ClockSource clockSource_;
    bool firstSyncIn_;

public:
    Clock(PatchCtrls* patchCtrls, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchState_ = patchState;

        clockSource_ = ClockSource::CLOCK_SOURCE_EXTERNAL;

        patchState_->tempo = TapTempo::create(patchState_->blockRate, kLooperChannelBufferLength);
        patchState_->tempo->setFrequency(kInternalClockFreq);
        samplesSinceSyncIn_ = kExternalClockLimit;
    }
    ~Clock() {}

    static Clock* create(PatchCtrls* patchCtrls, PatchState* patchState)
    {
        return new Clock(patchCtrls, patchState);
    }

    static void destroy(Clock* obj)
    {
        delete obj;
    }

    void Process()
    {
        patchState_->tempo->clock(1);

        patchState_->clockReset = false;

        // Listen to sync in.
        if (patchState_->syncIn)
        {
            samplesSinceSyncIn_ = 0;
            firstSyncIn_ = true;
        }

        bool externalClock = samplesSinceSyncIn_ < kExternalClockLimit && firstSyncIn_;
        if (externalClock)
        {
            // We received a sync, keep listening.
            samplesSinceSyncIn_++;
        }
        else
        {
            // We didn't receive a sync, or too much time has passed.
            firstSyncIn_ = false;
        }

        if (ClockSource::CLOCK_SOURCE_EXTERNAL == patchState_->clockSource && !externalClock)
        {
            // It looks like that the external clock stopped or is too slow,
            // switch to the internal clock.
            patchState_->clockSource = ClockSource::CLOCK_SOURCE_INTERNAL;
            patchState_->tempo->setFrequency(kInternalClockFreq);
            patchState_->clockReset = true;
        }
        else if (ClockSource::CLOCK_SOURCE_INTERNAL == patchState_->clockSource && externalClock)
        {
            // Switch to the external clock.
            patchState_->clockSource = ClockSource::CLOCK_SOURCE_EXTERNAL;
            patchState_->clockReset = true;
        }

        size_t s = patchState_->tempo->getPeriodInSamples();
        if (fabs(patchState_->clockSamples - s) > kClockTempoSamplesMin)
        {
            patchState_->clockSamples = s;
        }

        patchState_->clockTick = trigger_.Process(patchState_->tempo->isOn());
    }
};
