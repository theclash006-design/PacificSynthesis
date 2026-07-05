#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

// ─────────────────────────────────────────────────────────────
//  PolyBLEP — Band-Limited Step
//  oscillator の不連続点 (saw のジャンプ、square のエッジ) に
//  この補正を加えると高周波エイリアシングが大幅に減る。
//  t  : 現在の phase (0〜1)
//  dt : 1サンプル分の phase 進行量 (= freq / sampleRate)
// ─────────────────────────────────────────────────────────────
inline float polyBLEP (float t, float dt) noexcept
{
    if (t < dt)
    {
        const float x = t / dt;
        return x + x - x * x - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        const float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

// ─────────────────────────────────────────────────────────────
//   BassVoice — TB-303 風の 4 voice ポリフォニック
//   - 内部に 4 ボイス保持、osc + amp env + slide はボイス毎
//   - ladder filter / filter env / LFO / drive / sub level は共有
//   - mono 互換: slide=true (内蔵シーケンサの連続音) は常に voice 0 へ
//     → 1 ノートのみの時は出力 bit 同一 (既存音質に影響なし)
//   - poly: slide=false の新ノートは未使用ボイス、なければ最古ボイスを steal
// ─────────────────────────────────────────────────────────────
class BassVoice
{
public:
    static constexpr int kMaxVoices = 4;

    void prepare (double sr, int samplesPerBlock)
    {
        sampleRate = sr;
        ladder.reset();
        juce::dsp::ProcessSpec spec {
            sr,
            (juce::uint32) juce::jmax (1, samplesPerBlock),
            (juce::uint32) 1
        };
        ladder.prepare (spec);
        ladder.setMode (juce::dsp::LadderFilterMode::LPF24);
        ladder.setEnabled (true);
        filterEnv = 0.0f;
        for (auto& v : voices) v = {};
    }

    void setWaveform (int w) noexcept       { waveform = (w == 1) ? 1 : 0; }
    void setCutoff (float hz) noexcept      { cutoffBase = hz; }
    void setResonance (float r) noexcept    { resonance = juce::jlimit (0.0f, 0.99f, r * 0.1f); ladder.setResonance (resonance); }
    void setEnvMod (float hz) noexcept      { envModAmount = hz; }
    void setDecay (float s) noexcept        { decaySeconds = juce::jmax (0.02f, s); recomputeCoeffs(); }
    void setAccentAmount (float a) noexcept { accentAmount = juce::jlimit (0.0f, 1.0f, a); }
    void setGlideTime (float s) noexcept    { glideTau = juce::jmax (0.001f, s); }
    void setDrive (float d) noexcept        { drive = juce::jlimit (0.0f, 1.0f, d); }
    void setSubLevel (float l) noexcept     { subLevel = juce::jlimit (0.0f, 1.0f, l); }

    // ─ LFO (sin) ─
    void setLfoRate   (float hz)   noexcept { lfoRate   = juce::jlimit (0.05f, 20.0f, hz); }
    void setLfoDepth  (float d)    noexcept { lfoDepth  = juce::jlimit (0.0f, 1.0f, d); }
    void setLfoTarget (int target) noexcept { lfoTarget = juce::jlimit (0, 1, target); }

    void noteOn (int midiNote, float velocity, bool accent, bool slide)
    {
        Voice* target = allocateVoice (midiNote, slide);
        const float accBoost = accent ? (1.0f + accentAmount) : 1.0f;
        const float newFreq  = midiToHz (midiNote);

        // Slide が成立する条件: 要求された + そのボイスが既に発音中
        const bool doSlide = slide && target->isPlaying && target->ampEnv > 1.0e-4f;
        if (! doSlide)
        {
            target->freq  = newFreq;
            target->phase = 0.0f;
        }
        target->targetFreq = newFreq;
        target->slidingTo  = doSlide ? newFreq : target->freq;
        target->sliding    = doSlide;
        target->midiNote   = midiNote;
        target->ampEnv     = juce::jlimit (0.0f, 1.0f, velocity) * accBoost * 0.9f;
        target->isPlaying  = true;

        // フィルタ env は共有、毎ノートトリガーで再起動
        filterEnv = 1.0f * accBoost;
        recomputeCoeffs();
    }

    void noteOff (int midiNote)
    {
        // 自然 decay に任せる (sustain がないので自動的に消える)
        (void) midiNote;
    }

    void render (float* buf, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
            buf[i] = renderSample();
    }

    bool isActive() const noexcept
    {
        for (const auto& v : voices)
            if (v.ampEnv > 1.0e-4f) return true;
        return false;
    }

private:
    // ─ Voice struct: per-voice oscillator + amp envelope state ─
    struct Voice
    {
        bool   isPlaying = false;
        int    midiNote  = -1;
        float  freq      = 110.0f;
        float  targetFreq = 110.0f;
        float  slidingTo = 110.0f;
        bool   sliding   = false;
        float  phase     = 0.0f;
        float  subPhase  = 0.0f;
        float  ampEnv    = 0.0f;
    };

    // mono 互換: slide=true は常に voice 0 へ → 既存挙動と同じ
    // poly: 未使用 voice、なければ最も amp env が低い (最古) を steal
    Voice* allocateVoice (int midiNote, bool slide)
    {
        if (slide)
            return &voices[0];

        // 同じ MIDI ノートが鳴ってる → 再発音 (retrigger)
        for (auto& v : voices)
            if (v.isPlaying && v.midiNote == midiNote)
                return &v;

        // 未使用 voice
        for (auto& v : voices)
            if (! v.isPlaying || v.ampEnv < 1.0e-4f)
                return &v;

        // 全部使用中 → 最も amp env が低い (最古) を steal
        Voice* oldest = &voices[0];
        for (auto& v : voices)
            if (v.ampEnv < oldest->ampEnv) oldest = &v;
        return oldest;
    }

    float renderSample()
    {
        // 全ボイス inactive なら早期終了 (mono 時の旧挙動と同じ、LFO 進行も止める)
        bool anyActive = false;
        for (const auto& v : voices)
            if (v.isPlaying && v.ampEnv > 1.0e-5f) { anyActive = true; break; }
        if (! anyActive)
        {
            for (auto& v : voices) { v.isPlaying = false; v.ampEnv = 0.0f; }
            return 0.0f;
        }

        // ─ LFO 更新 (共有)
        lfoPhase += lfoRate / (float) sampleRate;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        const float lfoVal = std::sin (lfoPhase * juce::MathConstants<float>::twoPi);

        // Pitch LFO ratio (target=1)
        float pitchModRatio = 1.0f;
        if (lfoTarget == 1 && lfoDepth > 0.001f)
            pitchModRatio = std::exp2 (lfoVal * lfoDepth * (1.0f / 12.0f));

        // ─ 各ボイスの osc を sum (filter は共有 1 個に通す)
        float oscSum = 0.0f;
        for (auto& v : voices)
        {
            if (! v.isPlaying || v.ampEnv < 1.0e-5f)
            {
                v.isPlaying = false;
                continue;
            }

            // Slide
            if (v.sliding)
            {
                const float a = 1.0f - std::exp (-1.0f / (glideTau * (float) sampleRate));
                v.freq += (v.slidingTo - v.freq) * a;
                if (std::abs (v.slidingTo - v.freq) < 0.01f) v.sliding = false;
            }

            // Oscillator + PolyBLEP
            const float oscFreq = v.freq * pitchModRatio;
            const float dt = oscFreq / (float) sampleRate;
            v.phase += dt;
            if (v.phase >= 1.0f) v.phase -= 1.0f;

            float vOsc = 0.0f;
            if (waveform == 0)
            {
                vOsc = v.phase * 2.0f - 1.0f;
                vOsc -= polyBLEP (v.phase, dt);
            }
            else
            {
                vOsc = (v.phase < 0.5f) ? 1.0f : -1.0f;
                vOsc += polyBLEP (v.phase, dt);
                float p2 = v.phase + 0.5f;
                if (p2 >= 1.0f) p2 -= 1.0f;
                vOsc -= polyBLEP (p2, dt);
            }

            // Sub osc (1 oct 下、pitch LFO 追従)
            if (subLevel > 0.001f)
            {
                v.subPhase += (oscFreq * 0.5f) / (float) sampleRate;
                if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
                vOsc = vOsc * (1.0f - subLevel * 0.5f)
                     + std::sin (v.subPhase * juce::MathConstants<float>::twoPi) * subLevel * 0.85f;
            }

            // ボイス毎の amp envelope
            v.ampEnv *= ampDecayCoeff;
            oscSum += vOsc * v.ampEnv;
        }

        // ─ 共有フィルタ envelope
        filterEnv *= filterDecayCoeff;

        // ─ Cutoff 計算 (envMod + LFO Cutoff)
        constexpr float baseFreq = 200.0f;
        const float octaves    = envModAmount * 0.001f;
        const float envWithSus = filterEnv * 0.9f + 0.1f;
        const float modCutoff  = baseFreq * std::exp2 (octaves * envWithSus);
        float cutoffNow = juce::jlimit (40.0f, 18000.0f,
                                        std::min (modCutoff, cutoffBase));
        if (lfoTarget == 0 && lfoDepth > 0.001f)
            cutoffNow = juce::jlimit (40.0f, 18000.0f,
                                      cutoffNow * std::exp2 (lfoVal * lfoDepth * 2.0f));
        ladder.setCutoffFrequencyHz (cutoffNow);

        // ─ ラダーフィルタを sum で通す
        float sampleBuf[1] = { oscSum };
        float* channelPtrs[1] = { sampleBuf };
        juce::dsp::AudioBlock<float> block (channelPtrs, 1, 1);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        ladder.process (ctx);
        float out = sampleBuf[0];

        // ─ Drive (post-filter)
        if (drive > 0.001f)
            out = std::tanh (out * (1.0f + drive * 8.0f)) / std::tanh (1.0f + drive * 8.0f);

        return out;
    }

    void recomputeCoeffs() noexcept
    {
        const float tau = decaySeconds;
        ampDecayCoeff    = std::exp (-1.0f / (tau * (float) sampleRate));
        filterDecayCoeff = std::exp (-1.0f / (tau * (float) sampleRate));
    }

    static float midiToHz (int n) { return 440.0f * std::pow (2.0f, (n - 69) / 12.0f); }

    // ─ Voice array
    std::array<Voice, kMaxVoices> voices;

    // ─ 共有ステート
    double sampleRate = 44100.0;
    int    waveform = 0;
    float  filterEnv = 0.0f;
    float  ampDecayCoeff = 0.0f;
    float  filterDecayCoeff = 0.0f;

    // Params
    float  cutoffBase = 800.0f;
    float  resonance = 0.7f;
    float  envModAmount = 3000.0f;
    float  decaySeconds = 0.3f;
    float  accentAmount = 0.5f;
    float  glideTau = 0.03f;
    float  drive    = 0.0f;
    float  subLevel = 0.0f;

    // LFO 状態 (1 つ、全ボイス共有)
    float  lfoPhase = 0.0f;
    float  lfoRate  = 2.0f;
    float  lfoDepth = 0.0f;
    int    lfoTarget = 0;

    juce::dsp::LadderFilter<float> ladder;
};
