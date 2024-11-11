// Ported and adapted from ParamController class of Emilie Gillet's Plaits.

#pragma once

#include "Commons.h"
#include "Led.h"
#include "TGate.h"

// extern PatchProcessor* getInitialisingPatchProcessor();

enum ParamKnob {
    PARAM_KNOB_LOOPER_SPEED,
    PARAM_KNOB_LOOPER_START,
    PARAM_KNOB_LOOPER_LENGTH,
    PARAM_KNOB_OSC_PITCH,
    PARAM_KNOB_OSC_DETUNE,
    PARAM_KNOB_FILTER_CUTOFF,
    PARAM_KNOB_FILTER_RESONANCE,
    PARAM_KNOB_RESONATOR_TUNE,
    PARAM_KNOB_RESONATOR_FEEDBACK,
    PARAM_KNOB_ECHO_REPEATS,
    PARAM_KNOB_ECHO_DENSITY,
    PARAM_KNOB_AMBIENCE_DECAY,
    PARAM_KNOB_AMBIENCE_SPACETIME,
    PARAM_KNOB_MOD_LEVEL,
    PARAM_KNOB_MOD_SPEED,
    PARAM_KNOB_LAST
};

const PatchParameterId paramKnobMap[PARAM_KNOB_LAST] = {
    PARAMETER_CA,
    PARAMETER_CF, // Looper start
    PARAMETER_CD,
    PARAMETER_CE,
    PARAMETER_CC,
    PARAMETER_DE,
    PARAMETER_CB,
    PARAMETER_CG,
    PARAMETER_CH,
    PARAMETER_DF,
    PARAMETER_DG,
    PARAMETER_DB,
    PARAMETER_DA,
    PARAMETER_DC,
    PARAMETER_DD,
};

enum ParamFader {
    PARAM_FADER_LOOPER_VOL,
    PARAM_FADER_OSC1_VOL,
    PARAM_FADER_OSC2_VOL,
    PARAM_FADER_IN_VOL,
    PARAM_FADER_FILTER_VOL,
    PARAM_FADER_RESONATOR_VOL,
    PARAM_FADER_ECHO_VOL,
    PARAM_FADER_AMBIENCE_VOL,
    PARAM_FADER_LAST
};

const PatchParameterId paramFaderMap[PARAM_FADER_LAST] = {
    PARAMETER_BA,
    PARAMETER_BH,
    PARAMETER_BG,
    PARAMETER_BF,
    PARAMETER_BE,
    PARAMETER_BD,
    PARAMETER_BC,
    PARAMETER_BB,
};

enum ParamCv {
    PARAM_CV_LOOPER_SPEED,
    PARAM_CV_LOOPER_START,
    PARAM_CV_LOOPER_LENGTH,
    PARAM_CV_OSC_PITCH,
    PARAM_CV_OSC_DETUNE,
    PARAM_CV_FILTER_CUTOFF,
    PARAM_CV_FILTER_RESONANCE,
    PARAM_CV_RESONATOR_TUNE,
    PARAM_CV_RESONATOR_FEEDBACK,
    PARAM_CV_ECHO_REPEATS,
    PARAM_CV_ECHO_DENSITY,
    PARAM_CV_AMBIENCE_DECAY,
    PARAM_CV_AMBIENCE_SPACETIME,
    PARAM_CV_LAST
};

const PatchParameterId paramCvMap[PARAM_CV_LAST] = {
    PARAMETER_G,
    PARAMETER_E,
    PARAMETER_F,
    PARAMETER_AD,
    PARAMETER_A,
    PARAMETER_B,
    PARAMETER_B,
    PARAMETER_C,
    PARAMETER_C,
    PARAMETER_D,
    PARAMETER_D,
    PARAMETER_AC,
    PARAMETER_AC,
};

enum ParamSwitch {
    PARAM_SWITCH_RANDOM_MODE,
    PARAM_SWITCH_RANDOM_AMOUNT,
    PARAM_SWITCH_OSC_USE_SSWT,
    PARAM_SWITCH_LAST,
};

const PatchParameterId paramSwitchMap[PARAM_SWITCH_LAST - 1] = {
    PARAMETER_DH, // Random mode
    PARAMETER_AA, // Random amount
};

enum ParamButton {
    PARAM_BUTTON_RECORD,
    PARAM_BUTTON_RANDOM,
    PARAM_BUTTON_SHIFT,
    PARAM_BUTTON_MOD_CV,
    PARAM_BUTTON_LAST,
};

enum ParamState {
    PARAM_STATE_TRACKING,
    PARAM_STATE_LOCKED,
    PARAM_STATE_CATCHING_UP,
    PARAM_STATE_MORPHING
};

enum ParamCatchUp {
    PARAM_CATCH_UP_NONE,
    PARAM_CATCH_UP_LEFT, // The param must move left to catch up
    PARAM_CATCH_UP_RIGHT, // The param must move right to catch up
    PARAM_CATCH_UP_DONE, // The param has finished catching up
};

enum LockableParamName {
    PARAM_LOCKABLE_MAIN,
    PARAM_LOCKABLE_ALT,
    PARAM_LOCKABLE_MOD,
    PARAM_LOCKABLE_CV,
    PARAM_LOCKABLE_LAST
};

enum LedButtonType {
    LED_BUTTON_TYPE_MOMENTARY,
    LED_BUTTON_TYPE_TOGGLE,
};

class LockableParam
{
private:
    PatchState* patchState_;

    ParamState state_, nextState_;
    ParamCatchUp catchUp_;

    LockableParamName paramName_, selectedParam_;

    float* value_;
    float initValue_;
    float storedValue_;
    float previousValue_;
    float ctrlValue_;
    float nextValue_;
    float slewInc_;

    uint32_t randomSeed_;

    bool bipolar_;
    bool active_;
    bool softTakeover_;

public:
    LockableParam() {}
    ~LockableParam() {}

    inline void Init(PatchState* patchState, float* value, LockableParamName paramName, ParamState state, bool bipolar = false, bool active = true)
    {
        patchState_ = patchState;
        value_ = value;
        paramName_ = paramName;
        state_ = nextState_ = state;
        catchUp_ = ParamCatchUp::PARAM_CATCH_UP_NONE;
        if (active)
        {
            ctrlValue_ = *value;
            previousValue_ = *value;
            storedValue_ = *value;
            initValue_ = *value;
            nextValue_ = *value;
        }
        bipolar_ = bipolar;
        active_ = active;
        softTakeover_ = patchState->softTakeover;
        randomSeed_ = rand();
        slewInc_ = 0;

        selectedParam_ = LockableParamName::PARAM_LOCKABLE_MAIN;
    }

    bool IsActive()
    {
        return active_;
    }

    inline void Lock()
    {
        if (!IsActive())
        {
            return;
        }

        if (state_ == PARAM_STATE_LOCKED)
        {
            return;
        }

        state_ = PARAM_STATE_LOCKED;
        storedValue_ = *value_;
    }

    inline void Unlock(bool slew = false)
    {
        if (!IsActive())
        {
            return;
        }

        if (slew)
        {
            nextState_ = PARAM_STATE_CATCHING_UP;

            return;
        }

        if (state_ == PARAM_STATE_LOCKED)
        {
            state_ = PARAM_STATE_CATCHING_UP;
        }
        else
        {
            state_ = PARAM_STATE_TRACKING;
        }
    }

    inline void SelectParam(LockableParamName selectedParam)
    {
        selectedParam_ = selectedParam;
    }

    inline void SetBipolar(bool bipolar)
    {
        bipolar_ = bipolar;
    }

    inline void SetValue(float value, bool slew = false)
    {
        previousValue_ = *value_;
        if (slew)
        {
            nextValue_ = value;
            state_ = PARAM_STATE_MORPHING;
            slewInc_ = 0;
        }
        else
        {
            storedValue_ = value;
            *value_ = value;
        }
    }

    inline void UndoRedo()
    {
        SetValue(previousValue_);
    }

    inline void Randomize(RandomAmount amount, bool semitones = false)
    {
        randomSeed_ ^= randomSeed_ << 13;
        randomSeed_ ^= randomSeed_ >> 17;
        randomSeed_ ^= randomSeed_ << 5;
        float rf = randomSeed_ * (1 / 4294967296.0f); // Random number between 0 and 1
        float s1 = *value_ < 0.5f ? 1 : -1;
        float s2 = rf < 0.5f ? 1 : -1;

        float v;

        if (amount == RandomAmount::RANDOM_HIGH)
        {
            if (rf < 0.1f)
            {
                v = (*value_ < 0.5f ? 0.9f : 0.1f);
            }
            else
            {
                //v = Wrap(*value_ + rf * s1);
                v = rf;
            }
        }
        else if (amount == RandomAmount::RANDOM_MID)
        {
            if (semitones)
            {
                if (rf < 0.1f)
                {
                    v = *value_ + (0.083f * 4 * s2);
                }
                else if (rf < 0.5f)
                {
                    v = *value_ + (0.083f * 3 * s2);
                }
                else
                {
                    v = *value_ + (0.083f * 2 * s2);
                }
            }
            else
            {
                v = *value_ + rf * 0.1f * s1;
            }
        }
        else
        {
            if (semitones)
            {
                if (rf < 0.1f)
                {
                    v = *value_ + (0.083f * 2 * s2);
                }
                else
                {
                    v = *value_ + 0.083f * s2;
                }
            }
            else
            {
                v = *value_ + rf * 0.02f * s1;
            }
        }

        v = Clamp(v, 0, 1);

        if (semitones)
        {
            v = Quantize(v, 12);
        }

        SetValue(v, semitones ? patchState_->randomHasSlew : true);
    }

    inline void Realign()
    {
        if (!IsActive())
        {
            return;
        }

        SetValue(ctrlValue_, true);
        nextState_ = PARAM_STATE_TRACKING;
    }

    inline void Reset()
    {
        if (!IsActive())
        {
            return;
        }

        SetValue(initValue_);
        state_ = PARAM_STATE_CATCHING_UP;
    }

    inline void Clear()
    {
        if (!IsActive())
        {
            return;
        }

        SetValue(0.f);
        state_ = PARAM_STATE_CATCHING_UP;
    }

    inline void InitValue(float value)
    {
        if (!IsActive())
        {
            return;
        }

        SetValue(value);
        state_ = PARAM_STATE_CATCHING_UP;
    }

    ParamState GetState()
    {
        return state_;
    }

    ParamCatchUp GetCatchUpState()
    {
        return catchUp_;
    }

    inline void Process(float ctrlValue, bool moving)
    {
        ctrlValue_ = ctrlValue;

        switch (state_)
        {
            // The parameter tracks the position of the control.
            case PARAM_STATE_TRACKING:
                catchUp_ = ParamCatchUp::PARAM_CATCH_UP_NONE;
                *value_ = ctrlValue_;
                if (moving)
                {
                    previousValue_ = *value_;
                }
                break;

            // The parameter is locked.
            case PARAM_STATE_LOCKED:
                catchUp_ = ParamCatchUp::PARAM_CATCH_UP_NONE;
                break;

            // The control adjusts the parameter until the position of the
            // control and the value of the parameter match again.
            case PARAM_STATE_CATCHING_UP:
                {
                    float d = ctrlValue_ - storedValue_;
                    if (softTakeover_)
                    {
                        if (fabsf(d) > kParamCatchUpDelta)
                        {
                            /// Still needs to catch up.
                            catchUp_ = d > 0 ? ParamCatchUp::PARAM_CATCH_UP_LEFT : ParamCatchUp::PARAM_CATCH_UP_RIGHT;
                        }
                        else
                        {
                            // Done catching up.
                            state_ = PARAM_STATE_TRACKING;
                            catchUp_ = ParamCatchUp::PARAM_CATCH_UP_DONE;
                        }
                    }
                    else if (moving)
                    {
                        // Just catch up if soft takeover is disabled and
                        // the control is moving.
                        state_ = PARAM_STATE_TRACKING;
                        catchUp_ = ParamCatchUp::PARAM_CATCH_UP_DONE;
                    }
                }
                break;

            case PARAM_STATE_MORPHING:
            {
                *value_ = *value_ * (1.f - patchState_->randomSlew) + nextValue_ * patchState_->randomSlew;
                slewInc_ += patchState_->randomSlew;

                if (slewInc_ >= 1.f)
                {
                    storedValue_ = *value_;
                    state_ = nextState_;
                }

                break;
            }
        }
    }
};

class CatchUpController
{
protected:
    ParamCatchUp catchUp_;

    float lpCoeff_;
    float readValue_;
    float ctrlValue_;
    float storedValue_;
    float movementDelta_;

    bool moving_;
    bool first_;

    int samplesSinceStartMoving_;
    int samplesSinceStopMoving_;

public:
    CatchUpController(){}
    virtual ~CatchUpController(){}

    inline bool IsMoving()
    {
        return moving_;
    }

    virtual ParamCatchUp GetCatchUpState() = 0;
    virtual bool Process() = 0;
    virtual void Reset() = 0;
};

class KnobController : public CatchUpController
{
private:
    PatchState* patchState_;

    LockableParam lockableParams_[PARAM_LOCKABLE_LAST];
    LockableParamName selectedParam_;

public:
    KnobController() {}
    KnobController(PatchState* patchState, float* mainParam, float* altParam, float* modParam, float* cvParam, float lpCoeff, float movementDelta)
    {
        patchState_ = patchState;
        lockableParams_[LockableParamName::PARAM_LOCKABLE_MAIN].Init(patchState, mainParam, LockableParamName::PARAM_LOCKABLE_MAIN, ParamState::PARAM_STATE_TRACKING);
        lockableParams_[LockableParamName::PARAM_LOCKABLE_ALT].Init(patchState, altParam, LockableParamName::PARAM_LOCKABLE_ALT, ParamState::PARAM_STATE_LOCKED, false, altParam != NULL);
        lockableParams_[LockableParamName::PARAM_LOCKABLE_MOD].Init(patchState, modParam, LockableParamName::PARAM_LOCKABLE_MOD, ParamState::PARAM_STATE_LOCKED, patchState->modAttenuverters, modParam != NULL);
        lockableParams_[LockableParamName::PARAM_LOCKABLE_CV].Init(patchState, cvParam, LockableParamName::PARAM_LOCKABLE_CV, ParamState::PARAM_STATE_LOCKED, patchState->cvAttenuverters, cvParam != NULL);

        selectedParam_ = LockableParamName::PARAM_LOCKABLE_MAIN;
        catchUp_ = ParamCatchUp::PARAM_CATCH_UP_NONE;
        movementDelta_ = movementDelta;

        lpCoeff_ = lpCoeff;
        readValue_ = 0.f;
        ctrlValue_ = 0.f;
        storedValue_ = 0.f;

        first_ = true;
        moving_ = false;
        samplesSinceStartMoving_ = 0;
        samplesSinceStopMoving_ = 0;
    }
    ~KnobController() {}

    static KnobController* create(
        PatchState* patchState,
        float* mainParam,
        float* altParam = NULL,
        float* modParam = NULL,
        float* cvParam = NULL,
        float lpCoeff = 0.01f,
        float movementDelta = 0.01f
    ) {
        return new KnobController(patchState, mainParam, altParam, modParam, cvParam, lpCoeff, movementDelta);
    }

    static void destroy(KnobController* obj)
    {
        delete obj;
    }

    inline void SetBipolarMod(bool bipolar)
    {
        lockableParams_[LockableParamName::PARAM_LOCKABLE_MOD].SetBipolar(bipolar);
    }

    inline void SetBipolarCv(bool bipolar)
    {
        lockableParams_[LockableParamName::PARAM_LOCKABLE_CV].SetBipolar(bipolar);
    }

    inline void SetValue(float value, LockableParamName name = LockableParamName::PARAM_LOCKABLE_MAIN)
    {
        if (LockableParamName::PARAM_LOCKABLE_MAIN == name)
        {
            lockableParams_[name].InitValue(value);
        }
        else
        {
            lockableParams_[name].SetValue(value);
        }
    }

    inline void UndoRedo(LockableParamName name = LockableParamName::PARAM_LOCKABLE_MAIN)
    {
        lockableParams_[name].UndoRedo();
    }

    inline void Randomize(RandomAmount amount, LockableParamName name = LockableParamName::PARAM_LOCKABLE_MAIN, bool semitones = false)
    {
        for (size_t i = 0; i < LockableParamName::PARAM_LOCKABLE_LAST; i++)
        {
            lockableParams_[i].Lock();
        }
        lockableParams_[name].Randomize(amount, semitones);
        lockableParams_[selectedParam_].Unlock(semitones ? patchState_->randomHasSlew : true);
    }

    inline void Reset()
    {
        switch (selectedParam_)
        {
        case LockableParamName::PARAM_LOCKABLE_MOD:
            lockableParams_[selectedParam_].Clear();
            break;
        case LockableParamName::PARAM_LOCKABLE_CV:
            lockableParams_[selectedParam_].Reset();
            break;

        default:
            lockableParams_[LockableParamName::PARAM_LOCKABLE_MAIN].Realign();
            break;
        }
    }

    inline void SetFuncMode(FuncMode funcMode)
    {
        LockableParamName selectedParam;

        switch (funcMode)
        {
        case FUNC_MODE_ALT:
            selectedParam = PARAM_LOCKABLE_ALT;
            break;

        case FUNC_MODE_MOD:
            selectedParam = PARAM_LOCKABLE_MOD;
            break;

        case FUNC_MODE_CV:
            selectedParam = PARAM_LOCKABLE_CV;
            break;

        default:
            selectedParam = PARAM_LOCKABLE_MAIN;
            break;
        }

        // Return if the selected parameter didn't change.
        if (selectedParam == selectedParam_)
        {
            return;
        }

        selectedParam_ = selectedParam;

        // If the selected parameter is inactive, set it to the main parameter
        // and return,
        if (!lockableParams_[selectedParam_].IsActive()) {
            selectedParam_ = LockableParamName::PARAM_LOCKABLE_MAIN;

            return;
        }

        for (size_t i = 0; i < LockableParamName::PARAM_LOCKABLE_LAST; i++)
        {
            lockableParams_[i].SelectParam(selectedParam_);
            lockableParams_[i].Lock();
        }
        lockableParams_[selectedParam_].Unlock();
    }

    inline void Read(ParamKnob ctrl)
    {
        readValue_ = 0.f; // getInitialisingPatchProcessor()->patch->getParameterValue(paramKnobMap[ctrl]);

        if (lpCoeff_ > 0 && !first_)
        {
            ONE_POLE(ctrlValue_, readValue_, lpCoeff_);
        }
        else
        {
            ctrlValue_ = readValue_;
        }
    }

    inline bool Process()
    {
        bool started = samplesSinceStartMoving_ > kParamStartMovementLimit;
        bool stopped = samplesSinceStopMoving_ > kParamStopMovementLimit;

        if (started && first_)
        {
            // Avoid a false positive on startup.
            storedValue_ = readValue_;
            first_ = false;
        }

        float d = fabsf(readValue_ - storedValue_);
        if (d > movementDelta_)
        {
            if (started)
            {
                // Moving.
                storedValue_ = readValue_;
                moving_ = true;
            }
            else
            {
                // Waiting to lock on movement (or not).
                samplesSinceStartMoving_++;
            }
            samplesSinceStopMoving_ = 0;
        }
        else
        {
            if (stopped)
            {
                // Stopped.
                moving_ = false;
            }
            else
            {
                // Waiting to stop (or not).
                samplesSinceStopMoving_++;
            }
            // Not moving.
            samplesSinceStartMoving_ = 0;
        }

        for (size_t i = 0; i < LockableParamName::PARAM_LOCKABLE_LAST; i++)
        {
            lockableParams_[i].Process(ctrlValue_, moving_);
        }

        return moving_;
    }

    inline ParamCatchUp GetCatchUpState()
    {
        ParamCatchUp catchUp = ParamCatchUp::PARAM_CATCH_UP_NONE;

        for (size_t i = 0; i < LockableParamName::PARAM_LOCKABLE_LAST; i++)
        {
            if (lockableParams_[i].IsActive())
            {
                catchUp = lockableParams_[i].GetCatchUpState();
                if (ParamCatchUp::PARAM_CATCH_UP_NONE != catchUp)
                {
                    // Found one parameter that needs to catch up.
                    break;
                }
            }
        }

        if (catchUp != catchUp_)
        {
            // Update the catch up state in case the parameter was tracking and
            // now needs to catch up or was catching up and now is tracking
            // again.
            catchUp_ = catchUp;
        }

        return catchUp_;
    }
};

class FaderController : public CatchUpController
{
private:
    LockableParam lockableParam_;

public:
    FaderController() {}
    FaderController(PatchState* patchState, float* param, float lpCoeff, float movementDelta)
    {
        lockableParam_.Init(patchState, param, LockableParamName::PARAM_LOCKABLE_MAIN, ParamState::PARAM_STATE_TRACKING);

        catchUp_ = ParamCatchUp::PARAM_CATCH_UP_NONE;
        movementDelta_ = movementDelta;

        lpCoeff_ = lpCoeff;
        readValue_ = 0.f;
        ctrlValue_ = 0.f;
        storedValue_ = 0.f;

        first_ = true;
        moving_ = false;
        samplesSinceStartMoving_ = 0;
        samplesSinceStopMoving_ = 0;
    }
    ~FaderController() {}

    static FaderController* create(
        PatchState* patchState,
        float* param,
        float lpCoeff = 0.02f,
        float movementDelta = 0.01f
    ) {
        return new FaderController(patchState, param, lpCoeff, movementDelta);
    }

    static void destroy(FaderController* obj)
    {
        delete obj;
    }

    inline void SetValue(float value)
    {
        lockableParam_.InitValue(value);
    }

    inline void Reset()
    {
        lockableParam_.Realign();
    }

    inline void Read(ParamFader fader)
    {
        readValue_ = 0.f; // getInitialisingPatchProcessor()->patch->getParameterValue(paramFaderMap[fader]);

        if (lpCoeff_ > 0 && !first_)
        {
            ONE_POLE(ctrlValue_, readValue_, lpCoeff_);
        }
        else
        {
            ctrlValue_ = readValue_;
        }
    }

    inline bool Process()
    {
        bool started = samplesSinceStartMoving_ > kParamStartMovementLimit;
        bool stopped = samplesSinceStopMoving_ > kParamStopMovementLimit;

        if (started && first_)
        {
            // Avoid a false positive on startup.
            storedValue_ = readValue_;
            first_ = false;
        }

        float d = fabsf(readValue_ - storedValue_);
        if (d > movementDelta_)
        {
            if (started)
            {
                // Moving.
                storedValue_ = readValue_;
                moving_ = true;
            }
            else
            {
                // Waiting to lock on movement (or not).
                samplesSinceStartMoving_++;
            }
            samplesSinceStopMoving_ = 0;
        }
        else
        {
            if (stopped)
            {
                // Stopped.
                moving_ = false;
            }
            else
            {
                // Waiting to stop (or not).
                samplesSinceStopMoving_++;
            }
            // Not moving.
            samplesSinceStartMoving_ = 0;
        }

        lockableParam_.Process(ctrlValue_, moving_);

        return moving_;
    }

    inline ParamCatchUp GetCatchUpState()
    {
        ParamCatchUp catchUp = lockableParam_.GetCatchUpState();

        if (catchUp != catchUp_)
        {
            // Update the catch up state in case the parameter was tracking and
            // now needs to catch up or was catching up and now is tracking
            // again.
            catchUp_ = catchUp;
        }

        return catchUp_;
    }
};

class SwitchController
{
private:
    float* mainParam_;
    float switchValue_;
    float undoValue_;
    float redoValue_;

    bool canUndo_;
    bool canRedo_;

public:
    SwitchController(float* mainParam)
    {
        mainParam_ = mainParam;
        switchValue_ = 0.f;
        undoValue_ = 0.f;
        redoValue_ = 0.f;
        canUndo_ = false;
        canRedo_ = false;
    }
    ~SwitchController() {}

    static SwitchController* create(float* mainParam)
    {
        return new SwitchController(mainParam);
    }

    static void destroy(SwitchController* obj)
    {
        delete obj;
    }

    inline void Set(float value)
    {
        switchValue_ = value;
        *mainParam_ = switchValue_;
    }

    inline void UndoRedo()
    {
        if (canUndo_)
        {
            *mainParam_ = undoValue_;

            canUndo_ = false;
            canRedo_ = true;
        }
        else if (canRedo_)
        {
            *mainParam_ = redoValue_;

            canUndo_ = true;
            canRedo_ = false;
        }
    }

    inline void Randomize(bool undoRedo = false)
    {
        if (undoRedo)
        {
            UndoRedo();
        }
        else
        {
            undoValue_ = *mainParam_;
            redoValue_ = RandomFloat();
            *mainParam_ = redoValue_;
            canUndo_ = true;
        }
    }

    inline void Reset()
    {
        *mainParam_ = switchValue_;
    }

    inline void Read(ParamSwitch swtch)
    {
        float v = 0.f; // getInitialisingPatchProcessor()->patch->getParameterValue(paramSwitchMap[swtch]);
        if (v != switchValue_)
        {
            switchValue_ = v;
            if (switchValue_ != *mainParam_)
            {
                *mainParam_ = switchValue_;
            }
        }
    }
};

class CvController
{
private:
    float* cvParam_;

    float cvValue_;
    float lpCoeff_;
    float offset_;
    float delta_;
    float mult_;

public:
    CvController(
        float* cvParam,
        float lpCoeff,
        float offset,
        float mult,
        float delta
    ) {
        cvParam_ = cvParam;
        lpCoeff_ = lpCoeff;
        offset_ = offset;
        mult_ = mult;
        delta_ = delta;

        cvValue_ = 0.f;
    }
    ~CvController() {}

    // -0.495 .. 0.99
    static CvController* create(
        float* cvParam,
        float lpCoeff = kCvLpCoeff,
        float offset = kCvOffset,
        float mult = kCvMult,
        float delta = kCvDelta
    ) {
        return new CvController(cvParam, lpCoeff, offset, mult, delta);
    }

    static void destroy(CvController* obj)
    {
        delete obj;
    }

    // Called at block rate
    inline void Read(ParamCv cv)
    {
        // -5V = 0
        //  0V = 0.3
        // 10V = 0.98
        float value = 0.f; // getInitialisingPatchProcessor()->patch->getParameterValue(paramCvMap[cv]);
        if (offset_ != 0)
        {
            value = value * mult_ + offset_;
        }

        if (lpCoeff_ > 0)
        {
            ONE_POLE(cvValue_, value, lpCoeff_);
        }
        else
        {
            cvValue_ = value;
        }

        if (delta_ == 0 || fabsf(cvValue_ - *cvParam_) > delta_)
        {
            *cvParam_ = cvValue_;
        }
    }
};

class RecordButtonController
{
private:
    Led* led_;
    TGate trigger_;
    FuncMode funcMode_;

    bool* on_;

    bool mainOn_;
    bool funcOn_;
    // bool latched_;
    bool hold_;
    bool pressed_;
    bool trig_;
    bool doBlink_;
    bool gate_;

    int samplesSincePressed_;
    int samplesSinceHeld_;

    const int kGateLimit, kHoldLimit;

public:
    RecordButtonController(Led* led, float sampleRate, float blockRate) : kGateLimit(kGateLimitSeconds * blockRate), kHoldLimit(kHoldLimitSeconds * blockRate)
    {
        led_ = led;

        funcMode_ = FuncMode::FUNC_MODE_NONE;
        mainOn_ = false;
        funcOn_ = false;
        on_ = &mainOn_;

        hold_ = false;
        pressed_ = false;
        trig_ = false;
        doBlink_ = false;

        trigger_.Init(sampleRate);

        samplesSincePressed_ = 0;
        samplesSinceHeld_ = 0;
    }
    ~RecordButtonController() {}

    static RecordButtonController* create(Led* led, float sampleRate, float blockRate)
    {
        return new RecordButtonController(led, sampleRate, blockRate);
    }

    static void destroy(RecordButtonController* obj)
    {
        delete obj;
    }

    inline bool IsOn()
    {
        return *on_;
    }

    // True when the button is pressed, false when released.
    inline bool IsPressed()
    {
        return pressed_;
    }

    // True when the button is being kept pressed for some time, false when released.
    inline bool IsHeld()
    {
        return hold_;
    }

    // True when the button is being kept pressed for some time, false after a little more.
    inline bool IsGate()
    {
        return gate_;
    }

    inline void Set(bool on)
    {
        *on_ = on;
        led_->Set(on);
    }

    inline void Toggle()
    {
        Set(!*on_);
    }

    inline void SetFuncMode(FuncMode funcMode)
    {
        // Set func mode only if the led button isn't pressed.
        if (!pressed_)
        {
            funcMode_ = funcMode;
            hold_ = false;
            doBlink_ = false;
            trig_ = false;
            if (FuncMode::FUNC_MODE_NONE == funcMode_)
            {
                // on_ now points to the main parameter
                on_ = &mainOn_;

                if (mainOn_)
                {
                    // While recording, blink the led.
                    led_->Blink(0);
                    led_->On();
                }
            }
            else
            {
                // The FUNC parameter status is never actually set within this
                // controller, only from outside, so we leave it off.
                funcOn_ = 0;
                // on_ now points to the selected FUNC parameter
                on_ = &funcOn_;

                if (mainOn_)
                {
                    // While recording, blink the led.
                    led_->Blink(-1);
                }
            }
        }
    }

    inline void Trig(bool pressed)
    {
        pressed_ = pressed;

        // Act only when the led button is pressed.
        if (pressed_)
        {
            Toggle();
            // Blink the led once only when in a func mode, unless the main
            // parameter is on (recording).
            if (FuncMode::FUNC_MODE_NONE != funcMode_ && !mainOn_)
            {
                trig_ = true;
                doBlink_ = true;
            }
            samplesSincePressed_ = 0;
            samplesSinceHeld_ = 0;
        }
    }

    // Called at block rate
    inline void Process()
    {
        if (doBlink_)
        {
            if (!trigger_.Process(trig_))
            {
                Toggle();
                doBlink_ = false;
            }
            trig_ = false;
        }
        else if (pressed_)
        {
            int limit = FuncMode::FUNC_MODE_NONE == funcMode_ ? kGateLimit : kHoldLimit;
            if (hold_)
            {
                if (samplesSinceHeld_ < limit)
                {
                    // Holding.
                    samplesSinceHeld_++;
                }
                else
                {
                    gate_ = false;
                }
            }
            else
            {
                if (samplesSincePressed_ < limit)
                {
                    // Holding.
                    samplesSincePressed_++;
                }
                else
                {
                    hold_ = true;
                    gate_ = true;
                    samplesSinceHeld_ = 0;
                }
            }
        }
        else if (hold_)
        {
            // Released.
            if (FuncMode::FUNC_MODE_NONE == funcMode_)
            {
                Toggle();
            }
            hold_ = false;
            gate_ = false;
        }
    }
};

class RandomButtonController
{
private:
    Led* led_;
    TGate trigger_;
    FuncMode funcMode_;

    bool* on_;

    bool mainOn_;
    bool funcOn_;
    bool latched_;
    bool hold_;
    bool pressed_;
    bool trig_;
    bool doBlink_;
    bool gate_;

    int samplesSincePressed_;
    int samplesSinceHeld_;

    const int kHoldLimit;

public:
    RandomButtonController(Led* led, float sampleRate, float blockRate) : kHoldLimit(kHoldLimitSeconds * blockRate)
    {
        led_ = led;

        funcMode_ = FuncMode::FUNC_MODE_NONE;
        mainOn_ = false;
        funcOn_ = false;
        on_ = &mainOn_;

        latched_ = false;
        hold_ = false;
        pressed_ = false;
        trig_ = false;
        doBlink_ = false;
        gate_ = false;

        trigger_.Init(sampleRate);

        samplesSincePressed_ = 0;
        samplesSinceHeld_ = 0;
    }
    ~RandomButtonController() {}

    static RandomButtonController* create(Led* led, float sampleRate, float blockRate)
    {
        return new RandomButtonController(led, sampleRate, blockRate);
    }

    static void destroy(RandomButtonController* obj)
    {
        delete obj;
    }

    inline bool IsOn()
    {
        return *on_ && !latched_;
    }

    // True when the button is pressed, false when released.
    inline bool IsPressed()
    {
        return pressed_;
    }

    // True when the button is being kept pressed for some time, false when released.
    inline bool IsHeld()
    {
        return hold_;
    }

    // True when the button is being kept pressed for some time, false after a little more.
    inline bool IsGate()
    {
        return gate_;
    }

    inline bool IsLatched()
    {
        return latched_;
    }

    inline void Set(bool on)
    {
        *on_ = on;
        led_->Set(on);
    }

    inline void SetFuncMode(FuncMode funcMode)
    {
        // Set func mode only if the led button isn't pressed.
        if (!pressed_)
        {
            funcMode_ = funcMode;
            hold_ = false;
            doBlink_ = false;
            trig_ = false;
            if (FuncMode::FUNC_MODE_NONE == funcMode_)
            {
                on_ = &mainOn_;
            }
            else
            {
                funcOn_ = 0;
                on_ = &funcOn_;
            }
            led_->Set(*on_);
        }
    }

    inline void Trig(bool pressed)
    {
        pressed_ = pressed;

        // Act only when the led button is pressed.
        if ((FuncMode::FUNC_MODE_NONE == funcMode_ && !pressed_) || (FuncMode::FUNC_MODE_NONE != funcMode_ && pressed_))
        {
            Set(!*on_);
            trig_ = true;
            doBlink_ = true;
            samplesSincePressed_ = 0;
            samplesSinceHeld_ = 0;
        }
    }

    // Called at block rate
    inline void Process()
    {
        if (doBlink_)
        {
            if (!trigger_.Process(trig_))
            {
                Set(!*on_);
                doBlink_ = false;
            }
            trig_ = false;
        }
        else if (pressed_)
        {
            if (hold_)
            {
                if (samplesSinceHeld_ < kHoldLimit)
                {
                    // Holding.
                    samplesSinceHeld_++;
                }
                else
                {
                    gate_ = false;
                }
            }
            else
            {
                if (samplesSincePressed_ < kHoldLimit)
                {
                    // Holding.
                    samplesSincePressed_++;
                }
                else
                {
                    hold_ = true;
                    gate_ = true;
                    samplesSinceHeld_ = 0;
                    /*
                    // The button can be latched only when not in a func mode.
                    if (FuncMode::FUNC_MODE_NONE == funcMode_)
                    {
                        latched_ = !latched_;
                        Set(!*on_);
                    }
                    else
                    {
                        Set(0);
                    }
                    */
                }
            }
        }
        else if (hold_)
        {
            // Released.
            hold_ = false;
            gate_ = false;
            Set(0);
        }
    }
};

class ShiftButtonController
{
private:
    Led* led_;

    bool on_;
    bool latched_;
    bool hold_;
    bool pressed_;
    bool trig_;
    bool gate_;

    int samplesSincePressed_;
    int samplesSinceHeld_;

    const int kGateLimit;

public:
    ShiftButtonController(Led* led, float sampleRate, float blockRate) : kGateLimit(blockRate * kGateLimitSeconds)
    {
        led_ = led;

        on_ = false;
        latched_ = false;
        hold_ = false;
        pressed_ = false;
        trig_ = false;
        gate_ = false;

        samplesSincePressed_ = 0;
        samplesSinceHeld_ = 0;
    }
    ~ShiftButtonController() {}

    static ShiftButtonController* create(Led* led, float sampleRate, float blockRate)
    {
        return new ShiftButtonController(led, sampleRate, blockRate);
    }

    static void destroy(ShiftButtonController* obj)
    {
        delete obj;
    }

    inline bool IsOn()
    {
        return on_;
    }

    // True when the button is pressed, false when released.
    inline bool IsPressed()
    {
        return pressed_;
    }

    // True when the button is being kept pressed for some time, false when released.
    inline bool IsHeld()
    {
        return hold_;
    }

    // True when the button is being kept pressed for some time, false after a little more.
    inline bool IsGate()
    {
        return gate_;
    }

    inline void Set(bool on)
    {
        on_ = on;
        led_->Set(on_);
    }

    inline void Trig(bool pressed)
    {
        pressed_ = pressed;

        // Act only when the led button is pressed.
        if (pressed_)
        {
            Set(!on_);
            samplesSincePressed_ = 0;
            samplesSinceHeld_ = 0;
        }
    }

    // Called at block rate
    inline void Process()
    {
        if (!on_)
        {
            return;
        }

        if (pressed_)
        {
            if (hold_)
            {
                if (samplesSinceHeld_ < kGateLimit)
                {
                    // Holding.
                    samplesSinceHeld_++;
                }
                else
                {
                    gate_ = false;
                }
            }
            else
            {
                if (samplesSincePressed_ < kGateLimit)
                {
                    // Holding.
                    samplesSincePressed_++;
                }
                else
                {
                    hold_ = true;
                    gate_ = true;
                    samplesSinceHeld_ = 0;
                }
            }
        }
        else if (hold_)
        {
            // Released.
            Set(!on_);
            hold_ = false;
            gate_ = false;
        }
    }
};

class ModCvButtonController
{
private:
    Led* modLed_;
    Led* cvLed_;
    FuncMode funcMode_;

    bool on_;

    bool latched_;
    bool hold_;
    bool pressed_;
    bool trig_;
    bool gate_;

    int samplesSincePressed_;
    int samplesSinceHeld_;

    const int kGateLimit;
public:
    ModCvButtonController(Led* modLed, Led* cvLed, float sampleRate, float blockRate) : kGateLimit(blockRate * kGateLimitSeconds)
    {
        modLed_ = modLed;
        cvLed_ = cvLed;

        funcMode_ = FuncMode::FUNC_MODE_NONE;
        
        on_ = false;
        latched_ = false;
        hold_ = false;
        pressed_ = false;
        trig_ = false;
        gate_ = false;

        samplesSincePressed_ = 0;
        samplesSinceHeld_ = 0;
    }
    ~ModCvButtonController() {}

    static ModCvButtonController* create(Led* modLed, Led* cvLed, float sampleRate, float blockRate)
    {
        return new ModCvButtonController(modLed, cvLed, sampleRate, blockRate);
    }

    static void destroy(ModCvButtonController* obj)
    {
        delete obj;
    }

    inline bool IsOn()
    {
        return on_;
    }

    // True when the button is pressed, false when released.
    inline bool IsPressed()
    {
        return pressed_;
    }

    // True when the button is being kept pressed for some time, false when released.
    inline bool IsHeld()
    {
        return hold_;
    }

    // True when the button is being kept pressed for some time, false after a little more.
    inline bool IsGate()
    {
        return gate_;
    }

    inline void Set(bool on)
    {
        on_ = on;

        if (FuncMode::FUNC_MODE_CV == funcMode_)
        {
            modLed_->Set(0);
            cvLed_->Set(on_);
        }
        else
        {
            modLed_->Set(on_);
            cvLed_->Set(0);
        }
    }

    inline void SetFuncMode(FuncMode funcMode)
    {
        // Set func mode only if the led button isn't pressed.
        if (!pressed_)
        {
            funcMode_ = funcMode;
            hold_ = false;
            trig_ = false;
            Set(on_);
        }
    }

    inline void Trig(bool pressed)
    {
        pressed_ = pressed;

        // Act only when the led button is pressed.
        if (pressed_)
        {
            Set(!on_);
            samplesSincePressed_ = 0;
            samplesSinceHeld_ = 0;
        }
    }

    // Called at block rate
    inline void Process()
    {
        if (!on_)
        {
            return;
        }

        if (pressed_)
        {
            if (hold_)
            {
                if (samplesSinceHeld_ < kGateLimit)
                {
                    // Holding.
                    samplesSinceHeld_++;
                }
                else
                {
                    gate_ = false;
                }
            }
            else
            {
                if (samplesSincePressed_ < kGateLimit)
                {
                    // Holding.
                    samplesSincePressed_++;
                }
                else
                {
                    hold_ = true;
                    gate_ = true;
                    samplesSinceHeld_ = 0;
                }
            }
        }
        else if (hold_)
        {
            // Released.
            Set(!on_);
            hold_ = false;
            gate_ = false;
        }
    }
};