#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <random>

// 共通: 簡易ホワイトノイズ
struct WhiteNoise
{
    float next() noexcept { return dist (rng); }
    std::mt19937 rng { 0xc0a51u };
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };
};

// 簡易ピンクノイズ
struct PinkNoise
{
    WhiteNoise wn;
    float b0 = 0, b1 = 0, b2 = 0;
    float next() noexcept
    {
        const float w = wn.next();
        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        return (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
    }
};

// ─────────────────────────────────────────────────────────────
//  KICK — sine + pitch envelope (n oct decaying) + attack click
//  Tunable: pitch (base Hz), pitchAmt (octaves), decay, drive
//  Improvement: アタック直後に短いノイズクリックを足してパンチ感を出す
// ─────────────────────────────────────────────────────────────
class KickVoice
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        recomputeCoeffs();
    }
    void setDecay (float s) { decay = juce::jmax (0.05f, s); recomputeCoeffs(); }
    void setPitch (float hz) { baseHz = juce::jlimit (20.0f, 250.0f, hz); }
    void setPitchAmt (float oct) { pitchAmt = juce::jlimit (0.0f, 10.0f, oct); }
    void setPitchDecay (float s) { pitchDecaySec = juce::jmax (0.005f, s); recomputeCoeffs(); }
    void setDrive (float d) { drive = juce::jlimit (0.0f, 1.0f, d); }

    void trigger (float velocity)
    {
        amp = juce::jlimit (0.0f, 1.0f, velocity);
        pitchEnv = 1.0f;
        phase = 0.0f;
        // クリック: 短いノイズバースト (~2ms)
        clickAmp = amp * 0.45f;
    }

    float render() noexcept
    {
        if (amp < 1.0e-5f && clickAmp < 1.0e-5f) return 0.0f;

        // メイントーン (sine + pitch env)
        pitchEnv *= pitchEnvCoeff;
        const float octMod = pitchAmt * pitchEnv;
        // base=2 専用の std::exp2 は std::pow(2,x) より高速 (per-sample 呼び出しの最適化)
        const float instHz = baseHz * std::exp2 (octMod);
        phase += instHz / (float) sampleRate;
        if (phase >= 1.0f) phase -= 1.0f;
        amp *= ampDecayCoeff;
        float s = std::sin (phase * juce::MathConstants<float>::twoPi) * amp;

        // クリック層 (高速減衰のノイズバースト)
        if (clickAmp > 1.0e-5f)
        {
            s += clickNoise.next() * clickAmp;
            clickAmp *= clickDecayCoeff;
        }

        // Drive (tanh saturation)
        if (drive > 0.001f)
            s = std::tanh (s * (1.0f + drive * 6.0f)) / std::tanh (1.0f + drive * 6.0f);
        // HTML/Tone.js の MembraneSynth と同様、出力はそのまま (リミッタ緩和済み)
        return s;
    }

private:
    void recomputeCoeffs() noexcept
    {
        ampDecayCoeff = std::exp (-1.0f / (decay * (float) sampleRate));
        pitchEnvCoeff = std::exp (-1.0f / (pitchDecaySec * (float) sampleRate));
        // クリックの減衰時間 = 2ms (固定)
        clickDecayCoeff = std::exp (-1.0f / (0.002f * (float) sampleRate));
    }

    double sampleRate = 44100.0;
    float  decay = 0.4f;
    float  pitchDecaySec = 0.05f;
    float  baseHz = 50.0f;
    float  pitchAmt = 6.0f;
    float  drive = 0.0f;
    float  amp = 0.0f, pitchEnv = 0.0f, phase = 0.0f;
    float  clickAmp = 0.0f;
    float  ampDecayCoeff = 0.0f, pitchEnvCoeff = 0.0f, clickDecayCoeff = 0.0f;
    WhiteNoise clickNoise;
};

// ─────────────────────────────────────────────────────────────
//  SNARE
//  Tunable: noiseMix (white→pink), tone (200Hz tonal layer amount),
//           snap (very-short transient burst), decay
// ─────────────────────────────────────────────────────────────
class SnareVoice
{
public:
    void prepare (double sr) { sampleRate = sr; recomputeCoeffs(); updateHpCoeff(); }
    void setDecay (float s) { decay = juce::jmax (0.03f, s); recomputeCoeffs(); }
    void setNoiseMix (float m) { noiseMix = juce::jlimit (0.0f, 1.0f, m); }
    void setTone (float t) { tone = juce::jlimit (0.0f, 1.0f, t); }
    void setSnap (float s) { snap = juce::jlimit (0.0f, 1.0f, s); recomputeCoeffs(); }

    // Crisp: 0 = lo-fi (full-band noise, current behavior), 1 = crispy (HP ~5kHz)
    // 1-pole HP の cutoff を 30Hz → 5kHz でスウィープ (二乗カーブで低い側の解像度高く)
    void setCrisp (float c) noexcept
    {
        crisp = juce::jlimit (0.0f, 1.0f, c);
        updateHpCoeff();
    }

    void trigger (float velocity)
    {
        amp = juce::jlimit (0.0f, 1.0f, velocity);
        tonalAmp = amp * 0.5f;
        tonePitch = 1.0f;
        snapAmp = snap * amp * 1.6f;
        phase = 0.0f;
    }

    float render() noexcept
    {
        if (amp < 1.0e-5f && snapAmp < 1.0e-5f) return 0.0f;

        const float n = wn.next() * (1.0f - noiseMix) + pn.next() * noiseMix * 1.6f;

        tonePitch *= tonePitchCoeff;
        const float toneHz = 200.0f * (1.0f + 1.5f * tonePitch);
        phase += toneHz / (float) sampleRate;
        if (phase >= 1.0f) phase -= 1.0f;
        const float tonal = std::sin (phase * juce::MathConstants<float>::twoPi) * tonalAmp * tone;

        snapAmp *= snapCoeff;
        const float snapBurst = wn.next() * snapAmp;

        amp      *= ampDecayCoeff;
        tonalAmp *= ampDecayCoeff;

        // ─ Noise を 1-pole HP で必要に応じてブライト化
        //   y[n] = a*(y[n-1] + x[n] - x[n-1])
        //   crisp=0 → fc=30Hz (実質スルー)、crisp=1 → fc=5kHz (キレッキレ)
        const float noiseIn = n * amp * 0.6f;
        const float hpOut = hpA * (hpY + noiseIn - hpX);
        hpX = noiseIn;
        hpY = hpOut;

        return hpOut + tonal + snapBurst;
    }

private:
    void recomputeCoeffs() noexcept
    {
        ampDecayCoeff  = std::exp (-1.0f / (decay * (float) sampleRate));
        tonePitchCoeff = std::exp (-1.0f / (0.04f * (float) sampleRate));
        // snap タウは snap値に応じて 1ms〜5ms
        const float snapTau = 0.001f + (1.0f - snap) * 0.005f;
        snapCoeff = std::exp (-1.0f / (snapTau * (float) sampleRate));
    }
    void updateHpCoeff() noexcept
    {
        // crisp の二乗で低い側の解像度を上げる
        const float fc = 30.0f + crisp * crisp * 5000.0f;
        // 1-pole HP: a = exp(-2π fc / sr)、これに近いほどパススルー
        hpA = std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sampleRate);
    }

    double sampleRate = 44100.0;
    float  decay = 0.15f;
    float  noiseMix = 0.0f, tone = 0.5f, snap = 0.5f;
    float  crisp = 0.0f;
    float  amp = 0.0f, tonalAmp = 0.0f, tonePitch = 0.0f, phase = 0.0f;
    float  snapAmp = 0.0f;
    float  ampDecayCoeff = 0.0f, tonePitchCoeff = 0.0f, snapCoeff = 0.0f;
    // 1-pole HP filter (Crisp)
    float  hpA = 0.99f, hpX = 0.0f, hpY = 0.0f;
    WhiteNoise wn;
    PinkNoise  pn;
};

// ─────────────────────────────────────────────────────────────
//  HIHAT — white noise → bandpass, +metal (multi square mix)
//  Tunable: freq, Q, decay, metal
// ─────────────────────────────────────────────────────────────
class HiHatVoice
{
public:
    void prepare (double sr) { sampleRate = sr; recomputeCoeffs(); updateFilter(); }
    void setDecay (float s)     { closedDec = juce::jmax (0.005f, s); recomputeCoeffs(); }
    void setOpenDecay (float s) { openDec   = juce::jmax (0.05f,  s); recomputeCoeffs(); }
    void setFreq (float f)  { freqHz = juce::jlimit (1000.0f, 16000.0f, f); updateFilter(); }
    void setQ (float q)     { qVal   = juce::jlimit (0.5f, 15.0f, q); updateFilter(); }
    void setMetal (float m) { metal  = juce::jlimit (0.0f, 1.0f, m); }
    // Drive: 0=clean (BP'd noise そのまま)、1=tanh saturation (荒くアグレッシブ)
    void setDrive (float d) { drive  = juce::jlimit (0.0f, 1.0f, d); }

    // Closed HiHat (新トリガーで amp 上書き = 自然なチョーク)
    void trigger (float velocity)
    {
        amp = juce::jlimit (0.0f, 1.0f, velocity);
        ampIsOpen = false;
        ampDecayCoeff = closedCoeff;
    }
    // Open HiHat (closed と排他、長めの decay)
    void triggerOpen (float velocity)
    {
        amp = juce::jlimit (0.0f, 1.0f, velocity);
        ampIsOpen = true;
        ampDecayCoeff = openCoeff;
    }

    float render() noexcept
    {
        if (amp < 1.0e-5f) return 0.0f;

        // ベース: ホワイトノイズ
        float s = wn.next();

        // メタル成分: 6つの矩形波 (40Hz, 78, 116, 154, 192, 230 ベース)
        if (metal > 0.001f)
        {
            float sq = 0.0f;
            for (int i = 0; i < 6; ++i)
            {
                metalPhase[i] += metalFreqs[i] / (float) sampleRate;
                if (metalPhase[i] >= 1.0f) metalPhase[i] -= 1.0f;
                sq += (metalPhase[i] < 0.5f) ? 1.0f : -1.0f;
            }
            s = s * (1.0f - metal) + (sq / 6.0f) * metal;
        }

        s = filter.processSingleSampleRaw (s);

        // ─ Drive: post-filter で tanh saturation を追加 (倍音増加で aggressive)
        if (drive > 0.001f)
        {
            const float driveAmt = 1.0f + drive * 5.0f;     // 1.0 〜 6.0
            s = std::tanh (s * driveAmt) / std::tanh (driveAmt);
        }

        amp *= ampDecayCoeff;
        // HTML/Tone.js の NoiseSynth + Filter(8000,bandpass) と同等レベル。
        // 内部ゲインは 0.8 (旧 1.2 より控えめ、リミッタ緩和済みなのでヘッドルームあり)
        return s * amp * 0.8f;
    }

private:
    void recomputeCoeffs() noexcept
    {
        closedCoeff = std::exp (-1.0f / (closedDec * (float) sampleRate));
        openCoeff   = std::exp (-1.0f / (openDec   * (float) sampleRate));
        ampDecayCoeff = ampIsOpen ? openCoeff : closedCoeff;
    }
    void updateFilter()
    {
        filter.setCoefficients (juce::IIRCoefficients::makeBandPass (sampleRate, freqHz, qVal));
    }

    double sampleRate = 44100.0;
    float  closedDec = 0.06f;
    float  openDec   = 0.40f;
    float  closedCoeff = 0.0f, openCoeff = 0.0f;
    bool   ampIsOpen = false;
    float  freqHz = 8000.0f, qVal = 3.5f, metal = 0.0f;
    float  drive = 0.0f;
    float  amp = 0.0f, ampDecayCoeff = 0.0f;
    // metal oscillators — TR-808 の実測周波数
    // (オリジナル 808 で使われている矩形波発振器の周波数)
    float  metalPhase[6] { 0,0,0,0,0,0 };
    const float metalFreqs[6] { 215.0f, 304.0f, 364.0f, 467.0f, 542.0f, 681.0f };
    WhiteNoise wn;
    juce::IIRFilter filter;
};

// ─────────────────────────────────────────────────────────────
//  CLAP — pink noise → bandpass, single AR envelope (HTML/Tone.js 寄り)
//  HTML 版は NoiseSynth(pink) → Filter(1200Hz, bandpass) の単発バースト。
//  ここでも同じ構造に簡素化 (旧マルチバーストは廃止)。
//  Tunable: freq, Q, decay, spread (※ spread は UI 互換のため残すが内部で未使用)
// ─────────────────────────────────────────────────────────────
class ClapVoice
{
public:
    void prepare (double sr) { sampleRate = sr; recomputeCoeffs(); updateFilter(); }
    void setDecay (float s) { decay = juce::jmax (0.03f, s); recomputeCoeffs(); }
    void setFreq (float f)  { freqHz = juce::jlimit (300.0f, 4000.0f, f); updateFilter(); }
    void setQ (float q)     { qVal   = juce::jlimit (0.5f, 10.0f, q); updateFilter(); }
    void setSpread (float spreadSec) { (void) spreadSec; /* HTML版互換のため受けるだけ */ }

    // Style: 0.00–0.33 = Single (current)、0.33–0.67 = 707 (2 bursts)、0.67–1.00 = 909 (3 bursts)
    // 内部で量子化、UI 表示はステップ感ある rotary でも OK
    void setStyle (float s) noexcept
    {
        style = juce::jlimit (0.0f, 1.0f, s);
        if      (style < 0.33f) targetBursts = 1;
        else if (style < 0.67f) targetBursts = 2;
        else                    targetBursts = 3;
    }

    void trigger (float velocity)
    {
        amp = juce::jlimit (0.0f, 1.0f, velocity);
        triggerVel = amp;
        // 残りバースト数 (現在のトリガが 1 つ目なので、目標 - 1)
        burstsRemaining = juce::jmax (0, targetBursts - 1);
        // バースト間隔: 909 (3 bursts) は 13ms、707 (2 bursts) は 22ms
        const float spacingSec = (targetBursts == 3) ? 0.013f : 0.022f;
        burstSpacingSamples = (int) (spacingSec * (float) sampleRate);
        nextBurstCountdown  = burstSpacingSamples;
    }

    float render() noexcept
    {
        // 追加バーストのスケジュール
        if (burstsRemaining > 0)
        {
            --nextBurstCountdown;
            if (nextBurstCountdown <= 0)
            {
                // 後続バーストは少し弱めに (自然な減衰感)
                amp = triggerVel * (0.85f - 0.12f * (float) (targetBursts - burstsRemaining));
                --burstsRemaining;
                nextBurstCountdown = burstSpacingSamples;
            }
        }

        if (amp < 1.0e-5f) return 0.0f;

        float s = pn.next();
        s = filter.processSingleSampleRaw (s);

        amp *= ampDecayCoeff;
        // HTML版相当の自然な出力レベル
        return s * amp * 0.9f;
    }

private:
    void recomputeCoeffs() noexcept
    {
        ampDecayCoeff = std::exp (-1.0f / (decay * (float) sampleRate));
    }
    void updateFilter()
    {
        filter.setCoefficients (juce::IIRCoefficients::makeBandPass (sampleRate, freqHz, qVal));
    }

    double sampleRate = 44100.0;
    float  decay = 0.12f;
    float  freqHz = 1200.0f, qVal = 2.5f;
    float  amp = 0.0f, ampDecayCoeff = 0.0f;
    // Multi-burst state
    float  style = 0.0f;
    int    targetBursts = 1;
    int    burstsRemaining = 0;
    int    burstSpacingSamples = 0;
    int    nextBurstCountdown = 0;
    float  triggerVel = 0.0f;
    PinkNoise pn;
    juce::IIRFilter filter;
};
