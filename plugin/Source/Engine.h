// Vireo DSP engine. A sample-by-sample port of the Web Audio engine in index.html,
// kept close enough that the presets sound the same in both.
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>
#include "Params.h"

namespace vireo {

constexpr int MAXV = 16, MAXU = 7;
constexpr double TWO_PI = 6.283185307179586;
inline float mtof (float n) { return 440.f * std::pow (2.f, (n - 69.f) / 12.f); }
inline float cents (float c) { return std::exp2 (c / 1200.f); }

// same xorshift as the page, so the reverb impulse and noise match
struct Rng {
    uint32_t s;
    explicit Rng (uint32_t seed = 7) : s (seed ? seed : 1) {}
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) ((double) s / 4294967296.0); }
};

// ---------------------------------------------------------------- wavetables
enum Wave { SAW, SQUARE, TRI, SINE, PULSE, ORGAN, VOX, HOLLOW, GLASS, REED };
struct Tables {
    static constexpr int SZ = 2048, LV = 7; // harmonic caps 63, 32, 16, 8, 4, 2, 1
    std::vector<float> t[5][LV];
    static float amp (int k, int n) {
        auto sq = [] (double x) { return x * x; };
        switch (k) {
            case 0: { const int h[] = { 1, 2, 3, 4, 6, 8, 10, 12 }; const float a[] = { 1, .8f, .6f, .5f, .35f, .3f, .15f, .12f };
                      for (int i = 0; i < 8; ++i) if (h[i] == n) return a[i]; return 0; }
            case 1: return (float) (std::exp (-sq ((n - 5) / 1.8)) + .7 * std::exp (-sq ((n - 9) / 2.0)) + .3 * std::exp (-sq ((n - 20) / 3.0)) + .35 / n);
            case 2: return n % 2 ? 1.f / n : .1f / n;
            case 3: { const int h[] = { 1, 4, 9, 16, 25, 36 }; const float a[] = { 1, .6f, .4f, .25f, .12f, .06f };
                      for (int i = 0; i < 6; ++i) if (h[i] == n) return a[i]; return 0; }
            default: return (float) ((n % 2 ? 1.0 : .5) / std::pow (n, .8) * (n < 24 ? 1.0 : .3));
        }
    }
    void build() {
        for (int k = 0; k < 5; ++k) {
            float norm = 1;
            for (int lv = 0; lv < LV; ++lv) {
                int cap = lv == 0 ? 63 : (64 >> lv);
                auto& v = t[k][lv]; v.assign (SZ + 1, 0.f);
                for (int i = 0; i <= SZ; ++i) {
                    double x = (double) i / SZ, s = 0;
                    for (int n = 1; n <= cap; ++n) { float a = amp (k, n); if (a != 0) s += a * std::sin (TWO_PI * n * x); }
                    v[(size_t) i] = (float) s;
                }
                if (lv == 0) { float m = 0; for (float y : v) m = std::max (m, std::abs (y)); norm = m > 0 ? 1.f / m : 1.f; }
                for (auto& y : v) y *= norm;
            }
        }
    }
    float read (int k, float f, float sr, double ph) const {
        int maxH = (int) (sr * .5f / std::max (f, 1.f)), cap = 63, lv = 0;
        while (lv < LV - 1 && cap > maxH) { cap = 64 >> (lv + 1); ++lv; }
        const auto& v = t[k][lv];
        double x = ph * SZ; int i = (int) x; float fr = (float) (x - i);
        return v[(size_t) i] + (v[(size_t) i + 1] - v[(size_t) i]) * fr;
    }
};

inline float blep (double t, double dt) {
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.f;
}
inline float oscSample (int w, double ph, double dt, float pw, const Tables& T, float f, float sr) {
    switch (w) {
        case SAW: return (float) (1.0 - 2.0 * ph) + blep (ph, dt);
        case SQUARE: return (ph < .5 ? 1.f : -1.f) + blep (ph, dt) - blep (std::fmod (ph + .5, 1.0), dt);
        case TRI: return (float) (ph < .25 ? 4 * ph : ph < .75 ? 2 - 4 * ph : 4 * ph - 4);
        case SINE: return (float) std::sin (TWO_PI * ph);
        case PULSE: return (ph < pw ? 1.f : -1.f) + blep (ph, dt) - blep (std::fmod (ph + 1.0 - pw, 1.0), dt) - (2 * pw - 1);
        default: return T.read (w - ORGAN, f, sr, ph);
    }
}

// ---------------------------------------------------------------- building blocks
struct Env {
    int st = 0; float v = 0, inc = 0, dc = 0, rc = 0, s = 0;
    void set (float a, float d, float sus, float r, float sr, float relDiv) {
        inc = 1.f / std::max (1.f, a * sr);
        dc = 1.f - std::exp (-1.f / ((d / 3.f + 1e-4f) * sr));
        rc = 1.f - std::exp (-1.f / ((r / relDiv + 1e-4f) * sr));
        s = sus;
    }
    void gate (bool retrig) { st = 1; if (! retrig) v = 0; }
    void release() { st = 3; }
    float tick() {
        if (st == 1) { v += inc; if (v >= 1.f) { v = 1.f; st = 2; } }
        else if (st == 2) v += (s - v) * dc;
        else if (st == 3) v -= v * rc;
        return v;
    }
};

struct SVF { // Cytomic trapezoidal state variable filter
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set (float fc, float q, float sr) {
        float g = std::tan (juce::MathConstants<float>::pi * fc / sr);
        k = 1.f / q; a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    float process (float x, int mode) { // 0 lp, 1 hp, 2 bp, 3 notch
        float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        switch (mode) { case 0: return v2; case 1: return x - k * v1 - v2; case 2: return k * v1; default: return x - k * v1; } // band-pass normalised to 0 dB at the centre, like a Web Audio biquad
    }
    void reset() { ic1 = ic2 = 0; }
};

struct Delay {
    std::vector<float> b; int w = 0;
    void init (int n) { b.assign ((size_t) n, 0.f); w = 0; }
    void write (float x) { b[(size_t) w] = x; if (++w >= (int) b.size()) w = 0; }
    float read (float d) const { // d in samples, linear interpolation
        float r = (float) w - d; while (r < 0) r += (float) b.size();
        int i = (int) r; float f = r - i; int j = i + 1 >= (int) b.size() ? 0 : i + 1;
        return b[(size_t) i] + (b[(size_t) j] - b[(size_t) i]) * f;
    }
};

// ---------------------------------------------------------------- voice
struct Voice {
    bool active = false, released = false, arp = false;
    int note = 60; float vel = .8f; uint64_t age = 0; int offIn = -1;
    double ph[2][MAXU] {}, subPh = 0, fmPh = 0;
    float det[2][MAXU] {}, gl[2][MAXU] {}, gr[2][MAXU] {};
    float curF = 440, tgtF = 440, fmDev = 0, fmStart = 0, pe = 0, nlp = 0, f0 = 1000; int fcnt = 0;
    Env amp, flt; SVF f[2][2];
};

// ---------------------------------------------------------------- engine
class Engine {
public:
    float p[vp::NUM] {};
    float bend = 0, mw = 0; bool sustain = false; double hostBpm = 0;

    void prepare (double sampleRate, int block) {
        sr = (float) sampleRate; T.build();
        for (auto& c : ch) c.init ((int) (sr * .1f) + 4);
        dl[0].init ((int) (sr * 3.f)); dl[1].init ((int) (sr * 3.f));
        noise.resize ((size_t) sr * 2); Rng r (99); for (auto& x : noise) x = r.next() * 2 - 1;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) block, 2 };
        conv.prepare (spec); comp.prepare (spec);
        comp.setThreshold (-8.f); comp.setRatio (8.f); comp.setAttack (4.f); comp.setRelease (200.f);
        send.setSize (2, block); dry.setSize (2, block);
        irSize = -1;
    }
    // choice parameters hold an index, these map it to the option's value
    int ix (int id) const { return (int) std::lround (p[id]); }
    float cv (int id) const { return vp::specs[id].values[juce::jlimit (0, vp::specs[id].n - 1, ix (id))]; }
    float bpmNow() const { return hostBpm > 20 ? (float) hostBpm : p[vp::bpm]; }
    int activeCount() const { int c = 0; for (auto& v : voices) if (v.active && ! v.released) ++c; return c; }

    // the reverb impulse is rebuilt off the audio thread when size or damping changes
    bool irNeedsBuild() const { return std::abs (p[vp::rv_size] - irSize) > 1e-4f || std::abs (p[vp::rv_damp] - irDamp) > 1e-4f; }
    void buildIR() {
        irSize = p[vp::rv_size]; irDamp = p[vp::rv_damp];
        int len = (int) (sr * std::min (10.f, irSize));
        juce::AudioBuffer<float> b (2, len); Rng r (1234);
        for (int c = 0; c < 2; ++c) {
            float lp = 0; auto* d = b.getWritePointer (c);
            for (int i = 0; i < len; ++i) {
                float t = i / sr, env = std::pow (1.f - (float) i / len, 2.2f) * std::exp (-t * 1.6f / irSize);
                float a = std::min (.97f, .15f + irDamp * .8f * ((float) i / len));
                lp = lp * a + (r.next() * 2 - 1) * (1 - a);
                d[i] = lp * env * (i < sr * .012f ? i / (sr * .012f) : 1.f);
            }
        }
        conv.loadImpulseResponse (std::move (b), sr, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    }

    // ---------------- notes
    void noteOn (int n, float v) {
        if (ix (vp::arp_on)) {
            if (std::find (held.begin(), held.end(), n) == held.end()) { held.push_back (n); std::sort (held.begin(), held.end()); }
            if (! arpRun) { arpRun = true; arpCount = 0; arpIdx = 0; arpVel = v; }
            return;
        }
        sustained.erase (std::remove (sustained.begin(), sustained.end(), n), sustained.end());
        if (ix (vp::v_mode) != 0) { monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), n), monoStack.end()); monoStack.push_back (n); }
        voiceOn (n, v, false);
    }
    void noteOff (int n) {
        if (ix (vp::arp_on)) { held.erase (std::remove (held.begin(), held.end(), n), held.end()); if (held.empty()) arpRun = false; return; }
        if (sustain) { sustained.push_back (n); return; }
        if (ix (vp::v_mode) != 0) {
            monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), n), monoStack.end());
            Voice* v = mono; if (! v || v->note != n) return;
            if (! monoStack.empty()) { voiceOn (monoStack.back(), v->vel, false); return; }
            release (*v, false); return;
        }
        for (auto& v : voices) if (v.active && ! v.released && ! v.arp && v.note == n) release (v, false);
    }
    void setSustain (bool on) { sustain = on; if (! on) { auto s = sustained; sustained.clear(); for (int n : s) noteOff (n); } }
    void allOff() { for (auto& v : voices) if (v.active) release (v, true); held.clear(); monoStack.clear(); sustained.clear(); arpRun = false; mono = nullptr; }

    // ---------------- audio
    void render (float* L, float* R, int n) {
        const float dtBpm = bpmNow();
        refreshCommon();
        for (int i = 0; i < n; ++i) {
            // arpeggiator steps land on exact samples
            if (arpRun && ix (vp::arp_on)) {
                if (arpCount <= 0) {
                    std::vector<int> seq = arpNotes();
                    if (! seq.empty()) {
                        int step = std::max (1, (int) (cv (vp::arp_div) * 60.f / dtBpm * sr));
                        int note = ix (vp::arp_mode) == 3 ? seq[(size_t) (rng.next() * seq.size()) % seq.size()] : seq[(size_t) (arpIdx % (int) seq.size())];
                        ++arpIdx;
                        Voice* v = voiceOn (note, arpVel, true);
                        v->arp = true; v->offIn = std::max (1, (int) (step * p[vp::arp_gate]));
                        arpCount = step;
                    }
                }
                --arpCount;
            }
            float l = 0, r = 0;
            tickLfos();
            for (auto& v : voices) if (v.active) renderVoice (v, l, r);
            L[i] = l * p[vp::trim]; R[i] = r * p[vp::trim];
        }
        fx (L, R, n);
    }

private:
    float sr = 48000; Tables T; Rng rng { 7 };
    std::array<Voice, MAXV> voices {}; Voice* mono = nullptr; uint64_t ageC = 0; int lastNote = -1;
    std::vector<int> held, monoStack, sustained; bool arpRun = false; int arpCount = 0, arpIdx = 0; float arpVel = .8f;
    std::vector<float> noise; size_t nIdx = 0;
    // lfos
    double lph[2] {}; float lsh[2] {}, lval[2] {};
    // per block derived
    float pitchC = 0, cutC = 0, trem = 1, panv = 0, peakBase = .22f;
    // fx
    Delay ch[2], dl[2]; double chPh[2] {}; SVF toneF[2]; float irSize = -1, irDamp = -1;
    juce::dsp::Convolution conv; juce::dsp::Compressor<float> comp;
    juce::AudioBuffer<float> send, dry;

    std::vector<int> arpNotes() const {
        std::vector<int> out;
        for (int o = 0; o < (int) cv (vp::arp_oct); ++o) for (int n : held) out.push_back (n + 12 * o);
        int m = ix (vp::arp_mode);
        if (m == 1) std::reverse (out.begin(), out.end());
        if (m == 2 && out.size() > 2) { for (int i = (int) out.size() - 2; i >= 1; --i) out.push_back (out[(size_t) i]); }
        return out;
    }
    float baseCut (int n, float vel) const {
        return p[vp::f_cut] * std::exp2 (p[vp::f_key] * (n - 60) / 12.f) * std::exp2 (p[vp::f_vel] * (vel - 1) * 3.f);
    }
    void envOn (Voice& v, float vel, bool retrig) {
        v.vel = vel;
        v.amp.set (p[vp::ae_a], p[vp::ae_d], p[vp::ae_s], p[vp::ae_r], sr, 4.f);
        v.flt.set (p[vp::fe_a], p[vp::fe_d], p[vp::fe_s], p[vp::fe_r], sr, 3.f);
        v.amp.gate (retrig); v.flt.gate (retrig);
        v.f0 = baseCut (v.note, vel);
    }
    Voice* voiceOn (int n, float vel, bool forcePoly) {
        const bool monoMode = ! forcePoly && ix (vp::v_mode) != 0;
        const float f = mtof ((float) n);
        if (monoMode && mono && mono->active && ! mono->released) {
            mono->tgtF = f; if (p[vp::v_glide] < .002f) mono->curF = f;
            mono->note = n; if (ix (vp::v_mode) == 1) envOn (*mono, vel, true);
            lastNote = n; return mono;
        }
        int maxV = monoMode ? 1 : juce::jlimit (1, MAXV, (int) p[vp::v_voices]);
        int activeCount = 0; for (auto& v : voices) if (v.active && ! v.released) ++activeCount;
        while (activeCount >= maxV) { Voice* old = nullptr; for (auto& v : voices) if (v.active && ! v.released && (! old || v.age < old->age)) old = &v; if (! old) break; release (*old, true); --activeCount; }
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (! v) { for (auto& x : voices) if (! v || x.age < v->age) v = &x; }
        *v = Voice(); v->active = true; v->note = n; v->age = ++ageC;
        v->tgtF = f; v->curF = (monoMode && lastNote >= 0 && p[vp::v_glide] > .002f) ? mtof ((float) lastNote) : f;
        for (int o = 0; o < 2; ++o) {
            int N = (int) p[o ? vp::o2_uni : vp::o1_uni]; N = juce::jlimit (1, MAXU, N);
            float dt = p[o ? vp::o2_det : vp::o1_det], sp = p[o ? vp::o2_spr : vp::o1_spr];
            for (int u = 0; u < N; ++u) {
                float s = N == 1 ? 0.f : ((float) u / (N - 1) - .5f) * 2.f;
                v->det[o][u] = s * dt; float x = (s * sp + 1.f) * .5f;
                v->gl[o][u] = std::cos (x * juce::MathConstants<float>::halfPi) / std::sqrt ((float) N);
                v->gr[o][u] = std::sin (x * juce::MathConstants<float>::halfPi) / std::sqrt ((float) N);
                v->ph[o][u] = N > 1 ? rng.next() : 0.0;
            }
        }
        if (p[vp::fm_amt] > 0) v->fmDev = f * cv (vp::fm_ratio) * p[vp::fm_amt] * p[vp::fm_amt] * 9.f * (.6f + .4f * vel);
        v->pe = p[vp::pe_amt] * 100.f;
        for (auto& st : v->f) for (auto& fl : st) fl.reset();
        envOn (*v, vel, false);
        if (monoMode) mono = v;
        lastNote = n;
        return v;
    }
    void release (Voice& v, bool fast) {
        if (v.released) return;
        v.released = true;
        if (fast) v.amp.rc = 1.f - std::exp (-1.f / (.003f * sr));
        v.amp.release(); v.flt.release();
        if (mono == &v) mono = nullptr;
    }
    void refreshCommon() {
        const float l2d = ix (vp::mw_dest) == 3 ? p[vp::l2_depth] + mw * (1 - p[vp::l2_depth]) : p[vp::l2_depth];
        depth[0] = p[vp::l1_depth]; depth[1] = l2d;
        peakBase = .22f;
    }
    float depth[2] {};
    void tickLfos() {
        for (int i = 0; i < 2; ++i) {
            const int b = i ? vp::l2_shape : vp::l1_shape;
            lph[i] += p[b + 2] / sr; if (lph[i] >= 1) { lph[i] -= 1; lsh[i] = rng.next() * 2 - 1; }
            double x = lph[i];
            switch (ix (b)) {
                case 0: lval[i] = (float) std::sin (TWO_PI * x); break;
                case 1: lval[i] = (float) (x < .25 ? 4 * x : x < .75 ? 2 - 4 * x : 4 * x - 4); break;
                case 2: lval[i] = (float) (2 * x - 1); break;
                case 3: lval[i] = x < .5 ? 1.f : -1.f; break;
                default: lval[i] = lsh[i];
            }
        }
        pitchC = bend * 100.f * cv (vp::v_bend); cutC = 0; float ampSum = 0, ampMod = 0; panv = 0;
        for (int i = 0; i < 2; ++i) {
            const int dest = ix (i ? vp::l2_dest : vp::l1_dest); const float d = depth[i];
            if (dest == 0) pitchC += lval[i] * d * d * 1200.f;
            else if (dest == 1) cutC += lval[i] * d * 4800.f;
            else if (dest == 2) { ampSum += d; ampMod += lval[i] * d * .5f; }
            else panv += lval[i] * d;
        }
        const int md = ix (vp::mw_dest);
        if (md == 0) pitchC += lval[0] * mw * mw * 60.f;
        else if (md == 1) pitchC += mw * 1200.f;
        else if (md == 2) cutC += mw * 4800.f;
        trem = 1.f - std::min (1.f, ampSum) * .5f + ampMod;
        panv = juce::jlimit (-1.f, 1.f, panv);
    }
    void renderVoice (Voice& v, float& outL, float& outR) {
        if (v.offIn > 0 && --v.offIn == 0) release (v, false);
        const float glide = p[vp::v_glide];
        if (v.curF != v.tgtF) { float c = 1.f - std::exp (-1.f / (std::max (.001f, glide / 3.f) * sr)); v.curF += (v.tgtF - v.curF) * c; if (std::abs (v.curF - v.tgtF) < .01f) v.curF = v.tgtF; }
        if (v.pe != 0) { v.pe *= std::exp (-1.f / (std::max (.001f, p[vp::pe_dec] / 3.f) * sr)); if (std::abs (v.pe) < .01f) v.pe = 0; }
        const float f = v.curF * cents (pitchC + v.pe);
        float fm = 0;
        if (v.fmDev > 0) {
            fm = (float) std::sin (TWO_PI * v.fmPh) * v.fmDev;
            v.fmPh += f * cv (vp::fm_ratio) / sr; if (v.fmPh >= 1) v.fmPh -= std::floor (v.fmPh);
            fmEnv (v);
        }
        float l = 0, r = 0;
        for (int o = 0; o < 2; ++o) {
            const int on = o ? vp::o2_on : vp::o1_on; if (! ix (on)) continue;
            const float lvl = p[on + 5]; if (lvl <= 0) continue;
            const int w = ix (on + 1), N = juce::jlimit (1, MAXU, (int) p[on + 6]);
            const float ratio = std::exp2 (cv (on + 2) + p[on + 3] / 12.f), pw = p[on + 9], fine = p[on + 4];
            const float fo = f * ratio + (o == 0 ? fm : 0.f);
            for (int u = 0; u < N; ++u) {
                const float fu = fo * cents (fine + v.det[o][u]);
                const double dt = std::abs (fu) / sr;
                const float s = oscSample (w, v.ph[o][u], dt, pw, T, fu, sr) * lvl;
                v.ph[o][u] += fu / sr; v.ph[o][u] -= std::floor (v.ph[o][u]);
                l += s * v.gl[o][u]; r += s * v.gr[o][u];
            }
        }
        if (p[vp::sub_lvl] > 0) {
            const float fs = f * std::exp2 (cv (vp::sub_oct));
            const double dt = fs / sr; const bool sq = ix (vp::sub_wave) == 1;
            const float s = (sq ? oscSample (SQUARE, v.subPh, dt, .5f, T, fs, sr) * .5f : (float) std::sin (TWO_PI * v.subPh)) * p[vp::sub_lvl];
            v.subPh += dt; v.subPh -= std::floor (v.subPh); l += s; r += s;
        }
        if (p[vp::noise_lvl] > 0) {
            const float a = std::exp (-TWO_PI_F * p[vp::noise_tone] / sr);
            v.nlp = v.nlp * a + noise[nIdx] * (1 - a); if (++nIdx >= noise.size()) nIdx = 0;
            const float s = v.nlp * p[vp::noise_lvl] * .6f; l += s; r += s;
        }
        // filter: the envelope moves cutoff in octaves, the way the page schedules it
        const float fe = v.flt.tick();
        if ((v.fcnt++ & 7) == 0) {
            float fc = v.f0 * std::exp2 (p[vp::f_env] * 6.f * fe) * cents (cutC);
            fc = juce::jlimit (20.f, sr * .45f, fc);
            const int ty = ix (vp::f_type); const float res = p[vp::f_res];
            float q = (ty <= 2) ? std::pow (10.f, (res * res * 26.f - 1.f) / 20.f) : .6f + res * 18.f;
            q = std::max (.5f, q);
            for (int c = 0; c < 2; ++c) { v.f[0][c].set (fc, q, sr); v.f[1][c].set (fc, .7071f, sr); }
        }
        const int ty = ix (vp::f_type); const int mode = ty <= 1 ? 0 : ty == 2 ? 1 : ty == 3 ? 2 : 3;
        l = v.f[0][0].process (l, mode); r = v.f[0][1].process (r, mode);
        if (ty == 0) { l = v.f[1][0].process (l, 0); r = v.f[1][1].process (r, 0); }
        const float a = v.amp.tick(), pk = peakBase * (1 - p[vp::ae_vel] + p[vp::ae_vel] * v.vel);
        outL += l * a * pk; outR += r * a * pk;
        if (v.released && a < 1e-5f) { v.active = false; }
    }
    static constexpr float TWO_PI_F = 6.2831853f;
    void fmEnv (Voice& v) {
        // index decays towards 8% of its start with the FM decay time, like setTargetAtTime on the page
        if (v.fmStart == 0) v.fmStart = v.fmDev;
        const float target = v.fmStart * .08f, c = 1.f - std::exp (-1.f / (std::max (.001f, p[vp::fm_dec] / 3.f) * sr));
        v.fmDev += (target - v.fmDev) * c;
    }
    void fx (float* L, float* R, int n) {
        // drive
        const float d = p[vp::fx_drive];
        if (d >= .01f) { const float k = 1 + d * 18, nk = 1.f / std::tanh (k), g = 1.f / (1 + d * 1.6f);
            for (int i = 0; i < n; ++i) { L[i] = std::tanh (k * juce::jlimit (-1.f, 1.f, L[i])) * nk * g; R[i] = std::tanh (k * juce::jlimit (-1.f, 1.f, R[i])) * nk * g; } }
        // chorus: two modulated taps panned out
        const float cm = p[vp::ch_mix], crate = p[vp::ch_rate], cdep = p[vp::ch_depth] * .006f * sr;
        const float wet = cm * .8f, dryg = 1 - cm * .35f;
        const float glA = std::cos (.1f * juce::MathConstants<float>::halfPi), grA = std::sin (.1f * juce::MathConstants<float>::halfPi);
        // delay
        const float dsec = std::min (2.9f, cv (vp::dl_div) * 60.f / bpmNow()), dsm = dsec * sr, fb = p[vp::dl_fb], dmix = p[vp::dl_mix];
        toneF[0].set (std::min (p[vp::dl_tone], sr * .45f), .7071f, sr); toneF[1].set (std::min (p[vp::dl_tone], sr * .45f), .7071f, sr);
        send.setSize (2, n, false, false, true); dry.setSize (2, n, false, false, true);
        const float rmix = p[vp::rv_mix];
        for (int i = 0; i < n; ++i) {
            float l = L[i], r = R[i];
            if (cm > 0.001f) {
                const float m = (l + r) * .5f;
                const float t0 = .013f * sr + cdep * (float) std::sin (TWO_PI * chPh[0]), t1 = .019f * sr + cdep * (float) std::sin (TWO_PI * chPh[1]);
                const float a = ch[0].read (t0), b = ch[1].read (t1);
                ch[0].write (m); ch[1].write (m);
                l = l * dryg + (a * glA + b * grA) * wet; r = r * dryg + (a * grA + b * glA) * wet;
            } else { const float m = (l + r) * .5f; ch[0].write (m); ch[1].write (m); }
            chPh[0] += crate / sr; chPh[1] += crate * 1.13f / sr; if (chPh[0] >= 1) chPh[0] -= 1; if (chPh[1] >= 1) chPh[1] -= 1;
            // ping-pong: the send enters the left line, each line feeds the other through the tone filter
            const float yl = dl[0].read (dsm), yr = dl[1].read (dsm);
            dl[0].write ((l + r) * .5f * dmix + toneF[1].process (yr, 0) * fb);
            dl[1].write (toneF[0].process (yl, 0) * fb);
            l += yl * .8f; r += yr * .8f;
            dry.setSample (0, i, l); dry.setSample (1, i, r);
            send.setSample (0, i, l * rmix); send.setSample (1, i, r * rmix);
        }
        juce::dsp::AudioBlock<float> sb (send); conv.process (juce::dsp::ProcessContextReplacing<float> (sb));
        const float vol = p[vp::vol] * p[vp::vol] * 1.4f;
        for (int i = 0; i < n; ++i) {
            float l = dry.getSample (0, i) + send.getSample (0, i), r = dry.getSample (1, i) + send.getSample (1, i);
            l *= trem; r *= trem;
            if (panv != 0) { const float x = panv <= 0 ? panv + 1 : panv, g1 = std::cos (x * juce::MathConstants<float>::halfPi), g2 = std::sin (x * juce::MathConstants<float>::halfPi);
                if (panv <= 0) { const float nl = l + r * g1, nr = r * g2; l = nl; r = nr; } else { const float nl = l * g1, nr = r + l * g2; l = nl; r = nr; } }
            L[i] = l * vol; R[i] = r * vol;
        }
        float* chans[2] = { L, R }; juce::dsp::AudioBlock<float> ob (chans, 2, (size_t) n);
        comp.process (juce::dsp::ProcessContextReplacing<float> (ob));
        // the browser's DynamicsCompressor adds automatic make-up gain; 2.3 dB here lands the presets on the same levels as the page
        const float ct = 1.f / std::tanh (1.2f), mk = 1.3f;
        for (int i = 0; i < n; ++i) { L[i] = std::tanh (juce::jlimit (-1.f, 1.f, L[i] * mk) * 1.2f) * ct; R[i] = std::tanh (juce::jlimit (-1.f, 1.f, R[i] * mk) * 1.2f) * ct; }
    }
};

} // namespace vireo
