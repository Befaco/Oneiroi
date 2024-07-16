#ifndef __OneiroiPatch_hpp__
#define __OneiroiPatch_hpp__

#include "Commons.h"
#include "Ui.h"
#include "Clock.h"

class OneiroiPatch : public Patch {
private:
    Ui* ui_;
    Oneiroi* oneiroi_;
    Clock* clock_;

    PatchCtrls patchCtrls;
    PatchCvs patchCvs;
    PatchState patchState;

public:
    OneiroiPatch()
    {
        patchState.sampleRate = getSampleRate();
        patchState.blockRate = getBlockRate();
        patchState.blockSize = getBlockSize();
        ui_ = Ui::create(&patchCtrls, &patchCvs, &patchState);
        oneiroi_ = Oneiroi::create(&patchCtrls, &patchCvs, &patchState);
        clock_ = Clock::create(&patchCtrls, &patchState);
    }
    ~OneiroiPatch()
    {
        Oneiroi::destroy(oneiroi_);
        Ui::destroy(ui_);
        Clock::destroy(clock_);
    }

    void buttonChanged(PatchButtonId bid, uint16_t value, uint16_t samples) override
    {
        ui_->ProcessButton(bid, value, samples);
    }

    void processMidi(MidiMessage msg) override
    {
        ui_->ProcessMidi(msg);
    }

    void processAudio(AudioBuffer& buffer) override
    {
        clock_->Process();
        ui_->Poll();
        oneiroi_->Process(buffer);
    }
};

#endif // __OneiroiPatch_hpp__
