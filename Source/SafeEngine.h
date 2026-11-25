#pragma once
// SafeDeck engine: plain C++ (no JUCE), so it can be unit-tested on its own.
//
//  1. Guard   - NaN / infinity / anything over the panic threshold is replaced by
//               silence before it reaches the delay line. The audio before it is
//               faded out over the lookahead time, so the mute never clicks.
//               After `holdSec` of clean signal the output fades back in.
//  2. Limiter - lookahead brickwall limiter (optional true-peak detection).
//               When nothing goes over the ceiling its gain is exactly 1.0,
//               so the output is bit-for-bit the (delayed) input.
//
// Latency is constant (lookahead + true-peak centre) whether the limiter is on or
// off, so toggling never shifts the timing of the mix.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace safe
{
struct Params
{
    bool limiterOn = true;
    float ceilingDb = -0.3f;
    float releaseMs = 100.0f;
    bool truePeak = true;
    float panicDb = 18.0f;
};

enum EventType { evNone = 0, evNaN = 1, evOver = 2 };
enum State { stOk = 0, stFadeOut = 1, stMuted = 2, stFadeIn = 3 };

class Engine
{
public:
    static constexpr int tpCentre = 4;          // true-peak interpolator delay (8 taps)
    static constexpr double lookaheadSec = 0.002;
    static constexpr double holdSec = 1.0;      // clean signal needed before fading back in
    static constexpr double fadeInSec = 1.0;
    static constexpr int maxEvents = 8;

    Engine() { makeTaps(); }

    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        nch = std::max (1, numChannels);
        L = std::max (1, (int) std::lround (lookaheadSec * sr));
        lat = L + tpCentre;
        dsize = lat + 1;
        delay.assign ((size_t) nch, std::vector<float> ((size_t) dsize, 0.0f));
        hist.assign ((size_t) nch, std::array<float, 8> {});
        dqVal.assign ((size_t) L + 2, 1.0f);
        dqIdx.assign ((size_t) L + 2, 0);
        box.assign ((size_t) L, 1.0f);
        holdSamples = (int64_t) (holdSec * sr);
        fadeInStep = (float) (1.0 / (fadeInSec * sr));
        reset();
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& h : hist) h.fill (0.0f);
        wpos = 0;
        dqHead = dqTail = 0; dqCount = 0; frame = 0;
        std::fill (box.begin(), box.end(), 1.0f);
        bpos = 0; boxSum = (double) L;
        env = 1.0f;
        mute = 1.0f; state = stOk; cleanCount = 0;
    }

    int latency() const noexcept { return lat; }
    int lookahead() const noexcept { return L; }

    void setParams (const Params& p)
    {
        limiterOn = p.limiterOn;
        truePeak = p.truePeak;
        ceilLin = dbToLin (std::clamp (p.ceilingDb, -24.0f, 0.0f));
        panicLin = dbToLin (std::clamp (p.panicDb, -6.0f, 48.0f));
        const double rel = std::clamp ((double) p.releaseMs, 1.0, 5000.0) * 0.001;
        relCoef = (float) (1.0 - std::exp (-1.0 / (rel * sr)));
    }

    // ch: channel pointers (in place). numCh may differ from prepare(); extra channels are left alone.
    // blockTimeSec: host time of the first sample (for the event log).
    void process (float* const* ch, int numCh, int n, double blockTimeSec = 0.0)
    {
        const int C = std::min (numCh, nch);
        float inPk = 0.0f, outPk = 0.0f, gMin = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            // ---- 1. guard: sanitise input ----
            int bad = evNone;
            for (int c = 0; c < C; ++c)
            {
                float x = ch[c][i];
                if (! std::isfinite (x)) { bad = evNaN; x = 0.0f; }
                else
                {
                    const float a = std::fabs (x);
                    if (a > panicLin) { if (bad == evNone) bad = evOver; x = 0.0f; }
                    else inPk = std::max (inPk, a);
                }
                auto& h = hist[(size_t) c];
                std::copy (h.begin() + 1, h.end(), h.begin());
                h[7] = x;
                delay[(size_t) c][(size_t) wpos] = x;
            }
            if (bad != evNone) onBad (bad, blockTimeSec + i / sr);
            else ++cleanCount;

            // ---- 2. limiter gain ----
            float r = 1.0f;
            if (limiterOn)
            {
                float det = 0.0f;
                for (int c = 0; c < C; ++c)
                {
                    const auto& h = hist[(size_t) c];
                    det = std::max (det, std::fabs (h[3]));
                    if (truePeak)
                        for (int f = 0; f < 3; ++f)
                        {
                            float y = 0.0f;
                            for (int j = 0; j < 8; ++j) y += h[(size_t) j] * taps[(size_t) f][(size_t) j];
                            det = std::max (det, std::fabs (y));
                        }
                }
                if (det > ceilLin) r = ceilLin / det;
            }

            // sliding minimum of r over the last L+1 detections (monotonic queue)
            while (dqCount > 0 && dqVal[(size_t) back()] >= r) { dqTail = (dqTail + (int) dqVal.size() - 1) % (int) dqVal.size(); --dqCount; }
            dqVal[(size_t) dqTail] = r; dqIdx[(size_t) dqTail] = frame;
            dqTail = (dqTail + 1) % (int) dqVal.size(); ++dqCount;
            while (dqIdx[(size_t) dqHead] < frame - L) { dqHead = (dqHead + 1) % (int) dqVal.size(); --dqCount; }
            const float hmin = dqVal[(size_t) dqHead];
            ++frame;

            // instant attack, smooth release (never above the held minimum)
            if (hmin < env) env = hmin;
            else
            {
                env += (1.0f - env) * relCoef;
                if (env > 0.999999f) env = 1.0f;
                env = std::min (env, hmin);
            }

            // boxcar over L samples: gain ramps down in time for the peak
            boxSum += (double) env - (double) box[(size_t) bpos];
            box[(size_t) bpos] = env;
            if (++bpos == L) bpos = 0;
            float g = (float) (boxSum / (double) L);
            if (g > 0.999999f) g = 1.0f;
            g = std::clamp (g, 0.0f, 1.0f);
            gMin = std::min (gMin, g);

            // ---- 3. mute envelope ----
            switch (state)
            {
                case stFadeOut: mute -= fadeOutStep; if (mute <= 0.0f) { mute = 0.0f; state = stMuted; } break;
                case stMuted:   if (cleanCount >= holdSamples) state = stFadeIn; break;
                case stFadeIn:  mute += fadeInStep; if (mute >= 1.0f) { mute = 1.0f; state = stOk; } break;
                default: break;
            }

            // ---- 4. output (oldest sample in the delay line) ----
            const int rpos = (wpos + 1) % dsize;
            const float gain = g * mute;
            for (int c = 0; c < C; ++c)
            {
                float y = delay[(size_t) c][(size_t) rpos];
                if (gain != 1.0f) y *= gain;
                if (limiterOn) y = std::clamp (y, -ceilLin, ceilLin);
                ch[c][i] = y;
                outPk = std::max (outPk, std::fabs (y));
            }
            wpos = rpos;
        }

        // telemetry for the GUI
        inPeak.store (std::max (inPeak.load (std::memory_order_relaxed), inPk), std::memory_order_relaxed);
        outPeak.store (std::max (outPeak.load (std::memory_order_relaxed), outPk), std::memory_order_relaxed);
        minGain.store (std::min (minGain.load (std::memory_order_relaxed), gMin), std::memory_order_relaxed);
        curState.store (state, std::memory_order_relaxed);
    }

    // Host bypass: keep the same latency, touch nothing else.
    void processBypassed (float* const* ch, int numCh, int n)
    {
        const int C = std::min (numCh, nch);
        for (int i = 0; i < n; ++i)
        {
            const int rpos = (wpos + 1) % dsize;
            for (int c = 0; c < C; ++c)
            {
                float x = ch[c][i];
                delay[(size_t) c][(size_t) wpos] = std::isfinite (x) ? x : 0.0f;
                ch[c][i] = delay[(size_t) c][(size_t) rpos];
            }
            wpos = rpos;
        }
    }

    // ---- GUI side (any thread): read-and-reset meters ----
    float takeInPeak() { return inPeak.exchange (0.0f); }
    float takeOutPeak() { return outPeak.exchange (0.0f); }
    float takeMinGain() { return minGain.exchange (1.0f); }
    int getState() const { return curState.load(); }

    struct Event { std::atomic<int> type { 0 }; std::atomic<double> time { 0.0 }; };
    std::array<Event, maxEvents> events;
    std::atomic<uint32_t> eventCount { 0 };
    std::atomic<uint32_t> nanTotal { 0 }, overTotal { 0 };

    static float dbToLin (float db) { return std::pow (10.0f, db / 20.0f); }

private:
    void onBad (int type, double t)
    {
        cleanCount = 0;
        if (state == stOk || state == stFadeIn)
        {
            state = stFadeOut;
            fadeOutStep = mute / (float) lat;   // reaches 0 exactly when the bad sample comes out
            if (fadeOutStep <= 0.0f) { mute = 0.0f; state = stMuted; }
            const uint32_t k = eventCount.load (std::memory_order_relaxed);
            events[(size_t) (k % maxEvents)].type.store (type, std::memory_order_relaxed);
            events[(size_t) (k % maxEvents)].time.store (t, std::memory_order_relaxed);
            eventCount.store (k + 1, std::memory_order_release);
        }
        (type == evNaN ? nanTotal : overTotal).fetch_add (1, std::memory_order_relaxed);
    }

    int back() const { return (dqTail + (int) dqVal.size() - 1) % (int) dqVal.size(); }

    void makeTaps()
    {
        // 8-tap windowed-sinc interpolator at 1/4, 1/2, 3/4 between hist[3] and hist[4]
        const double pi = 3.14159265358979323846;
        for (int f = 0; f < 3; ++f)
        {
            const double frac = 0.25 * (f + 1);
            double sum = 0.0;
            for (int j = 0; j < 8; ++j)
            {
                const double u = 3.0 + frac - j;
                const double sinc = std::sin (pi * u) / (pi * u);
                const double w = 0.5 * (1.0 + std::cos (pi * u / 4.0));
                taps[(size_t) f][(size_t) j] = (float) (sinc * w);
                sum += sinc * w;
            }
            for (int j = 0; j < 8; ++j) taps[(size_t) f][(size_t) j] = (float) (taps[(size_t) f][(size_t) j] / sum);
        }
    }

    double sr = 44100.0;
    int nch = 2, L = 88, lat = 92, dsize = 93, wpos = 0;
    std::vector<std::vector<float>> delay;
    std::vector<std::array<float, 8>> hist;
    std::array<std::array<float, 8>, 3> taps {};

    std::vector<float> dqVal; std::vector<int64_t> dqIdx;
    int dqHead = 0, dqTail = 0, dqCount = 0; int64_t frame = 0;
    std::vector<float> box; int bpos = 0; double boxSum = 0.0;
    float env = 1.0f;

    bool limiterOn = true, truePeak = true;
    float ceilLin = 0.966f, panicLin = 7.94f, relCoef = 0.0002f;

    float mute = 1.0f, fadeOutStep = 0.0f, fadeInStep = 0.0f;
    int state = stOk;
    int64_t cleanCount = 0, holdSamples = 44100;

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, minGain { 1.0f };
    std::atomic<int> curState { stOk };
};
} // namespace safe
