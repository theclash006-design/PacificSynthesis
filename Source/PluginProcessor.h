#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

#include "Params.h"
#include "PatternState.h"
#include "DSP/BassVoice.h"
#include "DSP/DrumVoices.h"
#include "DSP/FxChain.h"
#include "DSP/Sequencer.h"

class PacificSynthesisProcessor : public juce::AudioProcessor
{
public:
    PacificSynthesisProcessor();
    ~PacificSynthesisProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ─── Editor から参照 ───
    // 注: patternUndoManager は apvts より先に宣言 (apvts の ctor が参照する)
    // 旧名 "patternUndoManager" を維持 (実態は params + pattern 両方の Undo を扱う)
    juce::UndoManager                  patternUndoManager { 30000, 30 };  // 30KB / 30 step
    juce::AudioProcessorValueTreeState apvts;
    PatternState                       pattern;
    Sequencer                          sequencer;

    // ─── 内蔵Play (ホスト非再生時にも動かす) ───
    void setInternalPlaying (bool playing) noexcept;
    bool isInternalPlaying() const noexcept { return internalPlaying.load(); }
    bool isCurrentlyPlaying() const noexcept;
    double getDisplayBpm() const noexcept { return displayBpm.load(); }

    // ─── パート単位ミュート (表示モード連動) ───
    void setDrumsMuted (bool m) noexcept { drumsMuted.store (m); }
    void setBassMuted  (bool m) noexcept { bassMuted .store (m); }
    bool areDrumsMuted () const noexcept { return drumsMuted.load(); }
    bool isBassMuted   () const noexcept { return bassMuted .load(); }

    // ─── View モード状態を Editor で永続化 (DAW 上で editor 再オープンしても維持) ───
    //   0 = Both / 1 = DrumOnly / 2 = BassOnly  ※ StepGridComponent::ViewMode と一致
    void setSavedViewMode (int vm) noexcept { savedViewMode.store (vm); }
    int  getSavedViewMode () const noexcept { return savedViewMode.load(); }

    // ─── Voice 個別の発音ソース 3 段階切替 ───
    //   0 = SEQ : 内蔵パターン再生 (+ DAW MIDI もミックス)
    //   1 = MIDI: 内蔵パターン停止、DAW MIDI のみ発音
    //   2 = OFF : 内蔵パターン停止、DAW MIDI も無視 (完全ミュート)
    // UI: グリッドの各トラック右端に 3-cycle ボタン
    static constexpr int kSrcSeq  = 0;
    static constexpr int kSrcMidi = 1;
    static constexpr int kSrcOff  = 2;

    void setVoiceSrc (int voiceIndex, int mode) noexcept;  // 0=Kick 1=Snare 2=HiHat 3=Clap 4=Bass
    int  getVoiceSrc (int voiceIndex) const noexcept;
    // 旧API互換 (true = MIDI mode のみ)
    bool isVoiceExternal (int voiceIndex) const noexcept { return getVoiceSrc (voiceIndex) == kSrcMidi; }
    bool isVoiceOff      (int voiceIndex) const noexcept { return getVoiceSrc (voiceIndex) == kSrcOff;  }

    // ─── パターン → MIDI ファイル書き出し ───
    //   buildMidiFile      … 互換性維持 (ドラム+ベース 1ファイル, トラック名付き)
    //   buildDrumMidiFile  … ドラムのみ (channel 10)
    //   buildBassMidiFile  … ベースのみ (channel 1)
    juce::MidiFile buildMidiFile() const;
    juce::MidiFile buildDrumMidiFile() const;
    juce::MidiFile buildBassMidiFile() const;

    // ─── MIDI CC マッピング ───
    // ccToParam[ccNumber] = "" なら未割当、それ以外は param ID。
    // 受信した CC は 0-127 を 0-1 に正規化して、対応 param に setValueNotifyingHost。
    // UI から MIDI Learn 状態を開始/取消 + 現在の割当を問い合わせる。
    void startMidiLearn (const juce::String& paramID) noexcept;
    void cancelMidiLearn () noexcept;
    void clearMidiMapping (const juce::String& paramID) noexcept;
    // 指定 param ID に割り当てられている CC 番号 (なければ -1)
    int  getCCForParam (const juce::String& paramID) const noexcept;
    // 現在 Learn 待機中の param ID (空文字なら待機なし)
    juce::String getLearnTargetParam () const noexcept { return learnTargetParam; }

private:
    void handleMidiEvent (const juce::MidiMessage& m);
    void triggerPatternStep (int stepIndex);
    void pullParameters();
    void cacheParameterPointers();
    // Bass Chord 対応: root + 自動ハーモニー intervals でトリガー
    //   slideRoot は root にのみ適用、ハーモニーは常に slide=false (別ボイスへ)
    void triggerBassWithChord (int rootMidi, float velocity, bool accent, bool slideRoot);

    // ── RT-safe: パラメータ atomic ポインタを prepareToPlay でキャッシュ ──
    // (毎ブロックの getRawParameterValue 呼び出しを避ける)
    struct ParamPointers
    {
        std::atomic<float>* bassWave   = nullptr;
        std::atomic<float>* cutoff     = nullptr;
        std::atomic<float>* reso       = nullptr;
        std::atomic<float>* envMod     = nullptr;
        std::atomic<float>* bassDec    = nullptr;
        std::atomic<float>* accAmt     = nullptr;
        std::atomic<float>* delMix     = nullptr;
        std::atomic<float>* revMix     = nullptr;
        std::atomic<float>* revMode    = nullptr;
        std::atomic<float>* dist       = nullptr;
        std::atomic<float>* masterGain = nullptr;
        std::atomic<float>* bpm        = nullptr;
        std::atomic<float>* swing      = nullptr;
        std::atomic<float>* kickDec    = nullptr;
        std::atomic<float>* snareDec   = nullptr;
        std::atomic<float>* hhDec      = nullptr;
        std::atomic<float>* kickPitch  = nullptr;
        std::atomic<float>* kickAmt    = nullptr;
        std::atomic<float>* kickDrive  = nullptr;
        std::atomic<float>* snareNoise = nullptr;
        std::atomic<float>* snareTone  = nullptr;
        std::atomic<float>* snareSnap  = nullptr;
        std::atomic<float>* snareCrisp = nullptr;
        std::atomic<float>* hhFreq     = nullptr;
        std::atomic<float>* hhQ        = nullptr;
        std::atomic<float>* hhMetal    = nullptr;
        std::atomic<float>* hhDrive    = nullptr;
        std::atomic<float>* clapFreq   = nullptr;
        std::atomic<float>* clapQ      = nullptr;
        std::atomic<float>* clapDec    = nullptr;
        std::atomic<float>* clapSpread = nullptr;
        std::atomic<float>* clapStyle  = nullptr;
        std::atomic<float>* bassGlide  = nullptr;
        std::atomic<float>* bassDrive  = nullptr;
        std::atomic<float>* bassSub    = nullptr;
        std::atomic<float>* kickVol    = nullptr;
        std::atomic<float>* snareVol   = nullptr;
        std::atomic<float>* hhVol      = nullptr;
        std::atomic<float>* clapVol    = nullptr;
        std::atomic<float>* bassVol    = nullptr;
        std::atomic<float>* kickPan    = nullptr;
        std::atomic<float>* snarePan   = nullptr;
        std::atomic<float>* hhPan      = nullptr;
        std::atomic<float>* clapPan    = nullptr;
        std::atomic<float>* bassPan    = nullptr;
        std::atomic<float>* scAmt      = nullptr;
        std::atomic<float>* scRel      = nullptr;
        std::atomic<float>* hhOpenDec  = nullptr;
        std::atomic<float>* compThresh = nullptr;
        std::atomic<float>* compRatio  = nullptr;
        std::atomic<float>* compAtk    = nullptr;
        std::atomic<float>* compRel    = nullptr;
        std::atomic<float>* bassLfoRate   = nullptr;
        std::atomic<float>* bassLfoDepth  = nullptr;
        std::atomic<float>* bassLfoTarget = nullptr;
        std::atomic<float>* bassChord     = nullptr;
    };
    ParamPointers params;

    BassVoice  bass;
    KickVoice  kick;
    SnareVoice snare;
    HiHatVoice hihat;
    ClapVoice  clap;
    FxChain    fx;

    // (旧 activeBassNote はポリフォニック MIDI 化により廃止)
    int activeBassPatternNote = -1;   // パターンのスライド検出用

    // 内蔵Play 状態
    std::atomic<bool>   internalPlaying { false };
    std::atomic<double> displayBpm      { 128.0 };

    // パートミュート (UI の表示モードと連動)
    std::atomic<bool>   drumsMuted      { false };
    std::atomic<bool>   bassMuted       { false };
    // VIEW モード (BOTH=0 / DrumOnly=1 / BassOnly=2)
    std::atomic<int>    savedViewMode   { 0 };
    double              internalPpq     = 0.0;
    bool                hostWasPlaying  = false;

    // Voice 個別の発音ソース mode (0=SEQ, 1=MIDI, 2=OFF)
    //   インデックス: 0=Kick 1=Snare 2=HiHat 3=Clap 4=Bass
    std::array<std::atomic<int>, 5> voiceSrcMode { 0, 0, 0, 0, 0 };
    static constexpr int kKickVoice  = 0;
    static constexpr int kSnareVoice = 1;
    static constexpr int kHiHatVoice = 2;
    static constexpr int kClapVoice  = 3;
    static constexpr int kBassVoice  = 4;

    // 個別ボリューム (pullParametersでセット)
    float kickVol = 1.0f, snareVol = 0.9f, hhVol = 0.5f, clapVol = 0.7f, bassVol = 0.9f;

    // パン (constant-power 係数を pullParameters で事前計算)
    struct PanGain { float gL = 1.0f, gR = 1.0f; };
    PanGain kickPanG, snarePanG, hhPanG, clapPanG, bassPanG;
    static PanGain calcPan (float pan) noexcept
    {
        pan = juce::jlimit (-1.0f, 1.0f, pan);
        // -1=L, 0=center, +1=R   constant-power (cos/sin) で中央0dB
        const float a = (pan + 1.0f) * 0.5f * juce::MathConstants<float>::halfPi;
        return { std::cos (a), std::sin (a) };
    }

    // ─ MIDI CC マッピング ─
    //   ccToParam[ccNumber] = "" なら未割当、それ以外は param ID 文字列。
    //   audio スレッドからの参照は std::atomic<...>* params にあるので簡素化のため
    //   非 atomic な std::array 文字列で保持 (UI が変更、audio thread が読む)。
    //   ※ 同時書込が無いことを前提とする (UI 1 つのみ、audio 1 つのみ)
    std::array<juce::String, 128> ccToParam;
    juce::String                  learnTargetParam;   // 空文字 = Learn 待機なし

    // ─ サイドチェイン (Kick → Bass duck) ─
    // Kick trigger 時に scEnv を 1.0 にセット、毎サンプル scReleaseCoeff で減衰。
    // Bass の出力に (1 - scAmt * scEnv) を乗算して duck。
    float scEnv          = 0.0f;
    float scReleaseCoeff = 0.0f;  // pullParameters で再計算
    float scAmt          = 0.0f;
    // Kick トリガー (パターン/MIDI) のタイミングで envelope を即時 1.0 へ
    void triggerSidechain() noexcept { scEnv = 1.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PacificSynthesisProcessor)
};
