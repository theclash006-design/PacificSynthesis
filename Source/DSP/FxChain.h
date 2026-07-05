#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

// 元コードの FX チェーン:
//   bus → FeedbackDelay (8n. = 付点8分) → Reverb (decay 1.5s) → Distortion → Master → Limiter
// JUCE 側でも同様の順序で組みます。
//
// マスターリミッタ:
//   HTML / Tone.js 版に寄せて 0dBFS 直前まで素通しにする。
//   |x| ≤ 0.95 はそのまま (= ほぼフルダイナミクスを残す)
//   |x| > 0.95 のみ tanh ソフトクリップで 1.0 に漸近
//   → ピークの突き抜けだけ守り、トランジェント (clap, snare) を潰さない
inline float softLimit (float x) noexcept
{
    constexpr float threshold = 0.95f;
    constexpr float range     = 1.0f - threshold;   // = 0.05
    const float a = std::abs (x);
    if (a <= threshold) return x;
    const float sign = (x > 0.0f) ? 1.0f : -1.0f;
    const float over = a - threshold;
    return sign * (threshold + range * std::tanh (over / range));
}

// ─────────────────────────────────────────────────────────────
//  SimpleCompressor — VCA-style ピーク検出 + log-domain GR
//   ・detector = max(|L|, |R|) (stereo link)
//   ・GR を log domain (dB) で算出、attack/release は 1-pole で gr_db を追従
//   ・最後に線形 gain へ戻して L/R に同じ係数を適用
// ─────────────────────────────────────────────────────────────
class SimpleCompressor
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        grDb = 0.0f;
        recomputeCoeffs();
    }
    void setThreshold (float dB) noexcept { thresholdDb = juce::jlimit (-60.0f, 0.0f, dB); }
    void setRatio     (float r)  noexcept { ratio = juce::jlimit (1.0f, 20.0f, r); }
    void setAttack    (float ms) noexcept { attackMs  = juce::jmax (0.1f, ms); recomputeCoeffs(); }
    void setRelease   (float ms) noexcept { releaseMs = juce::jmax (1.0f, ms); recomputeCoeffs(); }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n  = buffer.getNumSamples();
        const int ch = juce::jmin (2, buffer.getNumChannels());
        if (ch == 0) return;

        auto* L = buffer.getWritePointer (0);
        auto* R = (ch > 1) ? buffer.getWritePointer (1) : nullptr;

        const float kneeInvRatio = 1.0f - (1.0f / ratio);

        for (int i = 0; i < n; ++i)
        {
            // ─ 検出器: peak (stereo link)
            const float aL = std::abs (L[i]);
            const float aR = (R != nullptr) ? std::abs (R[i]) : aL;
            const float det = juce::jmax (aL, aR);

            // log-domain
            const float det_dB = 20.0f * std::log10 (det + 1.0e-9f);
            const float over   = juce::jmax (0.0f, det_dB - thresholdDb);
            const float target = -over * kneeInvRatio;   // ≤ 0 dB

            // 1-pole 追従: target < grDb なら attack (圧縮深まる)、それ以外は release
            const float coeff = (target < grDb) ? attackCoeff : releaseCoeff;
            grDb += (target - grDb) * coeff;

            // 線形 gain
            const float gain = std::pow (10.0f, grDb * (1.0f / 20.0f));

            L[i] *= gain;
            if (R != nullptr) R[i] *= gain;
        }
    }

    // 現在の GR (デバッグ/メーター用、UI が必要なら参照)
    float getGainReductionDb() const noexcept { return grDb; }

private:
    void recomputeCoeffs() noexcept
    {
        // 1-pole 時定数 (= e^(-1/(t*sr)))
        // attackCoeff / releaseCoeff は「1 - そのまま」で 1 ステップの追従率
        attackCoeff  = 1.0f - std::exp (-1.0f / (attackMs  * 0.001f * (float) sampleRate));
        releaseCoeff = 1.0f - std::exp (-1.0f / (releaseMs * 0.001f * (float) sampleRate));
    }

    double sampleRate = 44100.0;
    float  thresholdDb = -12.0f;
    float  ratio       = 4.0f;
    float  attackMs    = 5.0f;
    float  releaseMs   = 100.0f;
    float  attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float  grDb        = 0.0f;
};

class FxChain
{
public:
    void prepare (double sr, int blockSize, int numChannels = 2)
    {
        sampleRate = sr;
        osChannels = juce::jmax (1, numChannels);

        // ─ Master bus compressor (VCA-style)
        comp.prepare (sr);

        // juce::Reverb (juce_audio_basics) は setSampleRate でセットアップする
        reverb.reset();
        reverb.setSampleRate (sr);

        // Delay buffer: ステレオ、最大2秒
        const int maxDelay = (int) (sr * 2.0);
        delayLineL.setMaximumDelayInSamples (maxDelay);
        delayLineR.setMaximumDelayInSamples (maxDelay);
        delayLineL.reset(); delayLineR.reset();
        delayLineL.prepare ({ sr, (juce::uint32) juce::jmax (1, blockSize), 1 });
        delayLineR.prepare ({ sr, (juce::uint32) juce::jmax (1, blockSize), 1 });

        // ─ Distortion 2x オーバーサンプリング ─
        // tanh waveshaper のエイリアシング軽減用。常時 up/down で latency を一定に保つ
        // (drive=0 でも経路を通すことで host のレイテンシ補正がぶれない)。
        // host の出力チャンネル数 (1 or 2) に合わせて初期化。
        distOS = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) osChannels,
            (size_t) 1,    // 1 stage = 2x
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            false,         // isMaxQuality (IIR variant では未使用)
            true);         // useIntegerLatency = true (host のレイテンシ補正を揃える)
        distOS->initProcessing ((size_t) juce::jmax (1, blockSize));
        distOS->reset();

        // ─ アナログ感: Delay feedback に LP (テープ風に repeats が暗くなる)
        const float lpCutoff = 3500.0f;
        delayLpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * lpCutoff / (float) sr);
        delayFbStateL = delayFbStateR = 0.0f;

        // ─ アナログ感: Reverb 後段に高域シェルフカット (-3dB @ 6kHz、テール温かく)
        const auto shelfCoeffs = juce::IIRCoefficients::makeHighShelf (
            sr, 6000.0, 0.7,
            juce::Decibels::decibelsToGain (-3.0f));
        reverbShelfL.setCoefficients (shelfCoeffs);
        reverbShelfR.setCoefficients (shelfCoeffs);
        reverbShelfL.reset();
        reverbShelfR.reset();

        setTempo (128.0);
    }

    // ホストにレイテンシ報告するための値 (Oversampling 由来)
    int getLatencySamples() const noexcept
    {
        return distOS != nullptr ? (int) std::ceil (distOS->getLatencyInSamples()) : 0;
    }

    void setTempo (double bpm)
    {
        bpm = juce::jlimit (30.0, 300.0, bpm);
        // 8n. = 付点8分 = 1拍の 3/4
        const double seconds = (60.0 / bpm) * 0.75;
        delaySamples = (float) (seconds * sampleRate);
    }

    void setDelayMix (float m) noexcept    { delayMix = juce::jlimit (0.0f, 1.0f, m); }
    void setReverbMix (float m) noexcept
    {
        reverbMix = juce::jlimit (0.0f, 1.0f, m);
        applyReverbParams();
    }
    // Reverb mode: 0=Plate (small/bright)、1=Room (中庸)、2=Hall (large/dark)
    void setReverbMode (int mode) noexcept
    {
        reverbMode = juce::jlimit (0, 2, mode);
        applyReverbParams();
    }
    void setDistortion (float d) noexcept
    {
        distortionAmt = juce::jlimit (0.0f, 1.0f, d);
        // drive カーブ: 1.0 → 4.0 (緩やかなテーパー)
        //   amt=0.0 → drive=1.0  (素通し)
        //   amt=0.25→ drive=1.5  (うっすら)
        //   amt=0.5 → drive=2.1  (温かみ)
        //   amt=0.75→ drive=3.0  (明確な歪み)
        //   amt=1.0 → drive=4.0  (musical saturation 上限)
        // 線形成分 + 緩やかな二次成分でつまみ全域で連続的に変化を感じられるように
        distortionDrive = 1.0f + distortionAmt * (1.5f + distortionAmt * 1.5f);
        distortionNorm  = std::tanh (distortionDrive);   // 正規化用
    }
    void setMasterGain (float g) noexcept  { masterGain = juce::jlimit (0.0f, 2.0f, g); }

private:
    // ─ Reverb パラメータを mode と wet 量に応じて再計算
    //   Plate: 短く明るい (roomSize 小、damping 低、width 1)
    //   Room : 中庸 (roomSize 中、damping 中)
    //   Hall : 長く暗い (roomSize 大、damping 高、width 1)
    void applyReverbParams() noexcept
    {
        juce::Reverb::Parameters p;
        switch (reverbMode)
        {
            case 0: // Plate
                p.roomSize = 0.55f;
                p.damping  = 0.20f;
                p.width    = 1.0f;
                break;
            case 2: // Hall
                p.roomSize = 0.92f;
                p.damping  = 0.55f;
                p.width    = 1.0f;
                break;
            case 1: // Room (default)
            default:
                p.roomSize = 0.75f;
                p.damping  = 0.40f;
                p.width    = 0.85f;
                break;
        }
        p.wetLevel   = reverbMix;
        p.dryLevel   = 1.0f - reverbMix * 0.6f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
    }
public:

    // ─ Master bus compressor 設定
    void setCompThreshold (float dB) noexcept { comp.setThreshold (dB); }
    void setCompRatio     (float r)  noexcept { comp.setRatio (r); }
    void setCompAttack    (float ms) noexcept { comp.setAttack (ms); }
    void setCompRelease   (float ms) noexcept { comp.setRelease (ms); }
    float getCompGainReductionDb() const noexcept { return comp.getGainReductionDb(); }

    // 2ch オーディオブロック処理
    void process (juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        const int ch = juce::jmin (2, buffer.getNumChannels());

        // ─ Delay (アナログ風: feedback に LP + tanh saturation)
        //   テープエコー的に repeats が暗くなり (LP)、温かみが乗る (tanh)
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            auto& line    = (c == 0) ? delayLineL    : delayLineR;
            auto& fbState = (c == 0) ? delayFbStateL : delayFbStateR;
            for (int i = 0; i < n; ++i)
            {
                const float in = d[i];
                line.setDelay (delaySamples);
                const float tap = line.popSample (0);

                // 1-pole LP (~3.5kHz) でフィードバック経路の高域を削る
                fbState += (tap - fbState) * delayLpCoeff;
                // Soft saturation でアナログ温かみ (出力を抑えてユニティ近辺に)
                const float satTap = std::tanh (fbState * 1.2f) * 0.83f;

                line.pushSample (0, in + satTap * 0.35f);
                d[i] = in + tap * delayMix;
            }
        }

        // ─ Reverb (juce_audio_basics::Reverb)
        if (ch == 2)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
        else
            reverb.processMono (buffer.getWritePointer (0), n);

        // ─ アナログ感: Reverb 出力後に高域シェルフ (-3dB @ 6kHz)
        //   テールの dispersion を温かくする (ドライ信号への影響は最小)
        for (int c = 0; c < ch; ++c)
        {
            auto& shelf = (c == 0) ? reverbShelfL : reverbShelfR;
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = shelf.processSingleSampleRaw (d[i]);
        }

        // ─ Distortion: 2x オーバーサンプル → tanh waveshape → ダウンサンプル
        //   OS は「常に通す」(drive=0 でも up/down 経路を通すことで latency が一定)。
        //   tanh だけは amt > 0 のときに限り適用 → drive=1 の時の 1/tanh(1) 倍の
        //   ゲインバンプを発生させないため。
        if (distOS != nullptr)
        {
            juce::dsp::AudioBlock<float> ioBlock (buffer);
            auto osBlock = distOS->processSamplesUp (ioBlock);

            if (distortionAmt > 0.001f)
            {
                const float drive = distortionDrive;
                const float norm  = distortionNorm;
                const int osN  = (int) osBlock.getNumSamples();
                const int osCh = (int) osBlock.getNumChannels();
                for (int c = 0; c < osCh; ++c)
                {
                    auto* d = osBlock.getChannelPointer ((size_t) c);
                    for (int i = 0; i < osN; ++i)
                        d[i] = std::tanh (drive * d[i]) / norm;
                }
            }
            distOS->processSamplesDown (ioBlock);
        }
        else if (distortionAmt > 0.001f)
        {
            // フォールバック (prepare 前等で OS 未準備の場合)
            const float drive = distortionDrive;
            const float norm  = distortionNorm;
            for (int c = 0; c < ch; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = std::tanh (drive * d[i]) / norm;
            }
        }

        // ─ Master bus compressor (VCA-style, stereo-link)
        //   Distortion 後、Master Gain 前に挿入 → ピークを丸めてからレベル合わせ。
        //   Threshold 0 dB なら実質透過 (over=0、GR=0)。
        comp.process (buffer);

        // ─ Master gain
        buffer.applyGain (masterGain);

        // ─ マスターリミッタ (0dBFS を超えないようにソフトクリップ)
        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = softLimit (d[i]);
        }
    }

private:
    double sampleRate = 44100.0;
    float  delaySamples = 0.0f;
    float  delayMix = 0.15f;
    float  reverbMix = 0.1f;
    int    reverbMode = 1;   // 0=Plate, 1=Room, 2=Hall
    float  distortionAmt = 0.0f;
    float  distortionDrive = 1.0f;
    float  distortionNorm  = std::tanh (1.0f);
    float  masterGain = 0.7f;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL, delayLineR;
    juce::Reverb reverb;
    std::unique_ptr<juce::dsp::Oversampling<float>> distOS;
    int osChannels = 2;
    SimpleCompressor comp;

    // ─ アナログ感用のフィルタ状態 ─
    // Delay feedback の 1-pole LP (テープ風暗化)
    float delayLpCoeff   = 0.0f;
    float delayFbStateL  = 0.0f;
    float delayFbStateR  = 0.0f;
    // Reverb 後段の高域シェルフ (テール温かく)
    juce::IIRFilter reverbShelfL, reverbShelfR;
};
