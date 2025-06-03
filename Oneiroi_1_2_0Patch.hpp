#ifndef __Oneiroi_1_2_0Patch_hpp__
#define __Oneiroi_1_2_0Patch_hpp__

#include "Commons.h"
#include "Ui.h"
#include "Clock.h"

class Oneiroi_1_2_0Patch : public Patch {
private:
    Ui* ui_;
    Oneiroi* oneiroi_;
    Clock* clock_;

    PatchCtrls patchCtrls;
    PatchCvs patchCvs;
    PatchState patchState;

public:
    Oneiroi_1_2_0Patch()
    {
        patchState.sampleRate = getSampleRate();
        patchState.blockRate = getBlockRate();
        patchState.blockSize = getBlockSize();
        ui_ = Ui::create(&patchCtrls, &patchCvs, &patchState);
        oneiroi_ = Oneiroi::create(&patchCtrls, &patchCvs, &patchState);
        clock_ = Clock::create(&patchCtrls, &patchState);
    }
    ~Oneiroi_1_2_0Patch()
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

#endif // __Oneiroi_1_2_0Patch_hpp__
