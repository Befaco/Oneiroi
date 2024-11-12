#pragma once

#include "Oneiroi.h"
#include "ParamController.h"
#include "Midi.h"
#include "Led.h"
#include "VoltsPerOctave.h"
#include "SquareWaveOscillator.h"
#include "Schmitt.h"
#include "MidiMessage.h"

enum RandomMode
{
    RANDOM_ALL,
    RANDOM_OSC,
    RANDOM_LOOPER,
    RANDOM_EFFECTS
};

enum RecordingState
{
    RECORDING_STATE_IDLE,
    RECORDING_STATE_START,
    RECORDING_STATE_WAITING_ONSET,
    RECORDING_STATE_RECORDING,
    RECORDING_STATE_STOP,
};

enum FuncState
{
    FUNC_STATE_IDLE,
    FUNC_STATE_PRESSED,
    FUNC_STATE_HELD,
    FUNC_STATE_RELEASED,
};

struct Configuration
{
  uint32_t voct1_scale; // For C0-C4 range
  uint32_t voct1_offset;
  uint32_t voct2_scale; // For C5-C9 range
  uint32_t voct2_offset;
  bool soft_takeover;
  bool mod_attenuverters;
  bool cv_attenuverters;
  uint16_t c5;
  uint16_t pitch_zero;
  uint16_t speed_zero;
  uint16_t params_min[40];
  uint16_t params_max[40];
};

// extern PatchProcessor* getInitialisingPatchProcessor();

class Ui
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;

    KnobController* knobs_[PARAM_KNOB_LAST];
    FaderController* faders_[PARAM_FADER_LAST];
    CvController* cvs_[PARAM_CV_LAST];
    SwitchController* switches_[PARAM_SWITCH_LAST];
    RecordButtonController* recordButton_;
    RandomButtonController* randomButton_;
public:
    ShiftButtonController* shiftButton_;
    ModCvButtonController* modCvButton_;
    Led* leds_[LED_LAST];
private:
    MidiController* midiOuts_[PARAM_MIDI_LAST];

    CatchUpController* movingParam_;

    RecordingState recordingState_;
    RandomMode randomMode_;
    RandomAmount randomAmount_;

    Schmitt clearLooperTrigger_, looperSpeedLockTrigger_, oscPitchCenterTrigger_, oscOctaveTrigger_, oscUnisonCenterTrigger_;
    Schmitt undoRedoRandomTrigger_, recordAndRandomTrigger_, modTypeLockTrigger_, modSpeedLockTrigger_, saveTrigger_;
    Schmitt filterModeTrigger_, filterPositionTrigger_;

    HysteresisQuantizer octaveQuantizer_;

    int samplesSinceShiftPressed_, samplesSinceRecordOrRandomPressed_, samplesSinceModCvPressed_, samplesSinceRecordInReceived_, samplesSinceRecordingStarted_, samplesSinceRandomPressed_;

    const int kResetLimit;

public:
    bool wasCvMap_, recordAndRandomPressed_, recordPressed_, fadeOutOutput_, fadeInOutput_, parameterChangedSinceLastSave_, saving_, saveFlag_, startup_, undoRedo_, doRandomSlew_;

    int lastOctave_, randomizeTask_;

    float octave_, tune_, vOctScale1_, vOctOffset1_, vOctScale2_, vOctOffset2_, unison_, looperVol_, osc1Vol_, osc2Vol_, inputVol_, noteCv_, notePot_, randomize_, randomSlewInc_;

public:
    Ui(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState) : kResetLimit(kResetLimitSeconds * patchState->blockRate)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        octaveQuantizer_.Init(8, 0.f, false);

        movingParam_ = NULL;

        recordingState_ = RecordingState::RECORDING_STATE_IDLE;
        randomMode_ = RandomMode::RANDOM_ALL;
        randomAmount_ = RandomAmount::RANDOM_HIGH;

        samplesSinceShiftPressed_ = 0;
        samplesSinceRecordOrRandomPressed_ = 0;
        samplesSinceModCvPressed_ = 0;
        samplesSinceRecordInReceived_ = 0;
        samplesSinceRecordingStarted_ = 0;
        samplesSinceRandomPressed_ = 0;

        wasCvMap_ = false;
        recordAndRandomPressed_ = false;
        recordPressed_ = false; // USE_RECORD_THRESHOLD
        fadeOutOutput_ = false;
        fadeInOutput_ = false;
        parameterChangedSinceLastSave_ = false;
        saving_ = false;
        saveFlag_ = false;
        startup_ = true;
        randomize_ = false;
        undoRedo_ = false;
        doRandomSlew_ = false;

        lastOctave_ = 0;
        randomizeTask_ = 0;

        octave_ = 0;
        tune_ = 0;
        vOctScale1_ = 0;
        vOctOffset1_ = 0;
        vOctScale2_ = 0;
        vOctOffset2_ = 0;
        unison_ = 0;
        looperVol_ = 0;
        osc1Vol_ = 0;
        osc2Vol_ = 0;
        inputVol_ = 0;
        noteCv_ = 0;
        notePot_ = 0;
        randomSlewInc_ = 0;

        patchState_->funcMode = FuncMode::FUNC_MODE_NONE;
        patchState_->inputLevel = FloatArray::create(patchState_->blockSize);
        patchState_->efModLevel = FloatArray::create(patchState_->blockSize);
        patchState_->outLevel = 1.f;
        patchState_->randomSlew = kRandomSlewSamples;
        patchState_->randomHasSlew = false;
        patchState_->clockSamples = 0;

        // Alt params
        patchCtrls_->looperSos = 0.f;
        patchCtrls_->looperFilter = 0.5f;
        patchCtrls_->looperResampling = 0; //getInitialisingPatchProcessor()->patch->isButtonPressed(PREPOST_SWITCH);
        patchCtrls_->oscUseWavetable = 0; //getInitialisingPatchProcessor()->patch->isButtonPressed(SSWT_SWITCH);
        lastOctave_ = 3;
        octave_ = 1.f / 8.f * lastOctave_;
        unison_ = 0.5f; 
        patchCtrls_->filterMode = 0.f;
        patchCtrls_->filterPosition = 0.f;
        patchCtrls_->modType = 0.f;
        patchCtrls_->resonatorDissonance = 0.f;
        patchCtrls_->echoFilter = 0.5f; 
        patchCtrls_->ambienceAutoPan = 0.f;

        // Modulation
        patchCtrls_->looperLengthModAmount = 0.f;
        patchCtrls_->looperSpeedModAmount = 0.f;
        patchCtrls_->looperStartModAmount = 0.f;
        patchCtrls_->oscDetuneModAmount = 0.f;
        patchCtrls_->oscPitchModAmount = 0.f;
        patchCtrls_->filterCutoffModAmount = 0.5f;
        patchCtrls_->filterResonanceModAmount = 0.f;
        patchCtrls_->resonatorTuneModAmount = 0.f;
        patchCtrls_->resonatorFeedbackModAmount = 0.f;
        patchCtrls_->echoDensityModAmount = 0.f;
        patchCtrls_->echoRepeatsModAmount = 0.f;
        patchCtrls_->ambienceDecayModAmount = 0.f;
        patchCtrls_->ambienceSpacetimeModAmount = 0.f;

        // CVs
        patchCtrls_->looperSpeedCvAmount = 1.f;
        patchCtrls_->looperStartCvAmount = 1.f;
        patchCtrls_->looperLengthCvAmount = 1.f;
        patchCtrls_->oscPitchCvAmount = 1.f;
        patchCtrls_->oscDetuneCvAmount = 1.f;
        patchCtrls_->filterCutoffCvAmount = 1.f;
        patchCtrls_->resonatorTuneCvAmount = 1.f;
        patchCtrls_->echoDensityCvAmount = 1.f;
        patchCtrls_->ambienceSpacetimeCvAmount = 1.f;

        LoadConfig();

        faders_[PARAM_FADER_IN_VOL] = FaderController::create(patchState_, &inputVol_);
        faders_[PARAM_FADER_LOOPER_VOL] = FaderController::create(patchState_, &looperVol_);
        faders_[PARAM_FADER_OSC1_VOL] = FaderController::create(patchState_, &osc1Vol_);
        faders_[PARAM_FADER_OSC2_VOL] = FaderController::create(patchState_, &osc2Vol_);
        faders_[PARAM_FADER_FILTER_VOL] = FaderController::create(patchState_, &patchCtrls_->filterVol);
        faders_[PARAM_FADER_RESONATOR_VOL] = FaderController::create(patchState_, &patchCtrls_->resonatorVol);
        faders_[PARAM_FADER_ECHO_VOL] = FaderController::create(patchState_, &patchCtrls_->echoVol);
        faders_[PARAM_FADER_AMBIENCE_VOL] = FaderController::create(patchState_, &patchCtrls_->ambienceVol);

        knobs_[PARAM_KNOB_LOOPER_SPEED] = KnobController::create(
            patchState_,
            &patchCtrls_->looperSpeed,
            NULL,
            &patchCtrls_->looperSpeedModAmount,
            &patchCtrls_->looperSpeedCvAmount
        );
        knobs_[PARAM_KNOB_LOOPER_START] = KnobController::create(
            patchState_,
            &patchCtrls_->looperStart,
            &patchCtrls_->looperSos,
            &patchCtrls_->looperStartModAmount,
            &patchCtrls_->looperStartCvAmount,
            0.005f
        );
        knobs_[PARAM_KNOB_LOOPER_LENGTH] = KnobController::create(
            patchState_,
            &patchCtrls_->looperLength,
            &patchCtrls_->looperFilter,
            &patchCtrls_->looperLengthModAmount,
            &patchCtrls_->looperLengthCvAmount,
            0.005f
        );

        knobs_[PARAM_KNOB_OSC_PITCH] = KnobController::create(
            patchState_,
            &tune_,
            &octave_,
            &patchCtrls_->oscPitchModAmount,
            &patchCtrls_->oscPitchCvAmount,
            0.01f,
            0.1f
        );
        knobs_[PARAM_KNOB_OSC_DETUNE] = KnobController::create(
            patchState_,
            &patchCtrls_->oscDetune,
            &unison_,
            &patchCtrls_->oscDetuneModAmount,
            &patchCtrls_->oscDetuneCvAmount,
            0.1f
        );

        knobs_[PARAM_KNOB_FILTER_CUTOFF] = KnobController::create(
            patchState_,
            &patchCtrls_->filterCutoff,
            &patchCtrls_->filterMode,
            &patchCtrls_->filterCutoffModAmount,
            &patchCtrls_->filterCutoffCvAmount
        );
        knobs_[PARAM_KNOB_FILTER_RESONANCE] = KnobController::create(
            patchState_,
            &patchCtrls_->filterResonance,
            &patchCtrls_->filterPosition,
            &patchCtrls_->filterResonanceModAmount,
            &patchCtrls_->filterResonanceCvAmount
        );

        knobs_[PARAM_KNOB_RESONATOR_TUNE] = KnobController::create(
            patchState_,
            &patchCtrls_->resonatorTune,
            &patchCtrls_->resonatorDissonance,
            &patchCtrls_->resonatorTuneModAmount,
            &patchCtrls_->resonatorTuneCvAmount,
            0.005f
        );
        knobs_[PARAM_KNOB_RESONATOR_FEEDBACK] = KnobController::create(
            patchState_,
            &patchCtrls_->resonatorFeedback,
            NULL,
            &patchCtrls_->resonatorFeedbackModAmount,
            &patchCtrls_->resonatorFeedbackCvAmount
        );

        knobs_[PARAM_KNOB_ECHO_DENSITY] = KnobController::create(
            patchState_,
            &patchCtrls_->echoDensity,
            &patchCtrls_->echoFilter,
            &patchCtrls_->echoDensityModAmount,
            &patchCtrls_->echoDensityCvAmount,
            0.005f
        );
        knobs_[PARAM_KNOB_ECHO_REPEATS] = KnobController::create(
            patchState_,
            &patchCtrls_->echoRepeats,
            NULL,
            &patchCtrls_->echoRepeatsModAmount,
            &patchCtrls_->echoRepeatsCvAmount
        );

        knobs_[PARAM_KNOB_AMBIENCE_SPACETIME] = KnobController::create(
            patchState_,
            &patchCtrls_->ambienceSpacetime,
            &patchCtrls_->ambienceAutoPan,
            &patchCtrls_->ambienceSpacetimeModAmount,
            &patchCtrls_->ambienceSpacetimeCvAmount,
            0.005f
        );
        knobs_[PARAM_KNOB_AMBIENCE_DECAY] = KnobController::create(
            patchState_,
            &patchCtrls_->ambienceDecay,
            NULL,
            &patchCtrls_->ambienceDecayModAmount,
            &patchCtrls_->ambienceDecayCvAmount
        );

        knobs_[PARAM_KNOB_MOD_LEVEL] = KnobController::create(patchState_, &patchCtrls_->modLevel);
        knobs_[PARAM_KNOB_MOD_SPEED] = KnobController::create(patchState_, &patchCtrls_->modSpeed, &patchCtrls_->modType);

        switches_[PARAM_SWITCH_OSC_USE_SSWT] = SwitchController::create(&patchCtrls_->oscUseWavetable);
        switches_[PARAM_SWITCH_RANDOM_AMOUNT] = SwitchController::create(&patchCtrls_->randomAmount);
        switches_[PARAM_SWITCH_RANDOM_MODE] = SwitchController::create(&patchCtrls_->randomMode);

        cvs_[PARAM_CV_LOOPER_SPEED] = CvController::create(&patchCvs_->looperSpeed, kCvLpCoeff, kCvOffset, kCvMult, 0.005f);
        cvs_[PARAM_CV_LOOPER_START] = CvController::create(&patchCvs_->looperStart, 0.5f);
        cvs_[PARAM_CV_LOOPER_LENGTH] = CvController::create(&patchCvs_->looperLength, 0.5f);
        cvs_[PARAM_CV_OSC_PITCH] = CvController::create(&patchCvs_->oscPitch, 0.995f, 0.f, 1.f, 0.f);
        cvs_[PARAM_CV_OSC_DETUNE] = CvController::create(&patchCvs_->oscDetune, 0.5f, kCvOffset, kCvMult, 0.f);
        cvs_[PARAM_CV_FILTER_CUTOFF] = CvController::create(&patchCvs_->filterCutoff, kCvLpCoeff, kCvOffset, kCvMult, 0.f);
        cvs_[PARAM_CV_FILTER_RESONANCE] = CvController::create(&patchCvs_->filterResonance);
        cvs_[PARAM_CV_RESONATOR_TUNE] = CvController::create(&patchCvs_->resonatorTune, kCvLpCoeff, kCvOffset, kCvMult, 0.f);
        cvs_[PARAM_CV_RESONATOR_FEEDBACK] = CvController::create(&patchCvs_->resonatorFeedback);
        cvs_[PARAM_CV_ECHO_DENSITY] = CvController::create(&patchCvs_->echoDensity, 0.995f);
        cvs_[PARAM_CV_ECHO_REPEATS] = CvController::create(&patchCvs_->echoRepeats);
        cvs_[PARAM_CV_AMBIENCE_SPACETIME] = CvController::create(&patchCvs_->ambienceSpacetime, 0.995f);
        cvs_[PARAM_CV_AMBIENCE_DECAY] = CvController::create(&patchCvs_->ambienceDecay);

        leds_[LED_INPUT] = Led::create(INPUT_LED_PARAM, patchState_->sampleRate, LedType::LED_TYPE_PARAM);
        leds_[LED_INPUT_PEAK] = Led::create(INPUT_PEAK_LED_PARAM, patchState_->sampleRate);
        leds_[LED_SYNC] = Led::create(SYNC_IN, patchState_->sampleRate);
        leds_[LED_MOD] = Led::create(MOD_LED_PARAM, patchState_->sampleRate, LedType::LED_TYPE_PARAM);
        leds_[LED_RECORD] = Led::create(RECORD_BUTTON, patchState_->sampleRate);
        leds_[LED_RANDOM] = Led::create(RANDOM_BUTTON, patchState_->sampleRate);
        leds_[LED_SHIFT] = Led::create(SHIFT_BUTTON, patchState_->sampleRate);
        leds_[LED_MOD_AMOUNT] = Led::create(MOD_CV_RED_LED_PARAM, patchState_->sampleRate);
        leds_[LED_CV_AMOUNT] = Led::create(MOD_CV_GREEN_LED_PARAM, patchState_->sampleRate);

        leds_[LED_ARROW_LEFT] = Led::create(LEFT_ARROW_PARAM, patchState_->sampleRate);
        leds_[LED_ARROW_RIGHT] = Led::create(RIGHT_ARROW_PARAM, patchState_->sampleRate);

        midiOuts_[PARAM_MIDI_LOOPER_LENGTH] = MidiController::create(&patchCtrls_->looperLength, ParamMidi::PARAM_MIDI_LOOPER_LENGTH);
        midiOuts_[PARAM_MIDI_LOOPER_SPEED] = MidiController::create(&patchCtrls_->looperSpeed, ParamMidi::PARAM_MIDI_LOOPER_SPEED);
        midiOuts_[PARAM_MIDI_LOOPER_START] = MidiController::create(&patchCtrls_->looperStart, ParamMidi::PARAM_MIDI_LOOPER_START);
        midiOuts_[PARAM_MIDI_LOOPER_SOS] = MidiController::create(&patchCtrls_->looperSos, ParamMidi::PARAM_MIDI_LOOPER_SOS);
        midiOuts_[PARAM_MIDI_LOOPER_FILTER] = MidiController::create(&patchCtrls_->looperFilter, ParamMidi::PARAM_MIDI_LOOPER_FILTER);
        midiOuts_[PARAM_MIDI_LOOPER_RECORDING] = MidiController::create(&patchCtrls_->looperRecording, ParamMidi::PARAM_MIDI_LOOPER_RECORDING);
        midiOuts_[PARAM_MIDI_LOOPER_RESAMPLING] = MidiController::create(&patchCtrls_->looperResampling, ParamMidi::PARAM_MIDI_LOOPER_RESAMPLING);
        midiOuts_[PARAM_MIDI_LOOPER_VOL] = MidiController::create(&patchCtrls_->looperVol, ParamMidi::PARAM_MIDI_LOOPER_VOL);
        midiOuts_[PARAM_MIDI_OSC_PITCH] = MidiController::create(&tune_, ParamMidi::PARAM_MIDI_OSC_PITCH);
        midiOuts_[PARAM_MIDI_OSC_DETUNE] = MidiController::create(&patchCtrls_->oscDetune, ParamMidi::PARAM_MIDI_OSC_DETUNE);
        midiOuts_[PARAM_MIDI_OSC_UNISON] = MidiController::create(&unison_, ParamMidi::PARAM_MIDI_OSC_UNISON);
        midiOuts_[PARAM_MIDI_OSC1_VOL] = MidiController::create(&patchCtrls_->osc1Vol, ParamMidi::PARAM_MIDI_OSC1_VOL);
        midiOuts_[PARAM_MIDI_OSC2_VOL] = MidiController::create(&patchCtrls_->osc2Vol, ParamMidi::PARAM_MIDI_OSC2_VOL);
        midiOuts_[PARAM_MIDI_FILTER_CUTOFF] = MidiController::create(&patchCtrls_->filterCutoff, ParamMidi::PARAM_MIDI_FILTER_CUTOFF);
        midiOuts_[PARAM_MIDI_FILTER_RESONANCE] = MidiController::create(&patchCtrls_->filterResonance, ParamMidi::PARAM_MIDI_FILTER_RESONANCE);
        midiOuts_[PARAM_MIDI_FILTER_MODE] = MidiController::create(&patchCtrls_->filterMode, ParamMidi::PARAM_MIDI_FILTER_MODE);
        midiOuts_[PARAM_MIDI_FILTER_POSITION] = MidiController::create(&patchCtrls_->filterPosition, ParamMidi::PARAM_MIDI_FILTER_POSITION);
        midiOuts_[PARAM_MIDI_FILTER_VOL] = MidiController::create(&patchCtrls_->filterVol, ParamMidi::PARAM_MIDI_FILTER_VOL);
        midiOuts_[PARAM_MIDI_RESONATOR_TUNE] = MidiController::create(&patchCtrls_->resonatorTune, ParamMidi::PARAM_MIDI_RESONATOR_TUNE);
        midiOuts_[PARAM_MIDI_RESONATOR_FEEDBACK] = MidiController::create(&patchCtrls_->resonatorFeedback, ParamMidi::PARAM_MIDI_RESONATOR_FEEDBACK);
        midiOuts_[PARAM_MIDI_RESONATOR_DISSONANCE] = MidiController::create(&patchCtrls_->resonatorDissonance, ParamMidi::PARAM_MIDI_RESONATOR_DISSONANCE);
        midiOuts_[PARAM_MIDI_RESONATOR_VOL] = MidiController::create(&patchCtrls_->resonatorVol, ParamMidi::PARAM_MIDI_RESONATOR_VOL);
        midiOuts_[PARAM_MIDI_ECHO_REPEATS] = MidiController::create(&patchCtrls_->echoRepeats, ParamMidi::PARAM_MIDI_ECHO_REPEATS);
        midiOuts_[PARAM_MIDI_ECHO_DENSITY] = MidiController::create(&patchCtrls_->echoDensity, ParamMidi::PARAM_MIDI_ECHO_DENSITY);
        midiOuts_[PARAM_MIDI_ECHO_FILTER] = MidiController::create(&patchCtrls_->echoFilter, ParamMidi::PARAM_MIDI_ECHO_FILTER);
        midiOuts_[PARAM_MIDI_ECHO_VOL] = MidiController::create(&patchCtrls_->echoVol, ParamMidi::PARAM_MIDI_ECHO_VOL);
        midiOuts_[PARAM_MIDI_AMBIENCE_DECAY] = MidiController::create(&patchCtrls_->ambienceDecay, ParamMidi::PARAM_MIDI_AMBIENCE_DECAY);
        midiOuts_[PARAM_MIDI_AMBIENCE_SPACETIME] = MidiController::create(&patchCtrls_->ambienceSpacetime, ParamMidi::PARAM_MIDI_AMBIENCE_SPACETIME);
        midiOuts_[PARAM_MIDI_AMBIENCE_AUTOPAN] = MidiController::create(&patchCtrls_->ambienceAutoPan, ParamMidi::PARAM_MIDI_AMBIENCE_AUTOPAN);
        midiOuts_[PARAM_MIDI_AMBIENCE_VOL] = MidiController::create(&patchCtrls_->ambienceVol, ParamMidi::PARAM_MIDI_AMBIENCE_VOL);
        midiOuts_[PARAM_MIDI_MOD_LEVEL] = MidiController::create(&patchCtrls_->modLevel, ParamMidi::PARAM_MIDI_MOD_LEVEL);
        midiOuts_[PARAM_MIDI_MOD_SPEED] = MidiController::create(&patchCtrls_->modSpeed, ParamMidi::PARAM_MIDI_MOD_SPEED);
        midiOuts_[PARAM_MIDI_MOD_TYPE] = MidiController::create(&patchCtrls_->modType, ParamMidi::PARAM_MIDI_MOD_TYPE);
        midiOuts_[PARAM_MIDI_INPUT_VOL] = MidiController::create(&patchCtrls_->inputVol, ParamMidi::PARAM_MIDI_INPUT_VOL);
        midiOuts_[PARAM_MIDI_RANDOM_MODE] = MidiController::create(&patchCtrls_->randomMode, ParamMidi::PARAM_MIDI_RANDOM_MODE);
        midiOuts_[PARAM_MIDI_RANDOM_AMOUNT] = MidiController::create(&patchCtrls_->randomAmount, ParamMidi::PARAM_MIDI_RANDOM_AMOUNT);
        midiOuts_[PARAM_MIDI_OSC_USE_SSWT] = MidiController::create(&patchCtrls_->oscUseWavetable, ParamMidi::PARAM_MIDI_OSC_USE_SSWT);
        midiOuts_[PARAM_MIDI_RANDOMIZE] = MidiController::create(&randomize_, ParamMidi::PARAM_MIDI_RANDOMIZE);

        midiOuts_[PARAM_MIDI_LOOPER_SPEED_CV] = MidiController::create(&patchCvs_->looperSpeed, ParamMidi::PARAM_MIDI_LOOPER_SPEED_CV, 0, 0.5f, 0.6666667f);
        midiOuts_[PARAM_MIDI_LOOPER_START_CV] = MidiController::create(&patchCvs_->looperStart, ParamMidi::PARAM_MIDI_LOOPER_START_CV, 0, 0.5f, 0.6666667f);
        midiOuts_[PARAM_MIDI_LOOPER_LENGTH_CV] = MidiController::create(&patchCvs_->looperLength, ParamMidi::PARAM_MIDI_LOOPER_LENGTH_CV, 0, 0.5f, 0.6666667f);
        midiOuts_[PARAM_MIDI_OSC_PITCH_CV] = MidiController::create(&patchCvs_->oscPitch, ParamMidi::PARAM_MIDI_OSC_PITCH_CV);
        midiOuts_[PARAM_MIDI_OSC_DETUNE_CV] = MidiController::create(&patchCvs_->oscDetune, ParamMidi::PARAM_MIDI_OSC_DETUNE_CV, 0, 0.5f, 0.6666667f);
        midiOuts_[PARAM_MIDI_FILTER_CUTOFF_CV] = MidiController::create(&patchCvs_->filterCutoff, ParamMidi::PARAM_MIDI_FILTER_CUTOFF_CV, 0, 0.5f, 0.6666667f);
        midiOuts_[PARAM_MIDI_RESONATOR_TUNE_CV] = MidiController::create(&patchCvs_->resonatorTune, ParamMidi::PARAM_MIDI_RESONATOR_TUNE_CV, 0, 0.5f, 0.6666667f);
        midiOuts_[PARAM_MIDI_ECHO_DENSITY_CV] = MidiController::create(&patchCvs_->echoDensity, ParamMidi::PARAM_MIDI_ECHO_DENSITY_CV, 0, 0.5f, 0.6666667f);
        midiOuts_[PARAM_MIDI_AMBIENCE_SPACETIME_CV] = MidiController::create(&patchCvs_->ambienceSpacetime, ParamMidi::PARAM_MIDI_AMBIENCE_SPACETIME_CV, 0, 0.5f, 0.6666667f);

        recordButton_ = RecordButtonController::create(leds_[LED_RECORD], patchState_->sampleRate, patchState_->blockRate);
        randomButton_ = RandomButtonController::create(leds_[LED_RANDOM], patchState_->sampleRate, patchState_->blockRate);
        shiftButton_ = ShiftButtonController::create(leds_[LED_SHIFT], patchState_->sampleRate, patchState_->blockRate);
        modCvButton_ = ModCvButtonController::create(leds_[LED_MOD_AMOUNT], leds_[LED_CV_AMOUNT], patchState_->sampleRate, patchState_->blockRate);
    }
    ~Ui()
    {
        FloatArray::destroy(patchState_->inputLevel);
        FloatArray::destroy(patchState_->efModLevel);
        TapTempo::destroy(patchState_->tempo);
        for (size_t i = 0; i < PARAM_KNOB_LAST; i++)
        {
            KnobController::destroy(knobs_[i]);
        }
        for (size_t i = 0; i < PARAM_FADER_LAST; i++)
        {
            FaderController::destroy(faders_[i]);
        }
        for (size_t i = 0; i < PARAM_CV_LAST; i++)
        {
            CvController::destroy(cvs_[i]);
        }
        for (size_t i = 0; i < PARAM_SWITCH_LAST; i++)
        {
            SwitchController::destroy(switches_[i]);
        }
        for (size_t i = 0; i < LED_LAST; i++)
        {
            Led::destroy(leds_[i]);
        }
        for (size_t i = 0; i < PARAM_MIDI_LAST; i++)
        {
            MidiController::destroy(midiOuts_[i]);
        }
        RecordButtonController::destroy(recordButton_);
        RandomButtonController::destroy(randomButton_);
        ShiftButtonController::destroy(shiftButton_);
        ModCvButtonController::destroy(modCvButton_);
    }

    static Ui* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Ui(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Ui* obj)
    {
        delete obj;
    }

    void LoadConfig()
    {
        Resource* resource = nullptr; //Resource::load(PATCH_SETTINGS_NAME ".cfg");
        if (resource)
        {
            Configuration* configuration = (Configuration*)resource->getData();
            vOctScale1_ = (float)configuration->voct1_scale / UINT16_MAX;
            vOctOffset1_ = (float)configuration->voct1_offset / UINT16_MAX;
            vOctScale2_ = (float)configuration->voct2_scale / UINT16_MAX;
            vOctOffset2_ = (float)configuration->voct2_offset / UINT16_MAX;

            PatchParameterId voctParam = paramCvMap[PARAM_CV_OSC_PITCH];
            patchState_->c5 = (float)configuration->c5 / (configuration->params_max[voctParam] - configuration->params_min[voctParam]);
            PatchParameterId pitchParam = paramKnobMap[PARAM_KNOB_OSC_PITCH];
            patchState_->pitchZero = (float)configuration->pitch_zero / (configuration->params_max[pitchParam] - configuration->params_min[pitchParam]);
            PatchParameterId speedParam = paramKnobMap[PARAM_KNOB_LOOPER_SPEED];
            patchState_->speedZero = (float)configuration->speed_zero / (configuration->params_max[speedParam] - configuration->params_min[speedParam]);
            patchState_->softTakeover = configuration->soft_takeover;
            patchState_->modAttenuverters = configuration->mod_attenuverters;
            patchState_->cvAttenuverters = configuration->cv_attenuverters;

            Resource::destroy(resource);
        }
        else {
            patchState_->c5 = 0.5f; // Where in the range 0..1 is C5 (V/OCT CV)
            vOctScale1_ = 120.f; // Used for pitches below C5 (V/OCT CV)
            vOctOffset1_ = 0.f; // Used for pitches below C5 (V/OCT CV)
            vOctScale2_ = 120.f; // Used for pitches above C5 (V/OCT CV)
            vOctOffset2_ = 0.f; // Used for pitches above C5 (V/OCT CV)
            patchState_->pitchZero = 0.5f; // Where in the range 0..1 of the PITCH pot is the center
            patchState_->speedZero = 0.5f; // Where in the range 0..1 of the VARISPEED pot is the center 
        }

    }

    void LoadAltParams()
    {
        Resource* resource = Resource::load(PATCH_SETTINGS_NAME ".alt");
        if (resource != NULL)
        {
            int16_t* cfg = (int16_t*)resource->getData();
            knobs_[PARAM_KNOB_AMBIENCE_SPACETIME]->SetValue(cfg[0] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Ambience auto-pan
            knobs_[PARAM_KNOB_ECHO_DENSITY]->SetValue(cfg[1] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Echo filter
            knobs_[PARAM_KNOB_FILTER_RESONANCE]->SetValue(cfg[2] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Filter position
            knobs_[PARAM_KNOB_FILTER_CUTOFF]->SetValue(cfg[3] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Filter mode
            knobs_[PARAM_KNOB_LOOPER_LENGTH]->SetValue(cfg[4] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Looper filter
            knobs_[PARAM_KNOB_LOOPER_START]->SetValue(cfg[5] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Looper SOS
            knobs_[PARAM_KNOB_MOD_SPEED]->SetValue(cfg[6] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Mod type
            knobs_[PARAM_KNOB_OSC_PITCH]->SetValue(cfg[7] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Octave
            knobs_[PARAM_KNOB_OSC_DETUNE]->SetValue(cfg[8] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Unison
            knobs_[PARAM_KNOB_RESONATOR_TUNE]->SetValue(cfg[9] / 8192.f, LockableParamName::PARAM_LOCKABLE_ALT); // Reso dissonance
        }
        Resource::destroy(resource);
    }

    void LoadModParams()
    {
        Resource* resource = Resource::load(PATCH_SETTINGS_NAME ".mod");
        if (resource)
        {
            int16_t* cfg = (int16_t*)resource->getData();
            knobs_[PARAM_KNOB_AMBIENCE_DECAY]->SetValue(cfg[0] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Ambience decay mod amount
            knobs_[PARAM_KNOB_AMBIENCE_SPACETIME]->SetValue(cfg[1] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Ambience spacetime mod amount
            knobs_[PARAM_KNOB_ECHO_DENSITY]->SetValue(cfg[2] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Echo density mod amount
            knobs_[PARAM_KNOB_ECHO_REPEATS]->SetValue(cfg[3] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Echo repeats mod amount
            knobs_[PARAM_KNOB_FILTER_CUTOFF]->SetValue(cfg[4] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Filter cutoff mod amount
            knobs_[PARAM_KNOB_FILTER_RESONANCE]->SetValue(cfg[5] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Filter resonance mod amount
            knobs_[PARAM_KNOB_LOOPER_LENGTH]->SetValue(cfg[6] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Looper length mod amount
            knobs_[PARAM_KNOB_LOOPER_SPEED]->SetValue(cfg[7] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Looper speed mod amount
            knobs_[PARAM_KNOB_LOOPER_START]->SetValue(cfg[8] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Looper start mod amount
            knobs_[PARAM_KNOB_OSC_DETUNE]->SetValue(cfg[9] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Osc detune mod amount
            knobs_[PARAM_KNOB_OSC_PITCH]->SetValue(cfg[10] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Osc pitch mod amount
            knobs_[PARAM_KNOB_RESONATOR_FEEDBACK]->SetValue(cfg[11] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Resonator feedback mod amount
            knobs_[PARAM_KNOB_RESONATOR_TUNE]->SetValue(cfg[12] / 8192.f, LockableParamName::PARAM_LOCKABLE_MOD); // Resonator tune mod amount
        }
        Resource::destroy(resource);
    }

    void LoadCvParams()
    {
        Resource* resource = Resource::load(PATCH_SETTINGS_NAME ".cv");
        if (resource)
        {
            int16_t* cfg = (int16_t*)resource->getData();
            knobs_[PARAM_KNOB_AMBIENCE_DECAY]->SetValue(cfg[0] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Ambience decay cv amount
            knobs_[PARAM_KNOB_AMBIENCE_SPACETIME]->SetValue(cfg[1] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Ambience spacetime cv amount
            knobs_[PARAM_KNOB_ECHO_DENSITY]->SetValue(cfg[2] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Echo density cv amount
            knobs_[PARAM_KNOB_ECHO_REPEATS]->SetValue(cfg[3] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Echo repeats cv amount
            knobs_[PARAM_KNOB_FILTER_CUTOFF]->SetValue(cfg[4] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Filter cutoff cv amount
            knobs_[PARAM_KNOB_FILTER_RESONANCE]->SetValue(cfg[5] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Filter resonance cv amount
            knobs_[PARAM_KNOB_LOOPER_LENGTH]->SetValue(cfg[6] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Looper length cv amount
            knobs_[PARAM_KNOB_LOOPER_SPEED]->SetValue(cfg[7] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Looper speed cv amount
            knobs_[PARAM_KNOB_LOOPER_START]->SetValue(cfg[8] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Looper start cv amount
            knobs_[PARAM_KNOB_OSC_DETUNE]->SetValue(cfg[9] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Osc detune cv amount
            knobs_[PARAM_KNOB_OSC_PITCH]->SetValue(cfg[10] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Osc pitch cv amount
            knobs_[PARAM_KNOB_RESONATOR_FEEDBACK]->SetValue(cfg[11] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Resonator feedback cv amount
            knobs_[PARAM_KNOB_RESONATOR_TUNE]->SetValue(cfg[12] / 8192.f, LockableParamName::PARAM_LOCKABLE_CV); // Resonator tune cv amount
        }
        Resource::destroy(resource);
    }

    void LoadMainParams()
    {
        Resource* resource = Resource::load(PATCH_SETTINGS_NAME ".prm");
        if (resource)
        {
            int16_t* cfg = (int16_t*)resource->getData();
            knobs_[PARAM_KNOB_AMBIENCE_DECAY]->SetValue(cfg[0] / 8192.f); // Ambience decay
            knobs_[PARAM_KNOB_AMBIENCE_SPACETIME]->SetValue(cfg[1] / 8192.f); // Ambience spacetime
            knobs_[PARAM_KNOB_ECHO_DENSITY]->SetValue(cfg[2] / 8192.f); // Echo density
            knobs_[PARAM_KNOB_ECHO_REPEATS]->SetValue(cfg[3] / 8192.f); // Echo repeats
            knobs_[PARAM_KNOB_FILTER_CUTOFF]->SetValue(cfg[4] / 8192.f); // Filter cutoff
            knobs_[PARAM_KNOB_FILTER_RESONANCE]->SetValue(cfg[5] / 8192.f); // Filter resonance
            knobs_[PARAM_KNOB_LOOPER_LENGTH]->SetValue(cfg[6] / 8192.f); // Looper length
            knobs_[PARAM_KNOB_LOOPER_SPEED]->SetValue(cfg[7] / 8192.f); // Looper speed
            knobs_[PARAM_KNOB_LOOPER_START]->SetValue(cfg[8] / 8192.f); // Looper start
            knobs_[PARAM_KNOB_OSC_DETUNE]->SetValue(cfg[9] / 8192.f); // Osc detune
            knobs_[PARAM_KNOB_OSC_PITCH]->SetValue(cfg[10] / 8192.f); // Osc pitch
            knobs_[PARAM_KNOB_RESONATOR_FEEDBACK]->SetValue(cfg[11] / 8192.f); // Resonator feedback
            knobs_[PARAM_KNOB_RESONATOR_TUNE]->SetValue(cfg[12] / 8192.f); // Resonator tune
            knobs_[PARAM_KNOB_MOD_LEVEL]->SetValue(cfg[13] / 8192.f); // Mod level
            knobs_[PARAM_KNOB_MOD_SPEED]->SetValue(cfg[14] / 8192.f); // Mod speed

            /*
            faders_[PARAM_FADER_ECHO_VOL]->SetValue(cfg[15] / 8192.f); // Echo vol
            faders_[PARAM_FADER_OSC1_VOL]->SetValue(cfg[16] / 8192.f); // Osc1 vol
            faders_[PARAM_FADER_OSC2_VOL]->SetValue(cfg[17] / 8192.f); // Osc2 vol
            faders_[PARAM_FADER_IN_VOL]->SetValue(cfg[18] / 8192.f); // Input vol
            faders_[PARAM_FADER_FILTER_VOL]->SetValue(cfg[19] / 8192.f); // Filter vol
            faders_[PARAM_FADER_LOOPER_VOL]->SetValue(cfg[20] / 8192.f); // Looper vol
            faders_[PARAM_FADER_AMBIENCE_VOL]->SetValue(cfg[21] / 8192.f); // Ambience vol
            faders_[PARAM_FADER_RESONATOR_VOL]->SetValue(cfg[22] / 8192.f); // Resonator vol
            */
        }
        Resource::destroy(resource);
    }

    void SaveParametersConfig(FuncMode funcMode)
    {
        // not needed in VCV
        return;
        /*
        if (!parameterChangedSinceLastSave_)
        {
            return;
        }
        */

        //saving_ = true;
        //parameterChangedSinceLastSave_ = false;

        float values[MAX_PATCH_SETTINGS]{};

        switch (funcMode)
        {
        case FUNC_MODE_ALT:
            values[0] = patchCtrls_->ambienceAutoPan;
            values[1] = patchCtrls_->echoFilter;
            values[2] = patchCtrls_->filterPosition;
            values[3] = patchCtrls_->filterMode;
            values[4] = patchCtrls_->looperFilter;
            values[5] = patchCtrls_->looperSos;
            values[6] = patchCtrls_->modType;
            values[7] = octave_;
            values[8] = unison_;
            values[9] = patchCtrls_->resonatorDissonance;
            break;
        case FUNC_MODE_MOD:
            values[0] = patchCtrls_->ambienceDecayModAmount;
            values[1] = patchCtrls_->ambienceSpacetimeModAmount;
            values[2] = patchCtrls_->echoDensityModAmount;
            values[3] = patchCtrls_->echoRepeatsModAmount;
            values[4] = patchCtrls_->filterCutoffModAmount;
            values[5] = patchCtrls_->filterResonanceModAmount;
            values[6] = patchCtrls_->looperLengthModAmount;
            values[7] = patchCtrls_->looperSpeedModAmount;
            values[8] = patchCtrls_->looperStartModAmount;
            values[9] = patchCtrls_->oscDetuneModAmount;
            values[10] = patchCtrls_->oscPitchModAmount;
            values[11] = patchCtrls_->resonatorFeedbackModAmount;
            values[12] = patchCtrls_->resonatorTuneModAmount;
            break;
        case FUNC_MODE_CV:
            values[0] = patchCtrls_->ambienceDecayCvAmount;
            values[1] = patchCtrls_->ambienceSpacetimeCvAmount;
            values[2] = patchCtrls_->echoDensityCvAmount;
            values[3] = patchCtrls_->echoRepeatsCvAmount;
            values[4] = patchCtrls_->filterCutoffCvAmount;
            values[5] = patchCtrls_->filterResonanceCvAmount;
            values[6] = patchCtrls_->looperLengthCvAmount;
            values[7] = patchCtrls_->looperSpeedCvAmount;
            values[8] = patchCtrls_->looperStartCvAmount;
            values[9] = patchCtrls_->oscDetuneCvAmount;
            values[10] = patchCtrls_->oscPitchCvAmount;
            values[11] = patchCtrls_->resonatorFeedbackCvAmount;
            values[12] = patchCtrls_->resonatorTuneCvAmount;
            break;

        default:
            // NONE
            values[0] = patchCtrls_->ambienceDecay;
            values[1] = patchCtrls_->ambienceSpacetime;
            values[2] = patchCtrls_->echoDensity;
            values[3] = patchCtrls_->echoRepeats;
            values[4] = patchCtrls_->filterCutoff;
            values[5] = patchCtrls_->filterResonance;
            values[6] = patchCtrls_->looperLength;
            values[7] = patchCtrls_->looperSpeed;
            values[8] = patchCtrls_->looperStart;
            values[9] = patchCtrls_->oscDetune;
            values[10] = tune_;
            values[11] = patchCtrls_->resonatorFeedback;
            values[12] = patchCtrls_->resonatorTune;
            values[13] = patchCtrls_->modLevel;
            values[14] = patchCtrls_->modSpeed;
            /*
            values[15] = patchCtrls_->echoVol;
            values[16] = osc1Vol_;
            values[17] = osc2Vol_;
            values[18] = inputVol_;
            values[19] = patchCtrls_->filterVol;
            values[20] = looperVol_;
            values[21] = patchCtrls_->ambienceVol;
            values[22] = patchCtrls_->resonatorVol;
            */
            break;
        }

        // Start the save process.
        // getInitialisingPatchProcessor()->patch->sendMidi(MidiMessage(USB_COMMAND_SINGLE_BYTE, START, 0, 0)); // send MIDI START

        // Send the file index - 0: "oneiroi.prm", 1: "oneiroi.alt", 2: "oneiroi.mod", 3: "oneiroi.cv"
        // getInitialisingPatchProcessor()->patch->sendMidi(MidiMessage::cp(0, funcMode));

        for (size_t i = 0; i < MAX_PATCH_SETTINGS; i++)
        {
            // Convert to 14-bit signed int.
            // int16_t value = rintf(values[i] * 8192);
            // Send the parameter's value.
            // getInitialisingPatchProcessor()->patch->sendMidi(MidiMessage::pb(i, value));
        }

        // Finish the process.
        // getInitialisingPatchProcessor()->patch->sendMidi(MidiMessage(USB_COMMAND_SINGLE_BYTE, STOP, 0, 0)); // send MIDI STOP
    }

    // Callback
    void ProcessButton(PatchButtonId bid, uint16_t value, uint16_t samples)
    {
        bool on = value != 0;

        switch (bid)
        {
        case SYNC_IN:
            patchState_->tempo->trigger(on, samples);
            patchState_->syncIn = on;
            break;

        case RECORD_IN:
#ifdef USE_RECORD_THRESHOLD
            recordPressed_ = on;
            if (on)
            {
                // Start recording.
                recordingState_ = RecordingState::RECORDING_STATE_START;
                samplesSinceRecordingStarted_ = 0;
                samplesSinceRecordInReceived_ = 0;
            }
            else if (RecordingState::RECORDING_STATE_RECORDING == recordingState_)
            {
                // If the input goes down when the looper was recording, stop
                // it (gate mode).
                recordButton_->Set(0);
                recordingState_ = RecordingState::RECORDING_STATE_STOP;
            }
#else
            recordButton_->Set(on);
#endif // USE_RECORD_THRESHOLD
            break;
        case RECORD_BUTTON:
            recordButton_->Trig(on);
            break;

        case RANDOM_IN:
            if (on)
            {
                leds_[LED_RANDOM]->Blink();
                if (!randomize_)
                {
                    patchState_->randomSlew = 1.f / kRandomSlewSamples;
                    patchState_->randomHasSlew = false;
                    randomize_ = true;
                }
            }
            break;
        case RANDOM_BUTTON:
            randomButton_->Trig(on);
            break;

        case SSWT_SWITCH:
            switches_[PARAM_SWITCH_OSC_USE_SSWT]->Set(on);
            break;

        case PREPOST_SWITCH:
            patchCtrls_->looperResampling = on;
            break;

        case SHIFT_BUTTON:
            shiftButton_->Trig(on);
            break;

        case MOD_CV_BUTTON:
            modCvButton_->Trig(on);
            break;

        default:
            break;
        }
    }

    // Callback.
    void ProcessMidi(MidiMessage msg)
    {
        return;

        if (msg.isControlChange())
        {
            if (msg.getControllerNumber() < PARAM_MIDI_LAST)
            {
                //midiOuts_[msg.getControllerNumber()]->SetValue(msg.getControllerValue() / 127.0f);
            }
        }
        /*
        else if(msg.isNoteOn())
        {
            midinote = msg.getNote();
        }
        */
    }

    void HandleLeds()
    {
        float level = patchState_->inputLevel.getMean();
        if (level < 0.7f)
        {
            leds_[LED_INPUT]->Set(level);
            leds_[LED_INPUT_PEAK]->Off();
        }
        else
        {
            leds_[LED_INPUT]->Off();
            leds_[LED_INPUT_PEAK]->On();
        }

        float v = Map(patchState_->modValue, -0.5f, 0.5f, 0.f, 1.0f);
        // if (v < 0.5f) v = 0;

        leds_[LED_MOD]->Set(v);

        if (patchState_->clockTick)
        {
            leds_[LED_SYNC]->Blink();
        }

        if (oscPitchCenterTrigger_.Process(patchState_->oscPitchCenterFlag) || oscOctaveTrigger_.Process(patchState_->oscOctaveFlag) ||
            oscUnisonCenterTrigger_.Process(patchState_->oscUnisonCenterFlag) || looperSpeedLockTrigger_.Process(patchState_->looperSpeedLockFlag) ||
            modTypeLockTrigger_.Process(patchState_->modTypeLockFlag) || modSpeedLockTrigger_.Process(patchState_->modSpeedLockFlag) ||
            filterModeTrigger_.Process(patchState_->filterModeFlag) || filterPositionTrigger_.Process(patchState_->filterPositionFlag)
        ) {
            leds_[LED_ARROW_LEFT]->Blink(2);
            leds_[LED_ARROW_RIGHT]->Blink(2, false, false);
        }
    }

    void HandleCatchUp()
    {
        bool moving = false;
        bool wasCatchingUp = false;

        for (size_t i = 0; i < PARAM_KNOB_LAST + PARAM_FADER_LAST; i++)
        {
            CatchUpController* ctrl = (i < PARAM_KNOB_LAST) ? (CatchUpController*)knobs_[i] : (CatchUpController*)faders_[i - PARAM_KNOB_LAST];
            bool m = ctrl->Process();
            if (m && !moving)
            {
                movingParam_ = ctrl;
                moving = true;
            }
        }

        /*
        if (moving && FuncMode::FUNC_MODE_NONE != patchState_->funcMode)
        {
            parameterChangedSinceLastSave_ = true;
        }
        */

        if (!patchState_->softTakeover)
        {
            return;
        }

        if (movingParam_ && !movingParam_->IsMoving())
        {
            movingParam_ = NULL;
            moving = false;
            wasCatchingUp = true;
        }

        if (moving)
        {
            ParamCatchUp catchUp = movingParam_->GetCatchUpState();

            if (ParamCatchUp::PARAM_CATCH_UP_LEFT == catchUp)
            {
                leds_[LED_ARROW_LEFT]->On();
                leds_[LED_ARROW_RIGHT]->Off();
                wasCatchingUp = true;
            }
            else if (ParamCatchUp::PARAM_CATCH_UP_RIGHT == catchUp)
            {
                leds_[LED_ARROW_LEFT]->Off();
                leds_[LED_ARROW_RIGHT]->On();
                wasCatchingUp = true;
            }
            else if (ParamCatchUp::PARAM_CATCH_UP_DONE == catchUp)
            {
                // Done catching up.
                leds_[LED_ARROW_LEFT]->Blink(2, true);
                leds_[LED_ARROW_RIGHT]->Blink(2, true);
            }
        }
        else if (wasCatchingUp)
        {
            leds_[LED_ARROW_LEFT]->Off();
            leds_[LED_ARROW_RIGHT]->Off();
            wasCatchingUp = false;
        }
    }

    void HandleLedButtons()
    {
        recordButton_->Process();
        randomButton_->Process();
        shiftButton_->Process();
        modCvButton_->Process();

        FuncMode funcMode = patchState_->funcMode;
        if (modCvButton_->IsPressed())
        {
            // Handle long press for saving.
            if (samplesSinceModCvPressed_ < kSaveLimit || true)
            {
                samplesSinceModCvPressed_++;
                leds_[shiftButton_->IsOn() ? LED_CV_AMOUNT : LED_MOD_AMOUNT]->On();
            }
            else if (!saveFlag_)
            {
                // Save.
                saveFlag_ = true;
                saving_ = true;
                fadeOutOutput_ = true;
                leds_[shiftButton_->IsOn() ? LED_CV_AMOUNT : LED_MOD_AMOUNT]->Off();
            }
        }
        else if (shiftButton_->IsOn() && !modCvButton_->IsOn())
        {
            // Only SHIFT button is on.
            if (saveFlag_)
            {
                // Saving button has been released.
                samplesSinceModCvPressed_ = 0;
                saveFlag_ = false;

                // Restore previous button state.
                if (FuncMode::FUNC_MODE_CV == patchState_->funcMode)
                {
                    modCvButton_->SetFuncMode(FuncMode::FUNC_MODE_CV);
                    modCvButton_->Set(true);
                }
            }
            else if (wasCvMap_)
            {
                // If we were editing CV mapping and MOD/CV button turns off,
                // exit from CV mapping.
                modCvButton_->SetFuncMode(FuncMode::FUNC_MODE_NONE);
                shiftButton_->Set(false);
                wasCvMap_ = false;
                samplesSinceModCvPressed_ = 0;
            }
            else
            {
                funcMode = FuncMode::FUNC_MODE_ALT;
                wasCvMap_ = false;
            }
        }
        else if (!shiftButton_->IsOn() && modCvButton_->IsOn())
        {
            // Only MOD/CV button is on.
            if (wasCvMap_)
            {
                // If we were editing CV mapping and SHIFT button turns off,
                // exit from CV mapping.
                modCvButton_->SetFuncMode(FuncMode::FUNC_MODE_NONE);
                modCvButton_->Set(false);
                wasCvMap_ = false;
                samplesSinceModCvPressed_ = 0;
            }
            else
            {
                funcMode = FuncMode::FUNC_MODE_MOD;
            }
        }
        else if (shiftButton_->IsOn() && modCvButton_->IsOn())
        {
            // Both SHIFT and MOD/CV buttons are on.
            funcMode = FuncMode::FUNC_MODE_CV;
            wasCvMap_ = true;
        }
        else if (!shiftButton_->IsOn() && !modCvButton_->IsOn())
        {
            // Neither SHIFT nor MOD/CV buttons are on.
            if (saveFlag_)
            {
                // Saving button has been released.
                saveFlag_ = false;

                // Restore previous button state.
                if (FuncMode::FUNC_MODE_MOD == patchState_->funcMode)
                {
                    modCvButton_->SetFuncMode(FuncMode::FUNC_MODE_MOD);
                    modCvButton_->Set(true);
                }
            }
            else
            {
                funcMode = FuncMode::FUNC_MODE_NONE;
            }
            samplesSinceModCvPressed_ = 0;
        }

        if (funcMode != patchState_->funcMode)
        {
            patchState_->funcMode = funcMode;

            for (size_t i = 0; i < PARAM_KNOB_LAST; i++)
            {
                knobs_[i]->SetFuncMode(patchState_->funcMode);
            }
            recordButton_->SetFuncMode(patchState_->funcMode);
            randomButton_->SetFuncMode(patchState_->funcMode);
            modCvButton_->SetFuncMode(patchState_->funcMode);
        }

        switch (patchState_->funcMode)
        {
        case FuncMode::FUNC_MODE_NONE:
            patchCtrls_->looperRecording = recordButton_->IsOn();

            // Handle long press of the random button for slewing.
            if (randomButton_->IsPressed())
            {
                leds_[LED_RANDOM]->On();
                samplesSinceRandomPressed_++;
            }
            else if (samplesSinceRandomPressed_ > 0 && !randomize_)
            {
                // Random button has been released.
                doRandomSlew_ = samplesSinceRandomPressed_ > kRandomSlewSamples;
                patchState_->randomHasSlew = doRandomSlew_;
                patchState_->randomSlew = 1.f / samplesSinceRandomPressed_;
                randomSlewInc_ = 0;
                samplesSinceRandomPressed_ = 0;
                randomize_ = true;
            }
            break;

        default:
            if (recordButton_->IsPressed() || randomButton_->IsPressed())
            {
                if (samplesSinceRecordOrRandomPressed_ < kResetLimit)
                {
                    samplesSinceRecordOrRandomPressed_++;
                }
                else if (recordAndRandomTrigger_.Process(recordButton_->IsPressed() && randomButton_->IsPressed()))
                {
                    recordAndRandomPressed_ = true;
                    // Reset parameters.
                    for (size_t i = 0; i < PARAM_KNOB_LAST + PARAM_FADER_LAST; i++)
                    {
                        CatchUpController* ctrl = (i < PARAM_KNOB_LAST) ? (CatchUpController*)knobs_[i] : (CatchUpController*)faders_[i - PARAM_KNOB_LAST];
                        ctrl->Reset();
                    }
                }
                // recordAndRandomPressed_ assures that if the last operation was
                // the reset of parameters, the next operation may be performed only
                // if the buttons are released.
                else if (!recordAndRandomPressed_)
                {
                    if (FuncMode::FUNC_MODE_ALT == patchState_->funcMode)
                    {
                        if (clearLooperTrigger_.Process(recordButton_->IsPressed()))
                        {
                            // Clear the looper's buffer.
                            patchState_->clearLooperFlag = true;
                        }
                        if (!undoRedo_ && randomButton_->IsOn())
                        {
                            undoRedo_ = true;
                        }
                    }
                }
            }
            else
            {
                samplesSinceRecordOrRandomPressed_ = 0;
                recordAndRandomPressed_ = false;
                recordAndRandomTrigger_.Process(0);
                clearLooperTrigger_.Process(0);
                undoRedoRandomTrigger_.Process(0);
            }
            break;
        }

#ifdef USE_RECORD_THRESHOLD
        if (RecordingState::RECORDING_STATE_IDLE != recordingState_)
        {
            // Trigger: end recording or as soon as the audio input goes
            // silent or after a number of samples has passed.
            switch (recordingState_)
            {
            case RecordingState::RECORDING_STATE_START:
                if (samplesSinceRecordInReceived_ < kRecordGateLimit)
                {
                    samplesSinceRecordInReceived_++;
                    if (!recordPressed_)
                    {
                        recordingState_ = RecordingState::RECORDING_STATE_WAITING_ONSET;
                        samplesSinceRecordInReceived_ = 0;
                    }
                }
                else if (recordPressed_)
                {
                    recordingState_ = RecordingState::RECORDING_STATE_RECORDING;
                    recordButton_->Set(1);
                }
                break;
            case RecordingState::RECORDING_STATE_WAITING_ONSET:
                if (samplesSinceRecordInReceived_ < kRecordOnsetLimit)
                {
                    samplesSinceRecordInReceived_++;
                    if (patchState_->inputLevel.getMean() >= kRecordOnsetLevel)
                    {
                        recordingState_ = RecordingState::RECORDING_STATE_RECORDING;
                        recordButton_->Set(1);
                    }
                }
                else
                {
                    recordingState_ = RecordingState::RECORDING_STATE_STOP;
                }
                break;
            case RecordingState::RECORDING_STATE_RECORDING:
                if (!recordPressed_)
                {
                    if (patchState_->inputLevel.getMean() <= kRecordWindupLevel || samplesSinceRecordingStarted_ >= kLooperChannelBufferLength)
                    {
                        recordingState_ = RecordingState::RECORDING_STATE_STOP;
                    }
                    samplesSinceRecordingStarted_ += patchState_->blockSize;
                }
                break;
            case RecordingState::RECORDING_STATE_STOP:
                recordButton_->Set(0);
                patchCtrls_->looperRecording = false;
                recordingState_ = RecordingState::RECORDING_STATE_IDLE;
                break;

            default:
                break;
            }
        }
#endif // USE_RECORD_THRESHOLD
    }

    void UndoRedo()
    {
        if (RandomMode::RANDOM_OSC == randomMode_ || RandomMode::RANDOM_ALL == randomMode_)
        {
            knobs_[PARAM_KNOB_OSC_PITCH]->UndoRedo();
            knobs_[PARAM_KNOB_OSC_DETUNE]->UndoRedo();
        }
        if (RandomMode::RANDOM_LOOPER == randomMode_ || RandomMode::RANDOM_ALL == randomMode_)
        {
            knobs_[PARAM_KNOB_LOOPER_SPEED]->UndoRedo();
            knobs_[PARAM_KNOB_LOOPER_START]->UndoRedo();
            knobs_[PARAM_KNOB_LOOPER_LENGTH]->UndoRedo();
        }
        if (RandomMode::RANDOM_EFFECTS == randomMode_ || RandomMode::RANDOM_ALL == randomMode_)
        {
            knobs_[PARAM_KNOB_FILTER_CUTOFF]->UndoRedo();
            knobs_[PARAM_KNOB_FILTER_RESONANCE]->UndoRedo();

            knobs_[PARAM_KNOB_RESONATOR_TUNE]->UndoRedo();
            knobs_[PARAM_KNOB_RESONATOR_FEEDBACK]->UndoRedo();

            knobs_[PARAM_KNOB_ECHO_REPEATS]->UndoRedo();
            knobs_[PARAM_KNOB_ECHO_DENSITY]->UndoRedo();

            knobs_[PARAM_KNOB_AMBIENCE_DECAY]->UndoRedo();
            knobs_[PARAM_KNOB_AMBIENCE_SPACETIME]->UndoRedo();
        }

        undoRedo_ = false;
    }

    void Randomize()
    {
        // Value (approx): All = 0, Oscillators = 0.5, Looper = 0.75, Fx = 1
        if (patchCtrls_->randomMode > 0.9f)
        {
            randomizeTask_ = 2;
        }
        else if (patchCtrls_->randomMode > 0.7f)
        {
            randomizeTask_ = 1;
        }
        else if (patchCtrls_->randomMode > 0.4f)
        {
            randomizeTask_ = 0;
        }

        // Value: 0 = Mid, 0.33 = Low, 1.00 = High
        RandomAmount randomAmount = RandomAmount::RANDOM_MID;
        if (patchCtrls_->randomAmount > 0.9f)
        {
            randomAmount = RandomAmount::RANDOM_HIGH;
        }
        else if (patchCtrls_->randomAmount > 0.2f)
        {
            randomAmount = RandomAmount::RANDOM_LOW;
        }

        if (randomMode_ == RandomMode::RANDOM_ALL)
        {
            // Spread the randomization in time.
            randomizeTask_ = (randomizeTask_ + 1) % 3;
        }

        switch (randomizeTask_)
        {
        case 0:
            knobs_[PARAM_KNOB_OSC_PITCH]->Randomize(randomAmount, LockableParamName::PARAM_LOCKABLE_MAIN, true);
            knobs_[PARAM_KNOB_OSC_DETUNE]->Randomize(randomAmount);
            break;
        case 1:
            knobs_[PARAM_KNOB_LOOPER_SPEED]->Randomize(randomAmount);
            knobs_[PARAM_KNOB_LOOPER_START]->Randomize(randomAmount);
            knobs_[PARAM_KNOB_LOOPER_LENGTH]->Randomize(randomAmount);
            break;
        case 2:
            knobs_[PARAM_KNOB_FILTER_CUTOFF]->Randomize(randomAmount);
            knobs_[PARAM_KNOB_FILTER_RESONANCE]->Randomize(randomAmount);

            knobs_[PARAM_KNOB_RESONATOR_TUNE]->Randomize(randomAmount);
            knobs_[PARAM_KNOB_RESONATOR_FEEDBACK]->Randomize(randomAmount);

            knobs_[PARAM_KNOB_ECHO_REPEATS]->Randomize(randomAmount);
            knobs_[PARAM_KNOB_ECHO_DENSITY]->Randomize(randomAmount);

            knobs_[PARAM_KNOB_AMBIENCE_DECAY]->Randomize(randomAmount);
            knobs_[PARAM_KNOB_AMBIENCE_SPACETIME]->Randomize(randomAmount);
            break;
        }

        if (RandomMode::RANDOM_ALL == randomMode_)
        {
            if (randomizeTask_ == 2)
            {
                randomizeTask_ = -1;
                randomize_ = false;
            }
        }
        else
        {
            randomize_ = false;
        }
    }

    // Called at block rate
    void Poll()
    {
        if (startup_ && false)
        {
            LoadMainParams();
            LoadAltParams();
            LoadModParams();
            LoadCvParams();
            startup_ = false;

            return;
        }

        for (size_t i = 0; i < PARAM_KNOB_LAST; i++)
        {
            //knobs_[i]->Read(ParamKnob(i));
        }

        for (size_t i = 0; i < PARAM_FADER_LAST; i++)
        {
            //faders_[i]->Read(ParamFader(i));
        }

        for (size_t i = 0; i < PARAM_SWITCH_LAST - 1; i++)
        {
            //switches_[i]->Read(ParamSwitch(i));
        }

        for (size_t i = 0; i < PARAM_CV_LAST; i++)
        {
            //cvs_[i]->Read(ParamCv(i));
        }

        for (size_t i = 0; i < LED_LAST; i++)
        {
            leds_[i]->Read();
        }

        for (size_t i = 0; i < PARAM_MIDI_LAST; i++)
        {
            //midiOuts_[i]->Process();
        }

        HandleLeds();
        // HandleCatchUp();
        HandleLedButtons();

        patchState_->modActive = patchCtrls_->modLevel > 0.1f;

        patchCtrls_->oscOctave = octaveQuantizer_.Process(octave_) + 1;
        if (patchCtrls_->oscOctave != lastOctave_)
        {
            lastOctave_ = patchCtrls_->oscOctave;
            patchState_->oscOctaveFlag = true;
        }
        else
        {
            patchState_->oscOctaveFlag = false;
        }

        float vOctAmount = patchCtrls_->oscPitchCvAmount;
        if (patchState_->cvAttenuverters)
        {
            vOctAmount = CenterMap(patchCtrls_->oscPitchCvAmount);
            // Deadband in the center.
            if (vOctAmount >= -0.1f && vOctAmount <= 0.1f)
            {
                vOctAmount = 0.f;
            }
        }
        float note = vOctOffset1_ + patchCvs_->oscPitch * vOctAmount * vOctScale1_;
        if (patchCvs_->oscPitch >= patchState_->c5)
        {
            note = vOctOffset2_ + patchCvs_->oscPitch * vOctAmount * vOctScale2_;
        }

        if (note > 0 && note < 3.f)
        {
            // Compensate for the two lowest semitones.
            note = 0;
        }
        float interval = note - noteCv_;
        if (interval < -0.4f || interval > 0.4f)
        {
            noteCv_ = note;
        }
        else
        {
            noteCv_ += 0.1f * interval;
        }
        // tune_ is offset by half octave so that C is in the center.
        note = Modulate(tune_ - 0.5f, patchCtrls_->oscPitchModAmount, patchState_->modValue, 0, 0, -1.f, 1.f, patchState_->modAttenuverters);
        note = 12 * note + 12 * patchCtrls_->oscOctave;
        notePot_ += 0.1f * (note - notePot_);
        patchCtrls_->oscPitch = M2F(notePot_ + noteCv_);
        if ((int)(12 * (tune_ - 0.5f) + 12 * patchCtrls_->oscOctave) % 12 == 0)
        {
            patchState_->oscPitchCenterFlag = true;
        }
        else
        {
            patchState_->oscPitchCenterFlag = false;
        }

        patchCtrls_->oscUnison = Clamp(CenterMap(unison_), -1.f, 1.f);
        if (patchCtrls_->oscUnison >= -0.03f && patchCtrls_->oscUnison <= 0.03f)
        {
            patchCtrls_->oscUnison = 0.f;
            patchState_->oscUnisonCenterFlag = true;
        }
        else
        {
            patchState_->oscUnisonCenterFlag = false;
        }

        patchCtrls_->inputVol = MapExpo(inputVol_);
        patchCtrls_->looperVol = MapExpo(looperVol_);
        patchCtrls_->osc1Vol = MapExpo(osc1Vol_);
        patchCtrls_->osc2Vol = MapExpo(osc2Vol_);
        if (patchCtrls_->filterVol >= kOne)
        {
            patchCtrls_->filterVol = 1.f;
        }
        if (patchCtrls_->resonatorVol >= kOne)
        {
            patchCtrls_->resonatorVol = 1.f;
        }
        if (patchCtrls_->echoVol >= kOne)
        {
            patchCtrls_->echoVol = 1.f;
        }
        if (patchCtrls_->ambienceVol >= kOne)
        {
            patchCtrls_->ambienceVol = 1.f;
        }

        // Value (approx): All = 0, Oscillators = 0.5, Looper = 0.75, Fx = 1
        if (patchCtrls_->randomMode > 0.9f)
        {
            randomMode_ = RandomMode::RANDOM_EFFECTS;
        }
        else if (patchCtrls_->randomMode > 0.7f)
        {
            randomMode_ = RandomMode::RANDOM_LOOPER;
        }
        else if (patchCtrls_->randomMode > 0.4f)
        {
            randomMode_ = RandomMode::RANDOM_OSC;
        }
        else
        {
            randomMode_ = RandomMode::RANDOM_ALL;
        }

        if (fadeOutOutput_)
        {
            patchState_->outLevel -= kOutputFadeInc;
            if (patchState_->outLevel <= 0)
            {
                patchState_->outLevel = 0;
                if (saving_)
                {
                    SaveParametersConfig(FuncMode::FUNC_MODE_NONE);
                    SaveParametersConfig(FuncMode::FUNC_MODE_ALT);
                    SaveParametersConfig(FuncMode::FUNC_MODE_MOD);
                    SaveParametersConfig(FuncMode::FUNC_MODE_CV);
                    LedName activeLed = LED_MOD_AMOUNT;
                    if (shiftButton_->IsOn())
                    {
                        activeLed = LED_CV_AMOUNT;
                    }
                    leds_[activeLed]->Blink(2, false, !leds_[activeLed]->IsOn());
                    fadeOutOutput_ = false;
                    fadeInOutput_ = true;
                    saving_ = false;
                }
                else
                {
                    fadeOutOutput_ = false;
                    fadeInOutput_ = true;
                }
            }
        }

        if (fadeInOutput_)
        {
            patchState_->outLevel += kOutputFadeInc;
            if (patchState_->outLevel >= 1)
            {
                patchState_->outLevel = 1;
                fadeInOutput_ = false;
            }
        }

        // handled by VCV
        if (randomize_ && false)
        {
            Randomize();
        }

        // handled by VCV
        if (undoRedo_ && false)
        {
            UndoRedo();
        }

        if (doRandomSlew_)
        {
            randomSlewInc_ += patchState_->randomSlew;
            if (randomSlewInc_ >= 1.f)
            {
                doRandomSlew_ = false;
                leds_[LED_RANDOM]->Blink(2);
            }
        }
    }
};
