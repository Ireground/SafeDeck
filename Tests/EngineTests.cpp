// SafeDeck engine tests. Run on every GitHub build; a failure turns the build red.
#include "../Source/SafeEngine.h"
#include <cstdio>
#include <random>
#include <string>

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("FAIL: %s\n", msg); ++failures; } else std::printf ("ok:   %s\n", msg); } while (0)

using Buf = std::vector<std::vector<float>>;

static Buf noise (int ch, int n, float amp, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-amp, amp);
    Buf b ((size_t) ch, std::vector<float> ((size_t) n));
    for (auto& c : b) for (auto& x : c) x = d (rng);
    return b;
}

// process in random block sizes (like a real host)
static Buf run (safe::Engine& e, Buf in, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_int_distribution<int> bs (1, 700);
    const int n = (int) in[0].size();
    int pos = 0;
    while (pos < n)
    {
        const int len = std::min (n - pos, bs (rng));
        std::vector<float*> p;
        for (auto& c : in) p.push_back (c.data() + pos);
        e.process (p.data(), (int) p.size(), len, 0.0);
        pos += len;
    }
    return in;
}

static bool bitExactDelayed (const Buf& in, const Buf& out, int lat)
{
    for (size_t c = 0; c < in.size(); ++c)
        for (size_t i = (size_t) lat; i < in[c].size(); ++i)
            if (out[c][i] != in[c][i - (size_t) lat]) return false;
    return true;
}

static float peak (const Buf& b, size_t from = 0)
{
    float m = 0.0f;
    for (auto& c : b) for (size_t i = from; i < c.size(); ++i) m = std::max (m, std::fabs (c[i]));
    return m;
}

static bool allFinite (const Buf& b)
{
    for (auto& c : b) for (float x : c) if (! std::isfinite (x)) return false;
    return true;
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        std::printf ("\n--- %.0f Hz ---\n", fs);
        const int n = (int) (fs * 4.0);

        // 1. null test, limiter off, loud-ish signal
        {
            safe::Engine e; e.prepare (fs, 2);
            safe::Params p; p.limiterOn = false; e.setParams (p);
            auto in = noise (2, n, 3.0f, 11);   // up to +9.5 dBFS, under the +18 panic level
            auto out = run (e, in);
            CHECK (bitExactDelayed (in, out, e.latency()), "limiter off: output is bit-identical (null test)");
        }
        // 2. null test, limiter on but nothing reaches the ceiling
        {
            safe::Engine e; e.prepare (fs, 2);
            safe::Params p; e.setParams (p);
            auto in = noise (2, n, 0.5f, 12);
            auto out = run (e, in);
            CHECK (bitExactDelayed (in, out, e.latency()), "limiter on, signal below ceiling: bit-identical (null test)");
        }
        // 3. ceiling is never exceeded
        {
            safe::Engine e; e.prepare (fs, 2);
            safe::Params p; p.ceilingDb = -1.0f; p.truePeak = false; e.setParams (p);
            auto in = noise (2, n, 5.0f, 13);
            for (int i = 0; i < n; ++i) in[0][(size_t) i] += 2.0f * (float) std::sin (i * 0.01);
            auto out = run (e, in);
            CHECK (peak (out) <= safe::Engine::dbToLin (-1.0f) + 1e-6f, "limiter: no sample above the ceiling");
            CHECK (allFinite (out), "limiter: output finite");
        }
        // 4. true peak: fs/4 sine at 45 degrees -> sample peaks 0.707, real peak 1.0
        {
            safe::Engine e; e.prepare (fs, 1);
            safe::Params p; p.ceilingDb = -0.3f; e.setParams (p);
            Buf in (1, std::vector<float> ((size_t) n));
            for (int i = 0; i < n; ++i) in[0][(size_t) i] = 1.2f * (float) std::sin (3.14159265 * 0.5 * i + 0.785398);
            auto out = run (e, in);
            // reconstruct between samples with a long sinc to measure the real peak
            float tp = 0.0f;
            const auto& y = out[0];
            for (size_t i = (size_t) fs; i + 40 < y.size(); i += 7)
                for (double f : { 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875 })
                {
                    double s = 0.0;
                    for (int k = -32; k <= 32; ++k)
                    {
                        const double u = f - k;
                        const double w = 0.5 * (1.0 + std::cos (3.14159265 * u / 33.0));
                        s += y[(size_t) ((long) i + k)] * std::sin (3.14159265 * u) / (3.14159265 * u) * w;
                    }
                    tp = std::max (tp, (float) std::fabs (s));
                }
            const float tpDb = 20.0f * std::log10 (tp);
            std::printf ("      true peak measured: %.2f dBTP (ceiling -0.3)\n", tpDb);
            CHECK (tpDb <= 0.0f, "true peak mode keeps inter-sample peaks under 0 dBTP");
        }
        // 5. NaN in the middle: output stays finite, mutes, then comes back exactly
        {
            safe::Engine e; e.prepare (fs, 2);
            safe::Params p; p.limiterOn = false; e.setParams (p);
            auto in = noise (2, n, 0.8f, 14);
            const size_t bad = (size_t) fs / 2;
            in[1][bad] = std::nanf ("");
            in[0][bad + 10] = INFINITY;
            auto out = run (e, in);
            CHECK (allFinite (out), "NaN/inf in: output always finite");
            CHECK (out[0][bad + (size_t) e.latency()] == 0.0f, "NaN/inf in: muted when the bad sample arrives");
            CHECK (e.nanTotal.load() == 2 && e.eventCount.load() == 1, "NaN/inf in: logged as one event");
            // after hold (1 s) + fade (1 s) the audio is back, bit-exact
            const size_t back = bad + (size_t) (fs * 2.05) + (size_t) e.latency();
            Buf tailIn (2), tailOut (2);
            for (int c = 0; c < 2; ++c)
            {
                tailIn[(size_t) c].assign (in[(size_t) c].begin() + (long) back - e.latency(), in[(size_t) c].end());
                tailOut[(size_t) c].assign (out[(size_t) c].begin() + (long) back, out[(size_t) c].end());
            }
            bool same = true;
            for (int c = 0; c < 2; ++c) for (size_t i = 0; i < tailOut[(size_t) c].size(); ++i)
                if (tailOut[(size_t) c][i] != tailIn[(size_t) c][i]) same = false;
            CHECK (same, "NaN/inf in: fades back in and is bit-identical again");
            // no click: the fade-out is gradual
            float maxJump = 0.0f;
            for (size_t i = bad + 1; i < bad + (size_t) e.latency() + 2; ++i)
                maxJump = std::max (maxJump, std::fabs (out[0][i] - out[0][i - 1]));
            CHECK (maxJump < 1.7f, "NaN/inf in: no jump bigger than the signal itself");
        }
        // 6. panic level: +40 dB spike never reaches the output
        {
            safe::Engine e; e.prepare (fs, 2);
            safe::Params p; p.limiterOn = false; p.panicDb = 18.0f; e.setParams (p);
            auto in = noise (2, n, 0.5f, 15);
            for (size_t i = (size_t) fs; i < (size_t) fs + 2000; ++i) in[0][i] = 100.0f;
            auto out = run (e, in);
            CHECK (peak (out) <= 0.5f + 1e-6f, "over +18 dB: blast blocked, output never louder than the music");
            CHECK (e.overTotal.load() == 2000 && e.eventCount.load() == 1, "over +18 dB: logged as one event");
        }
        // 7. denormal / silence / extreme values don't break anything
        {
            safe::Engine e; e.prepare (fs, 2);
            safe::Params p; e.setParams (p);
            Buf in (2, std::vector<float> ((size_t) n, 1e-40f));
            in[0][100] = -1e38f; in[1][200] = 1e38f;
            auto out = run (e, in);
            CHECK (allFinite (out), "extreme values: output finite");
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
