#pragma once

#include "Commons.h"

extern PatchProcessor* getInitialisingPatchProcessor();

enum ParamMidi {
    PARAM_MIDI_LOOPER_SPEED,
    PARAM_MIDI_LOOPER_START,
    PARAM_MIDI_LOOPER_LENGTH,
    PARAM_MIDI_LOOPER_SOS,
    PARAM_MIDI_LOOPER_FILTER,
    PARAM_MIDI_LOOPER_RECORDING,
    PARAM_MIDI_LOOPER_RESAMPLING,
    PARAM_MIDI_LOOPER_VOL,
    PARAM_MIDI_OSC_PITCH,
    PARAM_MIDI_OSC_DETUNE,
    PARAM_MIDI_OSC_UNISON,
    PARAM_MIDI_OSC1_VOL,
    PARAM_MIDI_OSC2_VOL,
    PARAM_MIDI_FILTER_CUTOFF,
    PARAM_MIDI_FILTER_RESONANCE,
    PARAM_MIDI_FILTER_MODE,
    PARAM_MIDI_FILTER_POSITION,
    PARAM_MIDI_FILTER_VOL,
    PARAM_MIDI_RESONATOR_TUNE,
    PARAM_MIDI_RESONATOR_FEEDBACK,
    PARAM_MIDI_RESONATOR_DISSONANCE,
    PARAM_MIDI_RESONATOR_VOL,
    PARAM_MIDI_ECHO_REPEATS,
    PARAM_MIDI_ECHO_DENSITY,
    PARAM_MIDI_ECHO_FILTER,
    PARAM_MIDI_ECHO_VOL,
    PARAM_MIDI_AMBIENCE_DECAY,
    PARAM_MIDI_AMBIENCE_SPACETIME,
    PARAM_MIDI_AMBIENCE_AUTOPAN,
    PARAM_MIDI_AMBIENCE_VOL,
    PARAM_MIDI_MOD_LEVEL,
    PARAM_MIDI_MOD_SPEED,
    PARAM_MIDI_MOD_TYPE,
    PARAM_MIDI_INPUT_VOL,
    PARAM_MIDI_RANDOM_MODE,
    PARAM_MIDI_RANDOM_AMOUNT,
    PARAM_MIDI_OSC_USE_SSWT,
    PARAM_MIDI_RANDOMIZE,
    PARAM_MIDI_LOOPER_SPEED_CV,
    PARAM_MIDI_LOOPER_START_CV,
    PARAM_MIDI_LOOPER_LENGTH_CV,
    PARAM_MIDI_OSC_PITCH_CV,
    PARAM_MIDI_OSC_DETUNE_CV,
    PARAM_MIDI_FILTER_CUTOFF_CV,
    PARAM_MIDI_RESONATOR_TUNE_CV,
    PARAM_MIDI_ECHO_DENSITY_CV,
    PARAM_MIDI_AMBIENCE_SPACETIME_CV,
    PARAM_MIDI_LAST
};

class MidiController
{
private:
    float* param_;

    uint8_t cc_;
    uint8_t value_;
    uint8_t channel_;
    uint8_t delta_;

    float offset_;
    float mult_;

public:
    MidiController(
        float* param,
        uint8_t cc,
        uint8_t channel,
        float offset,
        float mult,
        uint8_t delta
    ) {
        param_ = param;
        cc_ = cc;
        channel_ = channel;
        value_ = 0;
        offset_ = offset;
        mult_ = mult;
        delta_ = delta;
    }
    ~MidiController() {}

    static MidiController* create(
        float* param,
        uint8_t cc,
        uint8_t channel = 0,
        float offset = 0,
        float mult = 1,
        uint8_t delta = 1
    ) {
        return new MidiController(param, cc, channel, offset, mult, delta);
    }

    static void destroy(MidiController* obj)
    {
        delete obj;
    }

    inline void SetValue(float value)
    {
        *param_ = value;
    }

    // Called at block rate
    inline void Process()
    {
        uint8_t value = static_cast<uint8_t>(((*param_ + offset_) * mult_) * INT8_MAX);
        if (abs(value_ - value) > delta_)
        {
            value_ = value;
            getInitialisingPatchProcessor()->patch->sendMidi(MidiMessage::cc(channel_, cc_, value_));
        }
    }
};
