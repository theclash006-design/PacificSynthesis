#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace PacificParams
{
    // ─── Parameter IDs (元HTMLの State 名と1:1で対応) ───
    inline constexpr auto BassWave   = "bassWave";   // 0 = saw, 1 = square
    inline constexpr auto Cutoff     = "cutoff";     // 100–5000 Hz
    inline constexpr auto Reso       = "reso";       // 0–1 (Ladder filter resonance)
    inline constexpr auto EnvMod     = "envMod";     // 0–6000 Hz (filter env amount)
    inline constexpr auto BassDec    = "bassDec";    // 0.05–1.5 s
    inline constexpr auto AccAmt     = "accAmt";     // 0–1
    inline constexpr auto DelMix     = "delMix";     // 0–0.6
    inline constexpr auto RevMix     = "revMix";     // 0–0.6
    inline constexpr auto RevMode    = "revMode";    // 0=Plate, 1=Room, 2=Hall
    inline constexpr auto Dist       = "dist";       // 0–1
    inline constexpr auto MasterGain = "masterGain"; // 0–1

    inline constexpr auto KickDec    = "kickDec";    // 0.05–1.5 s
    inline constexpr auto SnareDec   = "snareDec";   // 0.05–1.5 s
    inline constexpr auto HhDec      = "hhDec";      // 0.01–0.4 s

    // ─ 個別音色 詳細パラメータ ─
    // Kick
    inline constexpr auto KickPitch  = "kickPitch";  // 40–120 Hz
    inline constexpr auto KickAmt    = "kickAmt";    // 1–8 octaves
    inline constexpr auto KickDrive  = "kickDrive";  // 0–1
    // Snare
    inline constexpr auto SnareNoise = "snareNoise"; // 0=white, 1=pink
    inline constexpr auto SnareTone  = "snareTone";  // 0–1
    inline constexpr auto SnareSnap  = "snareSnap";  // 0–1
    inline constexpr auto SnareCrisp = "snareCrisp"; // 0=lo-fi, 1=crispy HP
    // HiHat
    inline constexpr auto HhFreq     = "hhFreq";     // 4000–12000 Hz
    inline constexpr auto HhQ        = "hhQ";        // 1–10
    inline constexpr auto HhMetal    = "hhMetal";    // 0–1
    inline constexpr auto HhDrive    = "hhDrive";    // 0=clean, 1=saturated
    // Clap
    inline constexpr auto ClapFreq   = "clapFreq";   // 800–2000 Hz
    inline constexpr auto ClapQ      = "clapQ";      // 1–5
    inline constexpr auto ClapDec    = "clapDec";    // 0.03–0.4 s
    inline constexpr auto ClapSpread = "clapSpread"; // 0.005–0.05
    inline constexpr auto ClapStyle  = "clapStyle";  // 0=Single, 0.5=707, 1=909
    // Bass (extras)
    inline constexpr auto BassGlide  = "bassGlide";  // 0–0.2 s
    inline constexpr auto BassDrive  = "bassDrive";  // 0–1
    inline constexpr auto BassSub    = "bassSub";    // 0–1

    // ─ 各音色 個別 Vol (ミックスバランス用) ─
    inline constexpr auto KickVol    = "kickVol";    // 0–1.5
    inline constexpr auto SnareVol   = "snareVol";
    inline constexpr auto HhVol      = "hhVol";
    inline constexpr auto ClapVol    = "clapVol";
    inline constexpr auto BassVol    = "bassVol";

    // ─ 各音色 Pan (-1.0 = L / 0 = center / +1.0 = R) ─
    inline constexpr auto KickPan    = "kickPan";
    inline constexpr auto SnarePan   = "snarePan";
    inline constexpr auto HhPan      = "hhPan";
    inline constexpr auto ClapPan    = "clapPan";
    inline constexpr auto BassPan    = "bassPan";

    // ─ Sidechain (Kick → Bass duck) ─
    inline constexpr auto ScAmt      = "scAmt";      // 0–1 (duck depth)
    inline constexpr auto ScRel      = "scRel";      // 0.02–0.5 s (release time)

    // ─ HiHat Open (Closed と排他) ─
    inline constexpr auto HhOpenDec  = "hhOpenDec";  // 0.05–1.5 s (open hihat decay)

    // ─ Master Bus Compressor (VCA-style) ─
    inline constexpr auto CompThresh = "compThresh"; // -60 〜 0 dB
    inline constexpr auto CompRatio  = "compRatio";  // 1〜20
    inline constexpr auto CompAtk    = "compAtk";    // 0.1〜50 ms
    inline constexpr auto CompRel    = "compRel";    // 10〜500 ms

    // ─ Bass LFO (sin, 自由 Hz、cutoff or pitch をモジュレート) ─
    inline constexpr auto BassLfoRate   = "bassLfoRate";   // 0.05〜20 Hz
    inline constexpr auto BassLfoDepth  = "bassLfoDepth";  // 0〜1
    inline constexpr auto BassLfoTarget = "bassLfoTarget"; // 0=Cutoff, 1=Pitch

    // ─ Bass Chord mode (内蔵シーケンサ + 外部 MIDI 両方に適用) ─
    // 0=Off (mono)、1=+Oct、2=Power(5th)、3=Maj、4=Min、5=Sus4
    inline constexpr auto BassChord = "bassChord";

    // Transport
    inline constexpr auto Bpm        = "bpm";        // 60–200 (内蔵Play時に使う)
    inline constexpr auto Swing      = "swing";      // 0–1

    // ─── MIDI Note Mapping (チャンネル別ルーティング方式) ───
    //
    // MIDI 出力 (Drag-to-DAW):
    //   ・bass.mid  → MIDI channel  1
    //   ・drums.mid → MIDI channel 10  (GM 規格のドラムチャンネル)
    //
    // MIDI 入力ルール (シンプル方式):
    //   ・channel 10           → 全領域ドラム判定 (高領域 84+ + GM 低領域 36+)
    //   ・channel 10 以外      → 全てベース (C5/D5/D#5 等の高領域も Bass で演奏可)
    //
    // 使い分け: MIDI キーボードでドラムを叩きたい時は ch 10、ベースを弾きたい時は
    //          ch 1〜9 または 11〜16 (どれでも可)。
    //
    // 高領域ドラム (MIDI 書き出しのデフォルト、チャンネル無視で常にドラム)
    // ※ Yamaha / Studio One / Ableton 規格 (Middle C = C3 = MIDI 60) 表記
    inline constexpr int KickNote  = 84; // C5
    inline constexpr int SnareNote = 86; // D5
    inline constexpr int ClapNote  = 87; // D#5
    inline constexpr int HiHatNote = 90; // F#5
    inline constexpr int OpenHiHatNote = 92; // G#5 (open HiHat、closed と排他)

    // GM 互換低領域 (channel 10 でのみドラム判定)
    inline constexpr int KickNoteGM  = 36; // C1
    inline constexpr int SnareNoteGM = 38; // D1
    inline constexpr int ClapNoteGM  = 39; // D#1
    inline constexpr int HiHatNoteGM = 42; // F#1
    inline constexpr int OpenHiHatNoteGM = 46; // A#1 (GM Open Hi-Hat)

    // 高領域ドラムノート判定 (チャンネル非依存で常にドラム扱い)
    inline bool isHighDrumNote (int n)
    {
        return n == KickNote || n == SnareNote || n == HiHatNote || n == ClapNote
            || n == OpenHiHatNote;
    }

    // GM ドラムノート判定 (channel 10 でのみドラム扱いに使う)
    inline bool isGMDrumNote (int n)
    {
        return n == KickNoteGM || n == SnareNoteGM || n == HiHatNoteGM || n == ClapNoteGM
            || n == OpenHiHatNoteGM;
    }

    // チャンネル考慮済みのドラム判定
    //   channel: 1-16 (juce::MidiMessage::getChannel() の戻り値)
    //   方針: channel 10 = ドラム専用、それ以外 = 全て Bass
    //   → C5/D5/D#5 等の高領域も ch 1 なら Bass として鳴る
    inline bool isDrumNoteForChannel (int n, int channel)
    {
        if (channel != 10) return false;                       // ch 10 以外は全て Bass
        return isHighDrumNote (n) || isGMDrumNote (n);          // ch 10 は高/低どちらも drum
    }

    // 後方互換: 旧 isDrumNote(n) - チャンネル無視で「ノート番号だけ見た」判定
    // (古いコードや UI 側で使われていれば残す)
    inline bool isDrumNote (int n)
    {
        return isHighDrumNote (n) || isGMDrumNote (n);
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using P = juce::AudioParameterFloat;
        using C = juce::AudioParameterChoice;
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        // Bass
        layout.add (std::make_unique<C>     (juce::ParameterID { BassWave, 1 }, "Bass Wave",
                                             juce::StringArray { "Saw", "Square" }, 0));
        layout.add (std::make_unique<P>     (juce::ParameterID { Cutoff,  1 }, "Cutoff",
                                             juce::NormalisableRange<float> (100.0f, 5000.0f, 1.0f, 0.35f),
                                             800.0f));
        // RESO レンジを HTML 版 (Tone.js Q) と揃える: 0-10 表示、内部で /10 して LadderFilter (0-0.99) に渡す
        layout.add (std::make_unique<P>     (juce::ParameterID { Reso,    1 }, "Resonance",
                                             juce::NormalisableRange<float> (0.0f, 10.0f, 0.05f),  8.0f));
        layout.add (std::make_unique<P>     (juce::ParameterID { EnvMod,  1 }, "Env Mod",
                                             juce::NormalisableRange<float> (0.0f, 6000.0f, 1.0f, 0.5f),
                                             3000.0f));
        layout.add (std::make_unique<P>     (juce::ParameterID { BassDec, 1 }, "Bass Decay",
                                             juce::NormalisableRange<float> (0.05f, 1.5f, 0.001f, 0.5f),
                                             0.300f));
        layout.add (std::make_unique<P>     (juce::ParameterID { AccAmt,  1 }, "Accent",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.500f));

        // Drums
        layout.add (std::make_unique<P>     (juce::ParameterID { KickDec,  1 }, "Kick Decay",
                                             juce::NormalisableRange<float> (0.05f, 1.5f, 0.001f, 0.5f),
                                             0.200f));
        layout.add (std::make_unique<P>     (juce::ParameterID { SnareDec, 1 }, "Snare Decay",
                                             juce::NormalisableRange<float> (0.05f, 0.6f, 0.001f, 0.7f),
                                             0.147f));
        layout.add (std::make_unique<P>     (juce::ParameterID { HhDec,    1 }, "HiHat Decay",
                                             juce::NormalisableRange<float> (0.01f, 0.4f, 0.001f, 0.5f),
                                             0.042f));

        // FX
        layout.add (std::make_unique<P>     (juce::ParameterID { DelMix,     1 }, "Delay",
                                             juce::NormalisableRange<float> (0.0f, 0.6f, 0.001f),   0.15f));
        layout.add (std::make_unique<P>     (juce::ParameterID { RevMix,     1 }, "Reverb",
                                             juce::NormalisableRange<float> (0.0f, 0.6f, 0.001f),   0.1f));
        layout.add (std::make_unique<C>     (juce::ParameterID { RevMode,    1 }, "Reverb Mode",
                                             juce::StringArray { "Plate", "Room", "Hall" }, 1));
        layout.add (std::make_unique<P>     (juce::ParameterID { Dist,       1 }, "Distortion",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),   0.0f));
        layout.add (std::make_unique<P>     (juce::ParameterID { MasterGain, 1 }, "Master",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),   0.7f));

        // Transport
        layout.add (std::make_unique<P>     (juce::ParameterID { Bpm,   1 }, "BPM",
                                             juce::NormalisableRange<float> (60.0f, 200.0f, 0.1f), 128.0f));
        layout.add (std::make_unique<P>     (juce::ParameterID { Swing, 1 }, "Swing",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.0f));

        // ─ 個別音色 ─
        // Kick
        layout.add (std::make_unique<P>     (juce::ParameterID { KickPitch, 1 }, "Kick Pitch",
                                             juce::NormalisableRange<float> (30.0f, 140.0f, 0.5f, 0.55f), 55.0f));
        layout.add (std::make_unique<P>     (juce::ParameterID { KickAmt,   1 }, "Kick Amount",
                                             juce::NormalisableRange<float> (0.0f, 8.0f, 0.05f),   3.80f));
        layout.add (std::make_unique<P>     (juce::ParameterID { KickDrive, 1 }, "Kick Drive",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.281f));
        // Snare
        layout.add (std::make_unique<P>     (juce::ParameterID { SnareNoise, 1 }, "Snare Noise",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.050f));
        layout.add (std::make_unique<P>     (juce::ParameterID { SnareTone,  1 }, "Snare Tone",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.150f));
        layout.add (std::make_unique<P>     (juce::ParameterID { SnareSnap,  1 }, "Snare Snap",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.478f));
        layout.add (std::make_unique<P>     (juce::ParameterID { SnareCrisp, 1 }, "Snare Crisp",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.0f));
        // HiHat
        layout.add (std::make_unique<P>     (juce::ParameterID { HhFreq,  1 }, "HiHat Freq",
                                             juce::NormalisableRange<float> (3000.0f, 14000.0f, 1.0f, 0.5f), 8500.0f));
        layout.add (std::make_unique<P>     (juce::ParameterID { HhQ,     1 }, "HiHat Q",
                                             juce::NormalisableRange<float> (1.0f, 12.0f, 0.05f),  2.15f));
        layout.add (std::make_unique<P>     (juce::ParameterID { HhMetal, 1 }, "HiHat Metal",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.300f));
        layout.add (std::make_unique<P>     (juce::ParameterID { HhDrive, 1 }, "HiHat Drive",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.0f));
        // Clap
        layout.add (std::make_unique<P>     (juce::ParameterID { ClapFreq,   1 }, "Clap Freq",
                                             juce::NormalisableRange<float> (500.0f, 3000.0f, 1.0f, 0.5f), 1100.0f));
        layout.add (std::make_unique<P>     (juce::ParameterID { ClapQ,      1 }, "Clap Q",
                                             juce::NormalisableRange<float> (1.0f, 8.0f, 0.05f),   1.20f));
        layout.add (std::make_unique<P>     (juce::ParameterID { ClapDec,    1 }, "Clap Decay",
                                             juce::NormalisableRange<float> (0.03f, 0.4f, 0.001f, 0.6f), 0.143f));
        layout.add (std::make_unique<P>     (juce::ParameterID { ClapSpread, 1 }, "Clap Spread",
                                             juce::NormalisableRange<float> (0.005f, 0.05f, 0.0005f), 0.0315f));
        layout.add (std::make_unique<P>     (juce::ParameterID { ClapStyle, 1 }, "Clap Style",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.0f));
        // Bass extras
        layout.add (std::make_unique<P>     (juce::ParameterID { BassGlide, 1 }, "Bass Glide",
                                             juce::NormalisableRange<float> (0.0f, 0.2f, 0.001f, 0.5f),  0.002f));
        layout.add (std::make_unique<P>     (juce::ParameterID { BassDrive, 1 }, "Bass Drive",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.357f));
        layout.add (std::make_unique<P>     (juce::ParameterID { BassSub,   1 }, "Bass Sub",
                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),  0.050f));

        // ─ 個別 Vol (ミックス) ─
        // 全 Vol ノブ統一仕様:
        //   range = [0 〜 default × 4]
        //   → デフォルトが約 25% 位置 (9〜10 時方向) に揃う
        //   → 各パートを +12dB までブースト可能 (リミッタが過大入力を保護)
        //   → 完全に絞ることもできる (= 0)
        auto volR = [] (float def) { return juce::NormalisableRange<float> (0.0f, def * 4.0f, 0.001f); };
        layout.add (std::make_unique<P> (juce::ParameterID { KickVol,  1 }, "Kick Vol",  volR (0.200f), 0.200f));
        layout.add (std::make_unique<P> (juce::ParameterID { SnareVol, 1 }, "Snare Vol", volR (0.300f), 0.300f));
        layout.add (std::make_unique<P> (juce::ParameterID { HhVol,    1 }, "HiHat Vol", volR (0.750f), 0.750f));
        layout.add (std::make_unique<P> (juce::ParameterID { ClapVol,  1 }, "Clap Vol",  volR (0.500f), 0.500f));
        layout.add (std::make_unique<P> (juce::ParameterID { BassVol,  1 }, "Bass Vol",  volR (0.170f), 0.170f));

        // ─ Pan (-1.0 = L / 0 = center / +1.0 = R)、constant-power で適用 ─
        auto panR = juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f);
        layout.add (std::make_unique<P> (juce::ParameterID { KickPan,  1 }, "Kick Pan",  panR, 0.0f));
        layout.add (std::make_unique<P> (juce::ParameterID { SnarePan, 1 }, "Snare Pan", panR, -0.15f));
        layout.add (std::make_unique<P> (juce::ParameterID { HhPan,    1 }, "HiHat Pan", panR, 0.25f));
        layout.add (std::make_unique<P> (juce::ParameterID { ClapPan,  1 }, "Clap Pan",  panR, -0.25f));
        layout.add (std::make_unique<P> (juce::ParameterID { BassPan,  1 }, "Bass Pan",  panR, 0.0f));

        // ─ Sidechain (Kick → Bass duck) ─
        // 0 = 無効、1.0 = 完全に Bass を duck
        layout.add (std::make_unique<P> (juce::ParameterID { ScAmt, 1 }, "SC Amount",
                                         juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
        layout.add (std::make_unique<P> (juce::ParameterID { ScRel, 1 }, "SC Release",
                                         juce::NormalisableRange<float> (0.02f, 0.5f, 0.001f, 0.5f), 0.15f));

        // ─ HiHat Open Decay (closed の hhDec とは別) ─
        layout.add (std::make_unique<P> (juce::ParameterID { HhOpenDec, 1 }, "HiHat Open Dec",
                                         juce::NormalisableRange<float> (0.05f, 1.5f, 0.001f, 0.5f), 0.400f));

        // ─ Master Bus Compressor ─
        // Threshold 0 dB がデフォルト = 実質透過 (opt-in でユーザが下げる)。
        layout.add (std::make_unique<P> (juce::ParameterID { CompThresh, 1 }, "Comp Threshold",
                                         juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), 0.0f));
        layout.add (std::make_unique<P> (juce::ParameterID { CompRatio,  1 }, "Comp Ratio",
                                         juce::NormalisableRange<float> (1.0f, 20.0f, 0.05f, 0.5f), 4.0f));
        layout.add (std::make_unique<P> (juce::ParameterID { CompAtk,    1 }, "Comp Attack",
                                         juce::NormalisableRange<float> (0.1f, 50.0f, 0.05f, 0.4f), 5.0f));
        layout.add (std::make_unique<P> (juce::ParameterID { CompRel,    1 }, "Comp Release",
                                         juce::NormalisableRange<float> (10.0f, 500.0f, 1.0f, 0.5f), 100.0f));

        // ─ Bass LFO ─
        // Depth 0 がデフォルト = 実質透過。Target はデフォルト Cutoff。
        layout.add (std::make_unique<P> (juce::ParameterID { BassLfoRate,  1 }, "Bass LFO Rate",
                                         juce::NormalisableRange<float> (0.05f, 20.0f, 0.01f, 0.4f), 2.0f));
        layout.add (std::make_unique<P> (juce::ParameterID { BassLfoDepth, 1 }, "Bass LFO Depth",
                                         juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
        layout.add (std::make_unique<C> (juce::ParameterID { BassLfoTarget, 1 }, "Bass LFO Target",
                                         juce::StringArray { "Cutoff", "Pitch" }, 0));

        // ─ Bass Chord (root に対して自動でハーモニーを追加)
        layout.add (std::make_unique<C> (juce::ParameterID { BassChord, 1 }, "Bass Chord",
                                         juce::StringArray { "Off", "+Oct", "Power", "Maj", "Min", "Sus4" }, 0));

        return layout;
    }
} // namespace PacificParams
