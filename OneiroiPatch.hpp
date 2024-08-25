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
        // technically can be null whilst updating sample rate
        if (oneiroi_ && clock_ && ui_) {
            clock_->Process();
            ui_->Poll();
            oneiroi_->Process(buffer);
        }
    }

    void updateSampleRateBlockSize(float newSampleRate, int blockSize) 
    {
        patchState.sampleRate = newSampleRate;        
        patchState.blockSize = blockSize;
        patchState.blockRate = newSampleRate / blockSize;

        // destroy and recreate Oneiroi, clock and UI
        Oneiroi::destroy(oneiroi_);
        oneiroi_ = Oneiroi::create(&patchCtrls, &patchCvs, &patchState);

        Clock::destroy(clock_);
        clock_ = Clock::create(&patchCtrls, &patchState);

        // causing hard to debug memory issues, ui not strongly affected by sample rates so leave for now
        // Ui::destroy(ui_);
        // ui_ = Ui::create(&patchCtrls, &patchCvs, &patchState);
    }

    PatchCtrls* getPatchCtrls() { return &patchCtrls; }
    PatchCvs* getPatchCvs() { return &patchCvs; }
    PatchState* getPatchState() { return &patchState; }
    Oneiroi* getOneiroi() { return oneiroi_; }
    Ui* getUi() { return ui_; }


};

#endif // __OneiroiPatch_hpp__
