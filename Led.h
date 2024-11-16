#pragma once

#include "Commons.h"

static const int BLINK_LIMIT = 75; // 50ms (1500 = 1s)

enum LedName
{
    LED_INPUT,
    LED_INPUT_PEAK,
    LED_SYNC,
    LED_MOD,
    LED_RECORD,
    LED_RANDOM,
    LED_ARROW_LEFT,
    LED_ARROW_RIGHT,
    LED_SHIFT,
    LED_MOD_AMOUNT,
    LED_CV_AMOUNT,
    LED_LAST
};

enum LedType
{
    LED_TYPE_BUTTON,
    LED_TYPE_PARAM,
};

class Led
{
private:
    Patch* patch_;

    TGate trigger_;
    LedType type_;

    int id_;
    int blinks_;
    int samplesBetweenBlinks_;

    float value_;

    bool trig_;
    bool doBlink_;
    bool fast_;

    inline void SetLed(float value)
    {
        if (LedType::LED_TYPE_BUTTON == type_)
        {
            patch_->setButton(PatchButtonId(id_), value);
        }
        else
        {
            patch_->setParameterValue(PatchParameterId(id_), value);
        }
    }

public:
    Led(int id, LedType type)
    {
        id_ = id;
        type_ = type;
        value_ = 0;
        trig_ = false;
        doBlink_ = false;
        trigger_.Init(48000);
        samplesBetweenBlinks_ = 0;
    }

    ~Led() {}

    static Led* create(int id, LedType type = LedType::LED_TYPE_BUTTON)
    {
        return new Led(id, type);
    }

    static void destroy(Led* obj)
    {
        delete obj;
    }

    inline void Set(float value)
    {
        value_ = value;

        SetLed(value_);
    }

    inline void On()
    {
        Set(1);
    }

    inline void Off()
    {
        Set(0);
    }

    inline void Toggle()
    {
        Set(value_ == 0 ? 1.f : 1.f - value_);
    }

    inline bool IsOn()
    {
        return value_;
    }

    inline bool IsBlinking()
    {
        return blinks_ > 0;
    }

    /**
     * @brief Blink the led.
     *
     * @param blinks number of blinks or -1 for continuous blinking or 0 for stopping
     * @param fast
     * @param initial the initial state of the led
     */
    inline void Blink(int blinks = 1, bool fast = false, bool initial = true)
    {
        if (blinks == -1 && blinks_ == -1)
        {
            return;
        }

        blinks_ = blinks;

        if (blinks == 0)
        {
            // Stop blinking.
            Off();
            doBlink_ = false;
            trig_ = false;

            return;
        }

        Set(initial);
        trig_ = true;
        doBlink_ = true;
        fast_ = fast;
        if (fast_ && blinks > 0)
        {
            trigger_.SetDuration(0.002f / blinks_);
        }
    }

    // Called at block rate
    inline void Read()
    {
        if (doBlink_)
        {
            if (!trigger_.Process(trig_))
            {
                Toggle();
                doBlink_ = false;
                // Decrement the number of blinks remaining,
                // unless it's -1 (continuous blinking).
                if (blinks_ > 0)
                {
                    blinks_--;
                    if (blinks_ == 0)
                    {
                        Off();
                    }
                }
                samplesBetweenBlinks_ = 0;
            }
            trig_ = false;
        }

        if (!doBlink_ && blinks_ != 0)
        {
            // If we must blink another time, wait a bit before the next one.
            if (samplesBetweenBlinks_ < (fast_ ? 75 : 150))
            {
                samplesBetweenBlinks_++;
            }
            else
            {
                Toggle();
                doBlink_ = true;
                trig_ = true;
            }
        }
    }
};
