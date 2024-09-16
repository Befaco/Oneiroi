#pragma once

#include "Patch.h"
#include "ParameterInterpolator.h"
#include "TapTempo.h"
#include <stdlib.h>
#include <stdint.h>
#include <cmath>

//#define USE_RECORD_THRESHOLD
#define MAX_PATCH_SETTINGS 16 // Max number of available MIDI channels
#define PATCH_SETTINGS_NAME "oneiroi"
#define PATCH_VERSION "v1.0"

// Taken from pichenettes' stmlib.
#define CONSTRAIN(var, min, max) \
  if (var < (min)) { \
    var = (min); \
  } else if (var > (max)) { \
    var = (max); \
  }
#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);
#define SLOPE(out, in, positive, negative)                \
    {                                                     \
        float error = (in)-out;                           \
        out += (error > 0 ? positive : negative) * error; \
    }

#define SHIFT_BUTTON PUSHBUTTON
#define LEFT_ARROW_PARAM GREEN_BUTTON
#define RIGHT_ARROW_PARAM RED_BUTTON
#define RECORD_BUTTON BUTTON_1
#define RECORD_IN BUTTON_2
#define RANDOM_BUTTON BUTTON_3
#define RANDOM_IN BUTTON_4
#define SYNC_IN BUTTON_5
#define INPUT_PEAK_LED_PARAM BUTTON_6
#define PREPOST_SWITCH BUTTON_7
#define SSWT_SWITCH BUTTON_8
#define MOD_CV_GREEN_LED_PARAM BUTTON_9
#define MOD_CV_RED_LED_PARAM BUTTON_10
#define MOD_CV_BUTTON BUTTON_11

#define INPUT_LED_PARAM PARAMETER_AF
#define MOD_LED_PARAM PARAMETER_AG

constexpr float kEqualCrossFadeP = 1.f;
constexpr float kEps = 0.0001f; // Commodity for minimum float
constexpr float kPi = 3.1415927410125732421875f;
const float k2Pi = kPi * 2.0f;
const float kHPi = kPi * 0.5f;
constexpr float kSqrt2 = 1.414213562373095f;
const float kRSqrt2 = 1.f / kSqrt2;
constexpr float kOne = 0.975f; //4095.f / 4096.f;
constexpr float k2One = kOne * 2;
static const float kOneHalf = kOne / 2.f;

constexpr float kCvLpCoeff = 0.7f;
constexpr float kCvOffset = -0.46035f;
constexpr float kCvMult = 1.485f;
constexpr float kCvDelta = 0.02f;
constexpr float kCvMinThreshold = 0.f;

constexpr int kRandomSlewSamples = 128;

constexpr float kA4Freq = 440.f;
constexpr int kA4Note = 69;
constexpr float kSemi4Oct = 12;
constexpr float kOscFreqMin = 16.35f; // C0
constexpr float kOscFreqMax = 8219.f; // C9

constexpr float kLooperLoopLengthMinSeconds = 1.0 / 130.813f; // C3 = 130.81Hz
constexpr float kLooperFade = 1. / 100; // 10ms @ audio rate
static const float kLooperFadeTwice = kLooperFade * 2;
static const int32_t kLooperTotalBufferLength = 1 << 19; // 524288 samples for both channels (interleaved) = 5.46 seconds stereo buffer
static const int32_t kLooperChannelBufferLength = kLooperTotalBufferLength / 2;
// static const float kLooperFadeInc = 1.f / kLooperFadeSamples;
constexpr float kLooperMakeupGain = 1.f;
constexpr int kLooperClearBlocks = 128; // Number of blocks of the buffer to be cleared
static const int32_t kLooperClearBlockSize = kLooperTotalBufferLength / kLooperClearBlocks;
static const int32_t kLooperClearBlockTypeSize = kLooperClearBlockSize * 4; // Float

constexpr float kRecordOnsetLevel = 0.005f;
constexpr float kRecordWindupLevel = 0.00001f;
constexpr int kRecordGateLimit = 375; // 250ms (1500 = 1s @ block rate)
constexpr int kRecordOnsetLimit = 375; // 250ms (1500 = 1s @ block rate)

constexpr int kWaveTableLength = 2048;
constexpr int kWaveTableNofTables = 32;
static const int kWaveTableBufferLength = kWaveTableLength * kWaveTableNofTables * 2;
constexpr int kWaveTableClearBlocks = 128; // Number of blocks of the buffer to be cleared
static const int32_t kWaveTableClearBlockSize = kWaveTableBufferLength / kWaveTableClearBlocks;
static const int32_t kWaveTableClearBlockTypeSize = kWaveTableClearBlockSize * 4; // Float

// When internally clocked, base frequency is ~0.18Hz
// When externally clocked, min bpm is 30 (0.5Hz), max is 300 (5Hz)
constexpr float kClockFreqMin = 0.01f;
constexpr float kClockFreqMax = 80.f;
constexpr float kExternalClockLimitSeconds = 2.f; // Samples required to detect a steady external clock - 2s (1500 = 1s @ block rate)
static const float kInternalClockFreq = (44100.f / kLooperChannelBufferLength);
constexpr int kClockNofRatios = 17;
constexpr int kClockUnityRatio = 9;
static const float kModClockRatios[kClockNofRatios] = { 0.015625f, 0.03125f, 0.0625f, 0.125f, 0.2f, 0.25f, 0.33f, 0.5f, 1, 2, 3, 4, 5, 8, 16, 32, 64};
static const float kRModClockRatios[kClockNofRatios] = { 64, 32, 16, 8, 5, 4, 3, 2, 1, 0.5f, 0.33f, 0.25f, 0.2f, 0.125f, 0.0625f, 0.03125f, 0.015625f};

constexpr float kOScSineGain = 0.3f;
static const float kOscSineFadeInc = 1.f / 128;
constexpr float kOScSuperSawGain = 0.65f;
constexpr float kOScWaveTableGain = 0.6f;
constexpr float kSourcesMakeupGain = 0.15f;

constexpr float kFilterFreqMin = 10.f;
constexpr float kFilterFreqMax = 22000.f;
constexpr float kFilterMakeupGain = 2.4f;
constexpr float kFilterChaosNoise = 1.8f;
constexpr float kFilterLpGainMin = 0.3f;
constexpr float kFilterLpGainMax = 0.4f;
constexpr float kFilterHpGainMin = 0.2f;
constexpr float kFilterHpGainMax = 0.4f;
constexpr float kFilterBpGainMin = 0.4f;
constexpr float kFilterBpGainMax = 1.6f;
constexpr float kFilterCombGainMin = 0.1f;
constexpr float kFilterCombGainMax = 0.2f;

constexpr float kResoGainMin = 0.7f;
constexpr float kResoGainMax = 0.9f;
constexpr float kResoMakeupGain = 1.f;
constexpr int32_t kResoBufferSize = 2400;
constexpr float kResoInfiniteFeedbackThreshold = 0.999f;
constexpr float kResoInfiniteFeedbackLevel = 1.001f;

constexpr float kEchoMinLength = 1.f / 100.f; // 10 ms @ audio rate
constexpr float kEchoMaxLength = 4.f; // 4 seconds @ audio rate
constexpr int kEchoTaps = 4;
const float kEchoTapsRatios[kEchoTaps] = { 0.75f, 0.25f, 0.375f, 1.f };  // TAP_LEFT_A (1/2 dot), TAP_LEFT_B (1/8), TAP_RIGHT_A (1/8 dot), TAP_RIGHT_B (1)
const float kEchoTapsFeedbacks[kEchoTaps] = { 0.35f, 0.65f, 0.55f, 0.45f };
const float kEchoMaxExternalClock = kEchoMaxLength / kModClockRatios[kClockNofRatios - 1]; // Maximum period for the external clock
constexpr int kEchoExternalClockMultiplier = 32;
constexpr int kEchoInternalClockMultiplier = 23; // ~192000 / 8192 (period of the buffer)
constexpr float kEchoMakeupGainMin = 0.7f;
constexpr float kEchoMakeupGainMax = 0.9f;

constexpr int32_t kAmbienceLengthSeconds = 1.f;
constexpr int kAmbienceNofDiffusers = 4;
constexpr float kAmbienceLowDampMin = -0.5f;
constexpr float kAmbienceLowDampMax = -40.f;
constexpr float kAmbienceHighDampMin = -0.5f;
constexpr float kAmbienceHighDampMax = -40.f;
constexpr float kAmbienceGainMid = 0.4f;
constexpr float kAmbienceGainMin = 1.f;
constexpr float kAmbienceGainMax = 0.5f;
constexpr float kAmbienceMakeupGain = 1.4f;

static const float kOutputFadeInc = 1.f / 16.f;
constexpr float kOutputMakeupGain = 5.f;
constexpr float kResampleGain = 2.f;

constexpr float kParamCatchUpDelta = 0.005f;

constexpr int kParamStartMovementLimit = 75; // Samples required to detect the start of a movement - 50ms (1500 = 1s @ block rate)
constexpr int kParamStopMovementLimit = 225; // Samples required to detect the stop of a movement - 150ms (1500 = 1s @ block rate)
constexpr int kResetLimit = 75; // Samples waited for both RECORD and RANDOM buttons to be pressed for resetting parameters - 50ms (1500 = 1s @ block rate)
constexpr int kSaveLimit = 3000; // Samples waited for MOD/CV button to be pressed for saving parameters - 2s (1500 = 1s @ block rate)
constexpr int kGateLimit = 750; // Samples waited for a button gate to go off - 500ms (1500 = 1s @ block rate)
constexpr int kHoldLimit = 75; // Samples waited for a pressed button to be considered held - 50ms (1500 = 1s @ block rate)

struct PatchCtrls
{
    float inputVol;

    float looperVol;
    float looperSos;
    float looperFilter;
    float looperSpeed;
    float looperSpeedModAmount;
    float looperSpeedCvAmount;
    float looperStart;
    float looperStartModAmount;
    float looperStartCvAmount;
    float looperLength;
    float looperLengthModAmount;
    float looperLengthCvAmount;
    float looperRecording;
    float looperResampling;

    float osc1Vol;
    float osc2Vol;
    float oscOctave;
    float oscUnison;
    float oscPitch;
    float oscPitchModAmount;
    float oscPitchCvAmount;
    float oscDetune;
    float oscDetuneModAmount;
    float oscDetuneCvAmount;
    float oscUseWavetable;

    float filterVol;
    float filterMode;
    float filterNoiseLevel;
    float filterCutoff;
    float filterCutoffModAmount;
    float filterCutoffCvAmount;
    float filterResonance;
    float filterResonanceModAmount;
    float filterResonanceCvAmount;
    float filterPosition;

    float resonatorVol;
    float resonatorTune;
    float resonatorTuneModAmount;
    float resonatorTuneCvAmount;
    float resonatorFeedback;
    float resonatorFeedbackModAmount;
    float resonatorFeedbackCvAmount;
    float resonatorDissonance;

    float echoVol;
    float echoRepeats;
    float echoRepeatsModAmount;
    float echoRepeatsCvAmount;
    float echoDensity;
    float echoDensityModAmount;
    float echoDensityCvAmount;
    float echoFilter;

    float ambienceVol;
    float ambienceDecay;
    float ambienceDecayModAmount;
    float ambienceDecayCvAmount;
    float ambienceSpacetime;
    float ambienceSpacetimeModAmount;
    float ambienceSpacetimeCvAmount;
    float ambienceAutoPan;

    float modLevel;
    float modSpeed;
    float modType;

    float randomMode;
    float randomAmount;
};

struct PatchCvs
{
    float looperSpeed;
    float looperStart;
    float looperLength;
    float oscPitch;
    float oscDetune;
    float filterCutoff;
    float filterResonance;
    float resonatorTune;
    float resonatorFeedback;
    float echoRepeats;
    float echoDensity;
    float ambienceDecay;
    float ambienceSpacetime;
};

enum FuncMode
{
    FUNC_MODE_NONE,
    FUNC_MODE_ALT,
    FUNC_MODE_MOD,
    FUNC_MODE_CV,
    FUNC_MODE_LAST
};

enum ClockSource
{
    CLOCK_SOURCE_INTERNAL,
    CLOCK_SOURCE_EXTERNAL,
};

struct PatchState
{
    float sampleRate;
    float blockRate;
    int blockSize;

    FloatArray inputLevel;
    FloatArray efModLevel;

    bool modActive;
    float modValue;
    float modValueRaw;

    ClockSource clockSource;
    TapTempo* tempo;

    bool syncIn;
    bool clockReset;
    bool clockTick;

    bool clearLooperFlag;
    bool oscPitchCenterFlag;
    bool oscUnisonCenterFlag;
    bool oscOctaveFlag;
    bool looperSpeedLockFlag;
    bool modTypeLockFlag;
    bool modSpeedLockFlag;
    bool filterModeFlag;
    bool filterPositionFlag;

    float c5;
    float pitchZero;
    float speedZero;

    float outLevel;
    float randomSlew;

    bool randomHasSlew;
    bool softTakeover;
    bool modAttenuverters;
    bool cvAttenuverters;

    FuncMode funcMode;
};

inline bool AreEquals(float val1, float val2, float d = kEps)
{
    return fabs(val1 - val2) <= d;
}

/**
 * @brief Taken from DaisySP.
 *
 * @param a
 * @param b
 * @return float
 */
inline float Max(float a, float b)
{
    float r;
#ifdef __arm__
    asm("vmaxnm.f32 %[d], %[n], %[m]" : [d] "=t"(r) : [n] "t"(a), [m] "t"(b) :);
#else
    r = (a > b) ? a : b;
#endif // __arm__
    return r;
}

/**
 * @brief Taken from DaisySP.
 *
 * @param a
 * @param b
 * @return float
 */
inline float Min(float a, float b)
{
    float r;
#ifdef __arm__
    asm("vminnm.f32 %[d], %[n], %[m]" : [d] "=t"(r) : [n] "t"(a), [m] "t"(b) :);
#else
    r = (a < b) ? a : b;
#endif // __arm__
    return r;
}

/**
 * @brief Taken from DaisySP.
 *
 * @param in
 * @param min
 * @param max
 * @return float
 */
inline float Clamp(float in, float min = 0.f, float max = 1.f)
{
    return Min(Max(in, min), max);
}

/**
 * @brief Taken from DaisySP.
 *
 * @param Base
 * @param Exponent
 * @return float
 */
float Power(float f, float n = 2, bool approx = false)
{
    if (approx)
    {
        long *lp, l;
        lp = (long *)(&f);
        l  = *lp;
        l -= 0x3F800000;
        l <<= ((int) n - 1);
        l += 0x3F800000;
        *lp = l;

        return f;
    }

    return pow(f, n);
}

/**
 * @brief Frequency to period in samples conversion.
 *
 * @param freq Frequency in Hz
 * @return float Samples
 */
inline float F2S(float freq, float sampleRate)
{
    return freq == 0.f ? 0.f : sampleRate / freq;
}

/**
 * @brief MIDI note to frequency conversion.
 * @note Taken from DaisySP.
 *
 * @param m MIDI note
 * @return float Frequency in Hz
 */
inline float M2F(float m)
{
    return Power(2.f, (m - kA4Note) / kSemi4Oct) * kA4Freq;
}

/**
 * @brief MIDI note to delay time conversion.
 *
 * @param note MIDI note
 * @return float Delay time in samples
 */
inline float M2D(float note, float sampleRate)
{
    return F2S(M2F(note), sampleRate);
}

/**
 * @brief Keeps the value between 0 and size by wrapping it back from the
 *        opposite boundary if necessary.
 *
 * @param val
 * @param size
 * @return float
 */
inline float Wrap(float val, float size = 1.f)
{
    while (val < 0)
    {
        val += size;
    }
    while (val > size)
    {
        val -= size;
    }

    return val;
}

enum RandomAmount
{
    RANDOM_MID,
    RANDOM_LOW,
    RANDOM_HIGH,
    RANDOM_CUSTOM,
};

inline float RandomFloat(float min = 0.f, float max = kOne)
{
    return min == max ? min : min + randf() * (max - min);
}

/**
 * @brief Maps the value that ranges from aMin to aMax to a value that
 *        ranges from bMin to bMax. Supports inverted ranges.
 */
inline float Map(float value, float aMin, float aMax, float bMin, float bMax)
{
    float k = abs(bMax - bMin) / abs(aMax - aMin) * (bMax > bMin ? 1 : -1);

    return bMin + k * (value - aMin);
}

inline float MapExpo(float value, float aMin = 0.f, float aMax = 1.f, float bMin = 0.f, float bMax = 1.f)
{
    value = (value - aMin) / (aMax - aMin);

    return bMin + (value * value) * (bMax - bMin);
}

inline float MapLog(float value, float aMin = 0.f, float aMax = 1.f, float bMin = 0.f, float bMax = 1.f)
{
    bMin = fast_logf(bMin < 0.0000001f ? 0.0000001f : bMin);
    bMax = fast_logf(bMax);

    return fast_expf(Map(value, aMin, aMax, bMin, bMax));
}

// Maps the value to a range by considering where the center actually is.
inline float CenterMap(float value, float min = -1.f, float max = 1.f, float center = 0.5f)
{
    if (value < center)
    {
        value = Map(value, 0.0f, center, min, 0.0f);
    }
    else
    {
        value = Map(value, center, 1.f, 0.0f, max);
    }

    return value;
}

/**
 * @brief Quantizes in steps a value between 0.f and 1.f, returning a value between 0.f and 1.f
 *
 * @param value
 * @param steps
 * @return float
 */
inline float Quantize(float value, int32_t steps)
{
    return rintf(value * steps) * (1.f / steps);
}

/**
 * @brief Quantizes in steps a value between 0.f and 1.f, returning a value between 0 and steps - 1
 *
 * @param value
 * @param steps
 * @return int
 */
inline int QuantizeInt(float value, int32_t steps)
{
    return rintf(Clamp(value, 0, 1) * (steps - 1));
}

// (a + b) * 1 / sqrt(2)
inline float Mix2(float a, float b)
{
    return (a + b) * 0.707f;
}
inline void Mix2(FloatArray a, FloatArray b, FloatArray o)
{
    o.copyFrom(a);
    o.add(b);
    o.multiply(0.707f);
}
inline void Mix2(AudioBuffer& a, AudioBuffer& b, AudioBuffer& o)
{
    o.copyFrom(a);
    o.add(b);
    o.multiply(0.707f);
}

// (a + b + c) * 1 / sqrt(3)
inline float Mix3(float a, float b, float c)
{
    return (a + b + c) * 0.577f;
}

// (a + b + c + d) * 1 / sqrt(4)
inline float Mix4(float a, float b, float c, float d)
{
    return (a + b + c + d) * 0.5f;
}

inline float Db2A(float db)
{
    return Power(10.f, db / 20.f);
}

inline float LinearCrossFade(float a, float b, float pos)
{
    return a * (1.f - pos) + b * pos;
}
inline void LinearCrossFade(FloatArray a, FloatArray b, FloatArray o, float pos)
{
    a.multiply(1.f - pos);
    b.multiply(pos);
    o.copyFrom(a);
    o.add(b);
}
inline void LinearCrossFade(AudioBuffer& a, AudioBuffer& b, AudioBuffer& o, float pos)
{
    AudioBuffer* t = AudioBuffer::create(2, o.getSize());

    for (size_t i = 0; i < 2; ++i)
    {
        a.getSamples(i).multiply(1.f - pos, o.getSamples(i));
        b.getSamples(i).multiply(pos, t->getSamples(i));
    }
    o.add(*t);

    AudioBuffer::destroy(t);
}

inline float VariableCrossFade(float a, float b, float pos, float length = 1.f, float offset = 0.f)
{
    if (offset > 0)
    {
        length = Min(length, 1.f - offset);
    }
    if (pos >= offset && pos <= offset + length)
    {
        float l = (pos - offset) * (1.f / length);

        return a * (1.f - l) + b * l;
    }
    if (pos > length + offset)
    {
        return b;
    }

    return a;
}

/**
 * @brief Energy preserving crossfade
 * @see https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/
 *
 * @param from
 * @param to
 * @param pos
 * @return float
 */
float CheapEqualPowerCrossFade(float from, float to, float pos, float p = kEqualCrossFadeP)
{
    float invPos = 1.f - pos;
    float k = -6.0026608f + p * (6.8773512f - 1.5838104f * p);
    float a = pos * invPos;
    float b = a * (1.f + k * a);
    float c = (b + pos);
    float d = (b + invPos);

    return from * d * d + to * c * c;
}
void CheapEqualPowerCrossFade(AudioBuffer &from, AudioBuffer &to, float pos, AudioBuffer &out, float p = kEqualCrossFadeP)
{
    float invPos = 1.f - pos;
    float k = -6.0026608f + p * (6.8773512f - 1.5838104f * p);
    float a = pos * invPos;
    float b = a * (1.f + k * a);
    float c = (b + pos);
    float d = (b + invPos);

    from.multiply(d * d);
    to.multiply(c * c);

    out.copyFrom(from);
    out.add(to);
}


inline void LR2MS(const float left, const float right, float &mid, float &side, float width = 1.f)
{
    mid = (left + right) / kSqrt2;
    side = ((left - right) / kSqrt2) * width;
}

inline void MS2LR(const float mid, const float side, float &left, float &right)
{
    left = (mid + side) / kSqrt2;
    right = (mid - side) / kSqrt2;
}

inline void StereoControl(float &left, float &right, float width)
{
    float mid, side;
    LR2MS(left, right, mid, side, width);
    MS2LR(mid, side, left, right);
}

/**
 * @brief Generates a sampled period of a square wave.
 *
 * @param tab Table to be filled
 * @param sz Table size
 * @param p Period size
 * @param a Amplification
 * @param d Duty cycle (0/1.f)
 * @param o Phase offset
 */
void SquareTable(float *tab, uint32_t sz, uint32_t p = 0, float a = 1.f, float d = 0.5f, uint32_t o = 0)
{
    if (sz <= 0) return;
    if (p <= 0)
    {
        p = sz;
    }

    uint32_t h = rintf(p * d);
    for (uint32_t i = 0; i < sz; i++)
    {
        tab[i] = ((i + o) % p) < h  ? a : -a;
    }
}

float SoftLimit(float x)
{
  return x * (27.f + x * x) / (27.f + 9.f * x * x);
}

float SoftClip(float x)
{
  if (x <= -3.f)
  {
    return -1.f;
  }
  else if (x >= 3.f)
  {
    return 1.f;
  }
  else
  {
    return SoftLimit(x);
  }
}

float HardClip(float x)
{
    return Clamp(x, -1.f, 1.f);
}

float AudioClip(float x, float s = 1.f)
{
    return SoftClip(x * s);
}

// Taken and adapted from stmlib
class HysteresisQuantizer
{
public:
    HysteresisQuantizer() { }
    ~HysteresisQuantizer() { }

    void Init(int num_steps, float hysteresis, bool symmetric)
    {
        num_steps_ = num_steps;
        hysteresis_ = hysteresis;

        scale_ = static_cast<float>(symmetric ? num_steps - 1 : num_steps);
        offset_ = symmetric ? 0.0f : -0.5f;

        quantized_value_ = 0;
    }

    inline int Process(float value)
    {
        return Process(0, value);
    }

    inline int Process(int base, float value)
    {
        value *= scale_;
        value += offset_;
        value += static_cast<float>(base);

        float hysteresis_sign = value > static_cast<float>(quantized_value_)
            ? -1.0f
            : +1.0f;
        int q = static_cast<int>(value + hysteresis_sign * hysteresis_ + 0.5f);
        CONSTRAIN(q, 0, num_steps_ - 1);
        quantized_value_ = q;

        return q;
    }

    template<typename T>
    const T& Lookup(const T* array, float value)
    {
        return array[Process(value)];
    }

    inline int num_steps() const
    {
        return num_steps_;
    }

    inline int quantized_value() const
    {
        return quantized_value_;
    }

private:
    int num_steps_;
    float hysteresis_;

    float scale_;
    float offset_;

    int quantized_value_;
};

template<typename T, int size>
class Lut
{
public:
    enum Type
    {
        LUT_TYPE_EXPO,
        LUT_TYPE_LINEAR,
    };

private:
    T lut_[size];
    T min_;
    T max_;
    Type type_;
    float expoY_ = 3;
    HysteresisQuantizer quantizer_;

    void Linear()
    {
        float d = (1.f / (size - 1));
        for (size_t i = 0; i < size; i++)
        {
            float x = d * i;
            lut_[i] = min_ + (max_ - min_) * x;
        }
    }

    void Expo()
    {
        float d = (1.f / (size - 1));
        for (size_t i = 0; i < size; i++)
        {
            float x = d * i;
            lut_[i] = min_ + (max_ - min_) * (x == 1 ? 1 : fast_powf(x, expoY_));
        }
    }

public:
    Lut(T min, T max, Type type = LUT_TYPE_LINEAR) : min_{min}, max_{max}, type_{type}
    {
        quantizer_.Init(size, 0.25f, false);

        if (LUT_TYPE_EXPO == type)
        {
            Expo();
        }
        else if (LUT_TYPE_LINEAR == type)
        {
            Linear();
        }
    }
    ~Lut() {}

    void SetExpo(float y)
    {
        expoY_ = y;
    }

    T GetValue(int pos)
    {
        return lut_[pos];
    }

    T Quantized(float pos)
    {
        return quantizer_.Lookup(lut_, pos);
    }
};

// These is taken and adapted from code found in Emilie Gillet's eurorack repo.
float Modulate(
    float baseValue,
    float modAmount,
    float modValue,
    float cvAmount = 0,
    float cvValue = 0,
    float minValue = -1.f,
    float maxValue = 1.f
) {
    // Reduce noise when there's nothing connected to the CV.
    if (cvValue >= -kCvMinThreshold && cvValue <= kCvMinThreshold )
    {
        cvValue = kCvMinThreshold;
    }
    baseValue += modAmount * modValue + cvAmount * cvValue;
    CONSTRAIN(baseValue, minValue, maxValue);

    return baseValue;
}

float Attenuate(
    float baseValue,
    float modAmount,
    float modValue,
    float minValue = 0.f,
    float maxValue = 1.f
) {
    baseValue *= (1.f - modAmount + modAmount * modValue);
    CONSTRAIN(baseValue, minValue, maxValue);

    return baseValue;
}