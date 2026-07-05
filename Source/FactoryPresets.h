#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <functional>
#include "PatternState.h"
#include "Params.h"

// ─────────────────────────────────────────────────────────────
//  FactoryPresets
//  バイナリに焼き込んだ工場出荷時プリセット。
//  各プリセットは apply lambda で:
//    1. 全パラメータをデフォルトにリセット
//    2. 自身固有のパラメータを上書き
//    3. パターン (drum + bass) を構築
//  これにより前 preset の残骸が leak しない。
// ─────────────────────────────────────────────────────────────
namespace FactoryPresets
{
    struct Preset
    {
        juce::String name;
        juce::String category;   // "Init" / "House" / "Techno" / "Acid" / "Dub" / "DnB" / "Chill" / "Other"
        std::function<void (juce::AudioProcessor& proc,
                            juce::AudioProcessorValueTreeState& apvts,
                            PatternState& pattern)> apply;
    };

    // メニュー表示順 (左の category がメニュー上で先に来る)
    inline const std::vector<juce::String>& categoryOrder()
    {
        static const std::vector<juce::String> order {
            "Init", "Machines", "House", "Techno", "Acid", "Dub",
            "DnB", "Breaks", "IDM", "Chill", "Other"
        };
        return order;
    }

    // ─ 内部ヘルパー ─
    namespace detail
    {
        // 全パラメータをデフォルト値へ
        inline void resetAll (juce::AudioProcessor& proc)
        {
            for (auto* p : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    r->setValueNotifyingHost (r->getDefaultValue());
        }

        // パラメータ id を 「人間が読む値」 で set (内部で convertTo0to1)
        inline void setP (juce::AudioProcessorValueTreeState& a, const char* id, float v)
        {
            if (auto* p = a.getParameter (juce::String (id)))
            {
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                    r->setValueNotifyingHost (r->convertTo0to1 (v));
            }
        }

        // ドラムセル: 複数 step を一括 ON (velocity は共通)
        inline void drum (PatternState& pat, int row,
                          std::initializer_list<int> steps, float vel = 1.0f)
        {
            for (int s : steps)
                pat.setDrum (row, s, { true, vel });
        }

        // ベースセル
        inline void bass (PatternState& pat, int step,
                          int note, int octave,
                          bool accent = false, bool slide = false)
        {
            pat.setBass (step, { true, note, octave, accent, slide });
        }

        constexpr int K = 0, S = 1, H = 2, C = 3;  // drum row index
    }

    // ─ プリセット定義 ─
    inline std::vector<Preset> all()
    {
        using namespace detail;
        std::vector<Preset> p;

        // ─ 01: Init ─ (デフォルト + 空パターン)
        p.push_back ({ "01 Init", "Init",
            [] (auto& proc, auto&, auto& pat)
            {
                resetAll (proc);
                pat.clear();
            }
        });

        // ─ 01: Acid Tech 128 — Hardfloor "Acperience 1" 風 (134 BPM、極限の 303) ─
        p.push_back ({ "01 Acid Tech 128", "Acid",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        134.0f);
                setP (a, PacificParams::KickPitch,  46.0f);
                setP (a, PacificParams::KickDec,    0.24f);   // 134 BPM 4分にタイト
                setP (a, PacificParams::KickDrive,  0.25f);  // 抑えめで thump
                setP (a, PacificParams::KickVol,    0.26f);
                // Bass: 極限まで squelchy
                setP (a, PacificParams::BassWave,   0.0f);   // Saw
                setP (a, PacificParams::Cutoff,     480.0f);
                setP (a, PacificParams::Reso,       9.5f);   // 自己発振寸前
                setP (a, PacificParams::EnvMod,     5500.0f);
                setP (a, PacificParams::BassDec,    0.16f);
                setP (a, PacificParams::AccAmt,     0.85f);
                setP (a, PacificParams::BassGlide,  0.045f);
                setP (a, PacificParams::BassDrive,  0.75f); // 歪んだ Saw
                setP (a, PacificParams::BassSub,    0.10f);
                // FX
                setP (a, PacificParams::DelMix,     0.22f);
                setP (a, PacificParams::RevMix,     0.108f);   // Hardfloor の signature 残響
                setP (a, PacificParams::Dist,       0.35f);
                // SC + Comp
                setP (a, PacificParams::ScAmt,      0.45f);
                setP (a, PacificParams::ScRel,      0.11f);
                setP (a, PacificParams::CompThresh, -14.0f);
                setP (a, PacificParams::CompRatio,  4.0f);
                setP (a, PacificParams::SnareCrisp, 0.45f);    // 適度にブライト
                setP (a, PacificParams::HhDrive,    0.25f);    // 軽くアグレッシブ
                setP (a, PacificParams::RevMode,    0.0f);     // Plate (techno タイト)

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 0, 2, 4, 6, 8, 10, 12, 14 }, 0.75f);
                // 303 line: スライドを多用 (Hardfloor 風の波打ち)
                bass (pat, 0,  0, 2, true,  false);
                bass (pat, 1,  0, 2, false, true);
                bass (pat, 3,  3, 2, false, true);
                bass (pat, 4,  0, 3, false, true);   // 高音へジャンプ
                bass (pat, 6,  7, 1, true,  false);
                bass (pat, 7,  7, 1, false, true);
                bass (pat, 8,  0, 2, true,  false);
                bass (pat, 10, 10, 1, false, true);
                bass (pat, 11, 0, 2, false, true);
                bass (pat, 13, 3, 2, true,  false);
                bass (pat, 14, 7, 1, false, true);
                bass (pat, 15, 0, 3, false, true);
            }
        });

        // ─ 01: Deep House 122 — Larry Heard "Can You Feel It" 風 (118 BPM、warm) ─
        p.push_back ({ "01 Deep House 122", "House",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        118.0f);
                // 柔らかく深いキック
                setP (a, PacificParams::KickPitch,  45.0f);
                setP (a, PacificParams::KickDec,    0.38f);
                setP (a, PacificParams::KickDrive,  0.08f);
                // Bass: 太く・暗く・サブ多め (Mr Fingers の質感)
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     360.0f);
                setP (a, PacificParams::Reso,       2.5f);
                setP (a, PacificParams::EnvMod,     1200.0f);
                setP (a, PacificParams::BassDec,    0.55f);
                setP (a, PacificParams::BassSub,    0.50f);
                setP (a, PacificParams::BassDrive,  0.10f);
                setP (a, PacificParams::BassGlide,  0.04f);
                // 広い空間
                setP (a, PacificParams::DelMix,     0.28f);
                setP (a, PacificParams::RevMix,     0.192f);
                // ふわっとした SC
                setP (a, PacificParams::ScAmt,      0.40f);
                setP (a, PacificParams::ScRel,      0.22f);
                setP (a, PacificParams::CompThresh, -10.0f);
                setP (a, PacificParams::CompRatio,  3.0f);
                setP (a, PacificParams::SnareCrisp, 0.10f);    // やわらかいまま
                setP (a, PacificParams::ClapStyle,  0.5f);     // 707 (2 bursts) 風で深い
                setP (a, PacificParams::HhDrive,    0.0f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall (deep house 空間)

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, C, { 4, 12 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.55f);
                bass (pat, 0,  0, 1, true,  false);
                bass (pat, 6,  7, 1, false, true);
                bass (pat, 8,  0, 1);
                bass (pat, 11, 5, 1);
                bass (pat, 14, 10, 1, false, true);
            }
        });

        // ─ 01: Techno 135 — Jeff Mills "The Bells" 風 (138 BPM、ドライで攻撃的) ─
        p.push_back ({ "01 Techno 135", "Techno",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        138.0f);
                // 太く深いキック (Jeff Mills 風の punch、タイト)
                setP (a, PacificParams::KickPitch,  46.0f);
                setP (a, PacificParams::KickAmt,    4.0f);    // ★控えめなクリック
                setP (a, PacificParams::KickDec,    0.24f);   // タイトに
                setP (a, PacificParams::KickDrive,  0.30f);   // ★大幅減
                setP (a, PacificParams::KickVol,    0.28f);   // 少しブースト
                // HiHat: ドライで刺さる
                setP (a, PacificParams::HhFreq,     9500.0f);
                setP (a, PacificParams::HhQ,        3.0f);
                setP (a, PacificParams::HhDec,      0.035f);
                // Bass: 最小限のサブ
                setP (a, PacificParams::BassWave,   1.0f);
                setP (a, PacificParams::Cutoff,     280.0f);
                setP (a, PacificParams::Reso,       1.0f);
                setP (a, PacificParams::EnvMod,     400.0f);
                setP (a, PacificParams::BassDec,    0.18f);
                setP (a, PacificParams::BassSub,    0.55f);
                setP (a, PacificParams::BassDrive,  0.20f);
                // Jeff Mills 風のドライ気味だが、最小限の空間あり
                setP (a, PacificParams::DelMix,     0.10f);
                setP (a, PacificParams::RevMix,     0.06f);
                setP (a, PacificParams::Dist,       0.10f);
                // 適正コンプ (kick punch を保護)
                setP (a, PacificParams::CompThresh, -15.0f);  // ★緩和
                setP (a, PacificParams::CompRatio,  4.0f);    // ★緩和
                setP (a, PacificParams::CompAtk,    8.0f);    // ★大幅遅く
                setP (a, PacificParams::CompRel,    70.0f);
                setP (a, PacificParams::ScAmt,      0.55f);
                setP (a, PacificParams::ScRel,      0.12f);
                setP (a, PacificParams::SnareCrisp, 0.55f);    // 909 風クリスピー
                setP (a, PacificParams::HhDrive,    0.35f);    // ドライでアグレッシブ
                setP (a, PacificParams::RevMode,    0.0f);     // Plate (Jeff Mills ドライ)

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, H, { 0, 2, 4, 6, 8, 10, 12, 14 }, 0.55f);
                bass (pat, 0,  0, 1);
                bass (pat, 8,  0, 1, true, false);
            }
        });

        // ─ 01: Ambient 70 — Brian Eno "Music for Airports" 風 (60 BPM、ほぼドラム無し) ─
        p.push_back ({ "01 Ambient 70", "Chill",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        60.0f);
                // ふんわりキック (ほぼ呼吸音、Eno 風の長尾)
                setP (a, PacificParams::KickPitch,  38.0f);
                setP (a, PacificParams::KickDec,    0.85f);   // 長くテクスチャ寄り
                setP (a, PacificParams::KickDrive,  0.0f);
                // Bass: 持続的、ドリーミー
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     650.0f);
                setP (a, PacificParams::Reso,       1.5f);
                setP (a, PacificParams::EnvMod,     500.0f);
                setP (a, PacificParams::BassDec,    1.50f);  // 最大近く
                setP (a, PacificParams::BassGlide,  0.12f);
                setP (a, PacificParams::BassSub,    0.55f);
                setP (a, PacificParams::BassDrive,  0.0f);
                // ゆっくりした cutoff LFO (Eno 風のたゆたい)
                setP (a, PacificParams::BassLfoRate,   0.18f);
                setP (a, PacificParams::BassLfoDepth,  0.55f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // 巨大な空間 (Reverb 主役、Delay は控えめに)
                setP (a, PacificParams::DelMix,     0.30f);
                setP (a, PacificParams::RevMix,     0.348f);  // ほぼマックス
                setP (a, PacificParams::Dist,       0.0f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall (Eno の大空間)
                // Crisp 0 (Ambient はスネアほぼ使わないが lo-fi が合う)

                pat.clear();
                // ドラムは超控えめに、4拍に 1 つだけ
                drum (pat, K, { 0 }, 0.5f);
                drum (pat, H, { 10 }, 0.25f);
                // Bass: 長いドローン的なノート
                bass (pat, 0,  0, 2);                          // C2 (ロング)
                bass (pat, 8,  7, 1, false, true);             // G1 にスライド
            }
        });

        // ─ 02: Industrial 140 — NIN "Head Like a Hole" 風 (118 BPM、極度に歪んだ) ─
        p.push_back ({ "02 Industrial 140", "Techno",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        118.0f);
                // ドカドカしたキック (NIN の重低音、drive 控えめで thump)
                setP (a, PacificParams::KickPitch,  44.0f);
                setP (a, PacificParams::KickAmt,    4.0f);
                setP (a, PacificParams::KickDec,    0.40f);   // 長めで body 重視
                setP (a, PacificParams::KickDrive,  0.35f);   // ★大幅減
                setP (a, PacificParams::KickVol,    0.32f);   // キックを目立たせる
                // 刺さるスネア
                setP (a, PacificParams::SnareSnap,  0.9f);
                setP (a, PacificParams::SnareNoise, 0.7f);
                setP (a, PacificParams::SnareTone,  0.5f);
                setP (a, PacificParams::SnareDec,   0.18f);
                // Bass: 中庸な歪みでキックと共存
                setP (a, PacificParams::BassWave,   1.0f);
                setP (a, PacificParams::Cutoff,     1100.0f);
                setP (a, PacificParams::Reso,       6.0f);
                setP (a, PacificParams::EnvMod,     3000.0f);
                setP (a, PacificParams::BassDec,    0.25f);
                setP (a, PacificParams::BassDrive,  0.55f);   // ★大幅減
                setP (a, PacificParams::BassSub,    0.15f);
                setP (a, PacificParams::BassVol,    0.15f);
                // LFO で機械的な動き
                setP (a, PacificParams::BassLfoRate,   4.5f);
                setP (a, PacificParams::BassLfoDepth,  0.40f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // FX: 中庸な歪み
                setP (a, PacificParams::Dist,       0.40f);   // ★緩和
                setP (a, PacificParams::DelMix,     0.15f);
                setP (a, PacificParams::RevMix,     0.108f);
                // 適正コンプ (kick トランジェントを残す)
                setP (a, PacificParams::CompThresh, -16.0f);  // ★緩和
                setP (a, PacificParams::CompRatio,  5.0f);    // ★緩和
                setP (a, PacificParams::CompAtk,    8.0f);    // ★大幅遅く
                setP (a, PacificParams::CompRel,    70.0f);
                setP (a, PacificParams::ScAmt,      0.70f);
                setP (a, PacificParams::ScRel,      0.12f);
                setP (a, PacificParams::SnareCrisp, 0.65f);    // NIN 風シャープ
                setP (a, PacificParams::HhDrive,    0.55f);    // 荒い・歪んだ HH
                setP (a, PacificParams::RevMode,    0.0f);     // Plate (NIN 風のシャープな空間)

                pat.clear();
                // ロック寄りのバックビート + キック追加
                drum (pat, K, { 0, 6, 8, 14 });
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 0, 2, 4, 6, 8, 10, 12, 14 }, 0.55f);
                // ベース: ロック風の単純ライン (NIN の "Head Like" の硬さ)
                bass (pat, 0,  0, 1, true,  false);
                bass (pat, 2,  0, 1);
                bass (pat, 4,  0, 1, true,  false);
                bass (pat, 6,  3, 1);
                bass (pat, 8,  0, 1, true,  false);
                bass (pat, 10, 0, 1);
                bass (pat, 12, 3, 1, true,  false);
                bass (pat, 14, 5, 1);
            }
        });

        // ─ 03: Dub Techno 128 — Basic Channel "Phylyps Trak" 風 (125 BPM、最大ディレイ) ─
        p.push_back ({ "03 Dub Techno 128", "Techno",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        125.0f);
                // ミニマルキック、深く
                setP (a, PacificParams::KickPitch,  42.0f);
                setP (a, PacificParams::KickDec,    0.45f);
                setP (a, PacificParams::KickDrive,  0.12f);
                // Bass: 持続的、超ローパス (chord-stab 風)
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     280.0f);  // すごく暗い
                setP (a, PacificParams::Reso,       2.0f);
                setP (a, PacificParams::EnvMod,     900.0f);
                setP (a, PacificParams::BassDec,    1.10f);   // ロング
                setP (a, PacificParams::BassGlide,  0.15f);
                setP (a, PacificParams::BassSub,    0.35f);
                setP (a, PacificParams::BassDrive,  0.10f);   // テープ風グリット
                // ゆっくりした cutoff うねり
                setP (a, PacificParams::BassLfoRate,   0.25f);
                setP (a, PacificParams::BassLfoDepth,  0.35f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // 最大級のディレイ + リバーブ (dub の核)
                setP (a, PacificParams::DelMix,     0.55f);
                setP (a, PacificParams::RevMix,     0.33f);
                setP (a, PacificParams::Dist,       0.08f);
                // ふわっとした SC
                setP (a, PacificParams::ScAmt,      0.35f);
                setP (a, PacificParams::ScRel,      0.30f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall (dub の象徴)
                // Snare ほぼ使わない、Crisp は default 0 のまま (lo-fi)

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 }, 0.85f);
                drum (pat, H, { 6 }, 0.35f);                   // たった 1 つの HH
                // ベース: 持続的なドローン
                bass (pat, 0,  0, 1);
                bass (pat, 8,  3, 1, false, true);
            }
        });

        // ─ 02: UK Garage 132 — Artful Dodger "Re-Rewind" 風 (135 BPM、強いシャッフル) ─
        p.push_back ({ "02 UK Garage 132", "House",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        135.0f);
                setP (a, PacificParams::Swing,      0.65f);    // 強いシャッフル
                // クリスピーなキック (UK garage 寄り、低音重視)
                setP (a, PacificParams::KickPitch,  50.0f);
                setP (a, PacificParams::KickDec,    0.22f);
                setP (a, PacificParams::KickDrive,  0.20f);
                // クリスピーな HiHat
                setP (a, PacificParams::HhFreq,     10000.0f);
                setP (a, PacificParams::HhDec,      0.035f);
                setP (a, PacificParams::HhQ,        4.0f);
                // 跳ねるベース、明るめ
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     900.0f);
                setP (a, PacificParams::Reso,       3.5f);
                setP (a, PacificParams::EnvMod,     2500.0f);
                setP (a, PacificParams::BassDec,    0.22f);
                setP (a, PacificParams::BassGlide,  0.012f);
                setP (a, PacificParams::BassSub,    0.30f);
                setP (a, PacificParams::BassDrive,  0.20f);
                setP (a, PacificParams::DelMix,     0.22f);
                setP (a, PacificParams::RevMix,     0.108f);
                setP (a, PacificParams::CompThresh, -13.0f);
                setP (a, PacificParams::CompRatio,  4.0f);
                setP (a, PacificParams::ScAmt,      0.30f);
                setP (a, PacificParams::SnareCrisp, 0.70f);    // UK garage はパキッと
                setP (a, PacificParams::HhDrive,    0.20f);
                setP (a, PacificParams::RevMode,    0.0f);     // Plate (タイトな空間)

                pat.clear();
                drum (pat, K, { 0, 10 });                      // 2-step 配置
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 2, 5, 6, 9, 10, 13, 14 }, 0.6f);  // 不規則
                bass (pat, 0,  0, 1, true,  false);
                bass (pat, 2,  3, 1);
                bass (pat, 5,  7, 1, false, true);
                bass (pat, 7,  10, 1);
                bass (pat, 10, 0, 1, true,  false);
                bass (pat, 13, 5, 1, false, true);
            }
        });

        // ─ 01: Drum & Bass 174 — Goldie "Inner City Life" 風 (168 BPM、深いサブ + 空間) ─
        p.push_back ({ "01 Drum & Bass 174", "DnB",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        168.0f);
                // タイトなキック
                setP (a, PacificParams::KickPitch,  48.0f);
                setP (a, PacificParams::KickDec,    0.14f);
                setP (a, PacificParams::KickDrive,  0.30f);
                // スネア: パンチあるがあまり歪まない (リキッド DnB)
                setP (a, PacificParams::SnareSnap,  0.65f);
                setP (a, PacificParams::SnareTone,  0.30f);
                setP (a, PacificParams::SnareDec,   0.16f);
                // HH 細かく
                setP (a, PacificParams::HhDec,      0.040f);
                setP (a, PacificParams::HhFreq,     9000.0f);
                // Bass: 重低音重視、サブ多め、長い (Reese 風)
                setP (a, PacificParams::BassWave,   1.0f);
                setP (a, PacificParams::Cutoff,     420.0f);
                setP (a, PacificParams::Reso,       2.5f);
                setP (a, PacificParams::EnvMod,     1200.0f);
                setP (a, PacificParams::BassDec,    0.85f);   // ロング
                setP (a, PacificParams::BassSub,    0.70f);   // サブ重視
                setP (a, PacificParams::BassDrive,  0.25f);
                setP (a, PacificParams::BassGlide,  0.025f);
                // FX: atmospheric な空間 (Inner City Life の浮遊感、Liquid DnB の核)
                setP (a, PacificParams::Dist,       0.12f);
                setP (a, PacificParams::DelMix,     0.25f);
                setP (a, PacificParams::RevMix,     0.24f);   // しっかり浮遊感
                // 軽めのコンプ (息のあるダイナミクス)
                setP (a, PacificParams::CompThresh, -14.0f);
                setP (a, PacificParams::CompRatio,  4.0f);
                setP (a, PacificParams::ScAmt,      0.30f);
                setP (a, PacificParams::SnareCrisp, 0.55f);    // 細かい breakbeat にキレ
                setP (a, PacificParams::HhDrive,    0.25f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall (Inner City Life の atmospheric)

                pat.clear();
                drum (pat, K, { 0, 10 });
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 0, 2, 4, 6, 8, 10, 12, 14 }, 0.55f);
                drum (pat, H, { 3, 7, 11, 15 }, 0.35f);        // 16分ゴースト
                // Bass: ロング・ノートで Reese 風
                bass (pat, 0,  0, 1, true,  false);
                bass (pat, 8,  5, 1, false, true);             // F1 にスライド
            }
        });

        // ─ 01: Trance 138 — Robert Miles "Children" 風 (132 BPM、明るく広い) ─
        p.push_back ({ "01 Trance 138", "Other",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        132.0f);
                // 明るく軽快なキック (90s dream trance)
                setP (a, PacificParams::KickPitch,  46.0f);
                setP (a, PacificParams::KickDec,    0.26f);
                setP (a, PacificParams::KickDrive,  0.25f);
                // クラップ: open で広い
                setP (a, PacificParams::ClapDec,    0.18f);
                setP (a, PacificParams::ClapFreq,   1400.0f);
                // 明るいオープンな bass
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     2000.0f);  // ブライト
                setP (a, PacificParams::Reso,       3.0f);
                setP (a, PacificParams::EnvMod,     2000.0f);
                setP (a, PacificParams::BassDec,    0.45f);
                setP (a, PacificParams::BassGlide,  0.08f);
                setP (a, PacificParams::BassSub,    0.20f);
                setP (a, PacificParams::BassDrive,  0.12f);
                // 巨大な空間 (Children の伝説的リバーブ感)
                setP (a, PacificParams::DelMix,     0.32f);   // Hall reverb との balance
                setP (a, PacificParams::RevMix,     0.33f);
                setP (a, PacificParams::Dist,       0.05f);
                // SC ポンプは中庸 (アンセム的、暴力的ではない)
                setP (a, PacificParams::ScAmt,      0.55f);
                setP (a, PacificParams::ScRel,      0.18f);
                setP (a, PacificParams::CompThresh, -12.0f);
                setP (a, PacificParams::CompRatio,  3.5f);
                setP (a, PacificParams::SnareCrisp, 0.40f);    // 明るすぎず
                setP (a, PacificParams::ClapStyle,  1.0f);     // 909 (3 bursts) Trance 王道
                setP (a, PacificParams::HhDrive,    0.10f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall (Children 風の伝説的空間)

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, C, { 4, 12 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.55f);
                // ベース: 流れるような melodic line
                bass (pat, 0,  0, 1);
                bass (pat, 3,  0, 1, false, true);
                bass (pat, 4,  3, 1, false, true);
                bass (pat, 6,  7, 1);
                bass (pat, 8,  0, 1);
                bass (pat, 11, 10, 1, false, true);
                bass (pat, 12, 0, 2);                          // 高音へジャンプ
                bass (pat, 14, 7, 1, false, true);
            }
        });

        // ─ 02: Synthwave 110 — Kavinsky "Nightcall" 風 (88 BPM、ゆったり 80s) ─
        p.push_back ({ "02 Synthwave 110", "Chill",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        88.0f);
                // 80s ファットキック (Nightcall の長い余韻)
                setP (a, PacificParams::KickPitch,  48.0f);
                setP (a, PacificParams::KickDec,    0.48f);   // 80s らしい長い decay
                setP (a, PacificParams::KickDrive,  0.18f);
                // ゲートリバーブ風スネア (80s sig)
                setP (a, PacificParams::SnareDec,   0.42f);
                setP (a, PacificParams::SnareSnap,  0.50f);
                setP (a, PacificParams::SnareTone,  0.50f);
                setP (a, PacificParams::SnareNoise, 0.20f);
                // クローズドハイハットを Open で長めに
                setP (a, PacificParams::HhOpenDec,  0.50f);
                // Bass: 太い Square + ヘビーサブ (Nightcall の特徴的ベース)
                setP (a, PacificParams::BassWave,   1.0f);
                setP (a, PacificParams::Cutoff,     650.0f);
                setP (a, PacificParams::Reso,       2.0f);
                setP (a, PacificParams::EnvMod,     1200.0f);
                setP (a, PacificParams::BassDec,    0.70f);   // ロング
                setP (a, PacificParams::BassGlide,  0.08f);
                setP (a, PacificParams::BassSub,    0.55f);   // 深いサブ
                setP (a, PacificParams::BassDrive,  0.20f);
                // 軽いビブラート
                setP (a, PacificParams::BassLfoRate,   3.5f);
                setP (a, PacificParams::BassLfoDepth,  0.18f);
                setP (a, PacificParams::BassLfoTarget, 1.0f);
                // 巨大な空間 (80s wet)
                setP (a, PacificParams::DelMix,     0.35f);
                setP (a, PacificParams::RevMix,     0.33f);
                setP (a, PacificParams::Dist,       0.10f);
                // 80s tape の温かさを残す → Crisp 控えめ
                setP (a, PacificParams::SnareCrisp, 0.15f);
                setP (a, PacificParams::HhDrive,    0.0f);     // 80s クリーンな HH
                setP (a, PacificParams::RevMode,    2.0f);     // Hall (80s wet 巨大空間)

                pat.clear();
                drum (pat, K, { 0, 8 });                       // 遅い 4-on-floor
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.55f);
                // ベース: ノスタルジックな下行ライン
                bass (pat, 0,  0, 1);
                bass (pat, 4,  10, 0, false, true);            // A#0 (低い)
                bass (pat, 8,  7, 0);                          // G0
                bass (pat, 12, 5, 0, false, true);             // F0
            }
        });

        // ─ 04: Detroit Techno 130 — Derrick May "Strings of Life" 風 (125 BPM、ソウル) ─
        p.push_back ({ "04 Detroit Techno 130", "Techno",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        125.0f);
                // クリスピーな TR-909 風キック (深い)
                setP (a, PacificParams::KickPitch,  46.0f);
                setP (a, PacificParams::KickAmt,    4.5f);
                setP (a, PacificParams::KickDec,    0.30f);
                setP (a, PacificParams::KickDrive,  0.25f);
                // クラップ (909 風、明るく)
                setP (a, PacificParams::ClapFreq,   1500.0f);
                setP (a, PacificParams::ClapDec,    0.16f);
                setP (a, PacificParams::ClapQ,      1.8f);
                // Open HiHat 風: longer decay
                setP (a, PacificParams::HhOpenDec,  0.55f);
                // ウォームベース
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     700.0f);
                setP (a, PacificParams::Reso,       3.0f);
                setP (a, PacificParams::EnvMod,     2200.0f);
                setP (a, PacificParams::BassDec,    0.38f);
                setP (a, PacificParams::BassGlide,  0.04f);
                setP (a, PacificParams::BassDrive,  0.25f);
                setP (a, PacificParams::BassSub,    0.20f);
                // 暖かいスペース
                setP (a, PacificParams::DelMix,     0.25f);
                setP (a, PacificParams::RevMix,     0.168f);
                setP (a, PacificParams::Dist,       0.08f);
                // 中庸 SC + コンプ (息のあるグルーヴ)
                setP (a, PacificParams::ScAmt,      0.40f);
                setP (a, PacificParams::ScRel,      0.20f);
                setP (a, PacificParams::CompThresh, -11.0f);
                setP (a, PacificParams::CompRatio,  3.0f);
                setP (a, PacificParams::SnareCrisp, 0.45f);    // 909 風中庸
                setP (a, PacificParams::ClapStyle,  1.0f);     // 909 (3 bursts) — Detroit の象徴
                setP (a, PacificParams::HhDrive,    0.15f);
                setP (a, PacificParams::RevMode,    0.0f);     // Plate (Strings of Life クリスピー)

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, C, { 4, 12 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.60f);
                // ソウルフルなベースライン
                bass (pat, 0,  0, 1, true,  false);
                bass (pat, 3,  7, 1, false, true);
                bass (pat, 6,  3, 2);
                bass (pat, 8,  5, 1, true,  false);
                bass (pat, 11, 0, 2, false, true);
                bass (pat, 14, 7, 1);
            }
        });

        // ─ 02: Acid House 125 — Phuture "Acid Tracks" 風 (122 BPM、史上初の 303 用法) ─
        p.push_back ({ "02 Acid House 125", "Acid",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        122.0f);
                // シンプルな 808 風キック (低音重視)
                setP (a, PacificParams::KickPitch,  44.0f);
                setP (a, PacificParams::KickDec,    0.32f);
                setP (a, PacificParams::KickDrive,  0.20f);
                // クラップ
                setP (a, PacificParams::ClapFreq,   1300.0f);
                setP (a, PacificParams::ClapDec,    0.15f);
                // 原典 303 (Square、極限レゾ、低めの cutoff)
                setP (a, PacificParams::BassWave,   1.0f);
                setP (a, PacificParams::Cutoff,     420.0f);
                setP (a, PacificParams::Reso,       9.5f);     // ぶっ飛ぶレゾ
                setP (a, PacificParams::EnvMod,     5800.0f);
                setP (a, PacificParams::BassDec,    0.20f);
                setP (a, PacificParams::AccAmt,     0.90f);
                setP (a, PacificParams::BassGlide,  0.06f);    // スライド長め
                setP (a, PacificParams::BassDrive,  0.55f);
                setP (a, PacificParams::BassSub,    0.05f);
                // 最小限の FX (87年の lo-fi)
                setP (a, PacificParams::DelMix,     0.18f);
                setP (a, PacificParams::RevMix,     0.06f);
                setP (a, PacificParams::Dist,       0.22f);
                setP (a, PacificParams::ScAmt,      0.35f);
                // 87 年の lo-fi 感、Crisp は低めに
                setP (a, PacificParams::SnareCrisp, 0.20f);
                setP (a, PacificParams::ClapStyle,  0.5f);     // 707 (2 bursts) 87 年っぽい
                setP (a, PacificParams::HhDrive,    0.15f);
                setP (a, PacificParams::RevMode,    0.0f);     // Plate (87 年シンプル)

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, C, { 4, 12 });
                drum (pat, H, { 0, 2, 4, 6, 8, 10, 12, 14 }, 0.60f);
                // Phuture 風: スライド・アクセントの嵐
                bass (pat, 0,  0, 1, true,  false);
                bass (pat, 1,  0, 1, false, true);
                bass (pat, 2,  0, 2, false, true);             // 高ジャンプ
                bass (pat, 4,  3, 1, false, true);
                bass (pat, 5,  3, 1);
                bass (pat, 7,  7, 1, false, true);
                bass (pat, 8,  0, 1, true,  false);
                bass (pat, 9,  0, 2, false, true);
                bass (pat, 11, 10, 1, false, true);
                bass (pat, 12, 0, 1, true,  false);
                bass (pat, 13, 5, 1, false, true);
                bass (pat, 15, 7, 1, false, true);
            }
        });

        // ─ 05: Berlin Minimal 126 — Villalobos "Easy Lee" 風 (125 BPM、swing + 極ミニマル) ─
        p.push_back ({ "05 Berlin Minimal 126", "Techno",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        125.0f);
                setP (a, PacificParams::Swing,      0.58f);    // Villalobos のクセ swing
                // タイトなキック
                setP (a, PacificParams::KickPitch,  46.0f);
                setP (a, PacificParams::KickDec,    0.24f);
                setP (a, PacificParams::KickDrive,  0.12f);
                // 細かい HiHat
                setP (a, PacificParams::HhFreq,     10500.0f);
                setP (a, PacificParams::HhDec,      0.025f);
                setP (a, PacificParams::HhQ,        4.5f);
                // Bass: 超ミニマル、サブのみ
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     260.0f);
                setP (a, PacificParams::Reso,       1.2f);
                setP (a, PacificParams::EnvMod,     400.0f);
                setP (a, PacificParams::BassDec,    0.28f);
                setP (a, PacificParams::BassSub,    0.60f);
                setP (a, PacificParams::BassDrive,  0.05f);
                // ほぼ無音空間 (dub-y、ミニマルの肝)
                setP (a, PacificParams::DelMix,     0.18f);
                setP (a, PacificParams::RevMix,     0.09f);  // よりミニマルに
                setP (a, PacificParams::Dist,       0.0f);
                // タイトコンプ
                setP (a, PacificParams::CompThresh, -16.0f);
                setP (a, PacificParams::CompRatio,  5.0f);
                setP (a, PacificParams::ScAmt,      0.45f);
                setP (a, PacificParams::ScRel,      0.20f);
                // Snare はほぼ使わない、Crisp デフォルト 0
                setP (a, PacificParams::ClapStyle,  0.0f);     // 単発 (minimal)
                setP (a, PacificParams::HhDrive,    0.30f);    // tight な歪み

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                // 不規則な細かいパーカッション (Villalobos の特徴)
                drum (pat, H, { 3, 7, 11, 15 }, 0.45f);        // 裏拍だけ
                drum (pat, C, { 6 }, 0.50f);
                bass (pat, 0,  0, 1);
                bass (pat, 8,  0, 1);
            }
        });

        // ─ 06: Hard Techno 145 — Speedy J "Pull Over" 風 (142 BPM、暴力的) ─
        p.push_back ({ "06 Hard Techno 145", "Techno",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        142.0f);
                // ヘビーキック — 8分パターンに合わせてタイト (overlap 回避)
                setP (a, PacificParams::KickPitch,  42.0f);
                setP (a, PacificParams::KickAmt,    4.0f);    // 控えめなクリック
                setP (a, PacificParams::KickDec,    0.18f);   // ★タイト (142BPM/8分=211ms)
                setP (a, PacificParams::KickDrive,  0.30f);   // 低音保護
                setP (a, PacificParams::KickVol,    0.30f);   // キックを目立たせる
                // 刺さるスネア
                setP (a, PacificParams::SnareSnap,  0.85f);
                setP (a, PacificParams::SnareDec,   0.10f);
                setP (a, PacificParams::SnareNoise, 0.50f);
                // Bass: 中庸な歪みでキックと衝突しない
                setP (a, PacificParams::BassWave,   1.0f);
                setP (a, PacificParams::Cutoff,     650.0f);
                setP (a, PacificParams::Reso,       5.0f);
                setP (a, PacificParams::EnvMod,     3000.0f);
                setP (a, PacificParams::BassDec,    0.18f);
                setP (a, PacificParams::BassDrive,  0.50f);   // ★大幅減
                setP (a, PacificParams::BassVol,    0.14f);   // 少し下げてキックを引き立てる
                // 強烈な LFO (Pull Over の機械音)
                setP (a, PacificParams::BassLfoRate,   10.0f);
                setP (a, PacificParams::BassLfoDepth,  0.50f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // FX: 軽い歪みでテクスチャ
                setP (a, PacificParams::Dist,       0.30f);
                setP (a, PacificParams::DelMix,     0.18f);
                setP (a, PacificParams::RevMix,     0.072f);
                // 適正コンプ (attack を遅くしてキックのトランジェントを残す)
                setP (a, PacificParams::CompThresh, -16.0f);  // ★緩和
                setP (a, PacificParams::CompRatio,  5.0f);    // ★緩和
                setP (a, PacificParams::CompAtk,    10.0f);   // ★大幅遅く (kick 殺さない)
                setP (a, PacificParams::CompRel,    80.0f);
                // SC で bass を深くダックして kick を出す
                setP (a, PacificParams::ScAmt,      0.85f);
                setP (a, PacificParams::ScRel,      0.10f);
                setP (a, PacificParams::SnareCrisp, 0.75f);    // パキッと刺さる hard techno snare
                setP (a, PacificParams::HhDrive,    0.65f);    // 歪んで攻撃的
                setP (a, PacificParams::ClapStyle,  0.0f);     // 単発 (シャープ)
                setP (a, PacificParams::RevMode,    0.0f);     // Plate (Pull Over 鋭く)

                pat.clear();
                drum (pat, K, { 0, 2, 4, 6, 8, 10, 12, 14 });
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 1, 3, 5, 7, 9, 11, 13, 15 }, 0.75f);
                drum (pat, C, { 6, 14 }, 0.55f);
                bass (pat, 0,  0, 1, true,  false);
                bass (pat, 2,  0, 1);
                bass (pat, 4,  3, 1);
                bass (pat, 6,  3, 1, false, true);
                bass (pat, 8,  0, 1, true,  false);
                bass (pat, 10, 7, 1);
                bass (pat, 12, 0, 1, true,  false);
                bass (pat, 14, 10, 1, false, true);
            }
        });

        // ─ 01: Acid Dub 110 — スロー 303 + 深い delay/reverb (Crew 7 系) ─
        p.push_back ({ "01 Acid Dub 110", "Dub",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        110.0f);
                setP (a, PacificParams::KickPitch,  46.0f);
                setP (a, PacificParams::KickDec,    0.35f);
                setP (a, PacificParams::KickDrive,  0.15f);
                // 303 acid (柔らかめ、深い空間用)
                setP (a, PacificParams::BassWave,   0.0f);   // Saw
                setP (a, PacificParams::Cutoff,     450.0f);
                setP (a, PacificParams::Reso,       8.5f);
                setP (a, PacificParams::EnvMod,     4500.0f);
                setP (a, PacificParams::BassDec,    0.30f);
                setP (a, PacificParams::AccAmt,     0.70f);
                setP (a, PacificParams::BassGlide,  0.05f);
                setP (a, PacificParams::BassDrive,  0.50f);
                setP (a, PacificParams::BassSub,    0.15f);
                // ゆっくり cutoff うねり
                setP (a, PacificParams::BassLfoRate,   0.4f);
                setP (a, PacificParams::BassLfoDepth,  0.35f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // 深い dub テクスチャ
                setP (a, PacificParams::DelMix,     0.45f);
                setP (a, PacificParams::RevMix,     0.21f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall
                setP (a, PacificParams::Dist,       0.20f);
                setP (a, PacificParams::ScAmt,      0.50f);
                setP (a, PacificParams::ScRel,      0.20f);
                setP (a, PacificParams::CompThresh, -12.0f);
                setP (a, PacificParams::CompRatio,  3.5f);
                setP (a, PacificParams::SnareCrisp, 0.20f);

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.55f);
                // 303 線 (スライド多用、acid + dub のテクスチャ)
                bass (pat, 0,  0, 2, true,  false);
                bass (pat, 2,  0, 2, false, true);
                bass (pat, 4,  7, 1);
                bass (pat, 6,  10, 1, false, true);
                bass (pat, 8,  0, 2, true,  false);
                bass (pat, 11, 3, 2, false, true);
                bass (pat, 14, 5, 1);
            }
        });

        // ─ 03: Pacific Acid 95 — 海岸線テンポ、Synthwave × Acid ─
        p.push_back ({ "03 Pacific Acid 95", "Acid",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        95.0f);
                setP (a, PacificParams::KickPitch,  50.0f);
                setP (a, PacificParams::KickDec,    0.40f);
                setP (a, PacificParams::KickDrive,  0.10f);
                // Synthwave 風スネア (gate reverb)
                setP (a, PacificParams::SnareDec,   0.40f);
                setP (a, PacificParams::SnareSnap,  0.50f);
                setP (a, PacificParams::SnareTone,  0.45f);
                setP (a, PacificParams::SnareCrisp, 0.20f);
                // 303 線 + synthwave fat sub
                setP (a, PacificParams::BassWave,   0.0f);    // Saw
                setP (a, PacificParams::Cutoff,     700.0f);
                setP (a, PacificParams::Reso,       5.0f);
                setP (a, PacificParams::EnvMod,     2200.0f);
                setP (a, PacificParams::BassDec,    0.50f);
                setP (a, PacificParams::AccAmt,     0.65f);
                setP (a, PacificParams::BassGlide,  0.06f);
                setP (a, PacificParams::BassDrive,  0.30f);
                setP (a, PacificParams::BassSub,    0.35f);
                // ゆっくりサーフ風 LFO
                setP (a, PacificParams::BassLfoRate,   1.0f);
                setP (a, PacificParams::BassLfoDepth,  0.50f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // 広い空間
                setP (a, PacificParams::DelMix,     0.30f);
                setP (a, PacificParams::RevMix,     0.27f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall
                setP (a, PacificParams::Dist,       0.12f);

                pat.clear();
                drum (pat, K, { 0, 8 });
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.55f);
                bass (pat, 0,  0, 2);
                bass (pat, 4,  7, 1, false, true);
                bass (pat, 8,  5, 1);
                bass (pat, 12, 3, 2, false, true);
            }
        });

        // ─ 04: Hardfloor Acid 138 — 02 より攻撃的、Acperience 2 寄り ─
        p.push_back ({ "04 Hardfloor Acid 138", "Acid",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        138.0f);
                setP (a, PacificParams::KickPitch,  48.0f);
                setP (a, PacificParams::KickDec,    0.20f);
                setP (a, PacificParams::KickDrive,  0.40f);
                setP (a, PacificParams::KickVol,    0.28f);
                setP (a, PacificParams::SnareSnap,  0.85f);
                setP (a, PacificParams::SnareDec,   0.10f);
                setP (a, PacificParams::SnareCrisp, 0.55f);
                setP (a, PacificParams::HhDrive,    0.40f);
                // 303 攻撃 MAX
                setP (a, PacificParams::BassWave,   0.0f);   // Saw
                setP (a, PacificParams::Cutoff,     400.0f);
                setP (a, PacificParams::Reso,       9.8f);   // 自己発振寸前
                setP (a, PacificParams::EnvMod,     6000.0f); // MAX
                setP (a, PacificParams::BassDec,    0.15f);
                setP (a, PacificParams::AccAmt,     0.95f);
                setP (a, PacificParams::BassGlide,  0.04f);
                setP (a, PacificParams::BassDrive,  0.85f); // 歪み MAX 近く
                setP (a, PacificParams::BassSub,    0.05f);
                // 速い LFO で acid 暴れ
                setP (a, PacificParams::BassLfoRate,   5.0f);
                setP (a, PacificParams::BassLfoDepth,  0.35f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // 02 より drier、攻撃的
                setP (a, PacificParams::DelMix,     0.20f);
                setP (a, PacificParams::RevMix,     0.09f);
                setP (a, PacificParams::RevMode,    0.0f);     // Plate
                setP (a, PacificParams::Dist,       0.45f);  // 02 より歪ませる
                setP (a, PacificParams::ScAmt,      0.55f);
                setP (a, PacificParams::ScRel,      0.10f);
                setP (a, PacificParams::CompThresh, -16.0f);
                setP (a, PacificParams::CompRatio,  5.0f);

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12, 14 });
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 0, 2, 4, 6, 8, 10, 12, 14 }, 0.80f);
                drum (pat, H, { 1, 5, 9, 13 }, 0.40f);   // 16分ゴースト
                // 暴れる acid line (LOTS of slides/accents)
                bass (pat, 0,  0, 2, true,  false);
                bass (pat, 1,  0, 2, false, true);
                bass (pat, 3,  3, 2, false, true);
                bass (pat, 4,  0, 3, false, true);
                bass (pat, 6,  7, 1, true,  false);
                bass (pat, 7,  7, 1, false, true);
                bass (pat, 8,  0, 2, true,  false);
                bass (pat, 10, 10, 1, false, true);
                bass (pat, 11, 0, 2, false, true);
                bass (pat, 13, 3, 2, true,  false);
                bass (pat, 14, 7, 1, false, true);
                bass (pat, 15, 0, 3, false, true);
            }
        });

        // ─ 02: Dub Plate 130 — Roland Dub 風、Bass Chord で stab ─
        p.push_back ({ "02 Dub Plate 130", "Dub",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        130.0f);
                setP (a, PacificParams::KickPitch,  42.0f);
                setP (a, PacificParams::KickDec,    0.45f);
                setP (a, PacificParams::KickDrive,  0.10f);
                // Bass: chord stab 用に開けた tone
                setP (a, PacificParams::BassWave,   0.0f);   // Saw
                setP (a, PacificParams::Cutoff,     550.0f);
                setP (a, PacificParams::Reso,       2.5f);
                setP (a, PacificParams::EnvMod,     1500.0f);
                setP (a, PacificParams::BassDec,    0.75f);   // 長め
                setP (a, PacificParams::BassGlide,  0.10f);
                setP (a, PacificParams::BassSub,    0.30f);
                setP (a, PacificParams::BassDrive,  0.15f);
                // chord stab を Power (5th) で
                setP (a, PacificParams::BassChord,  2.0f);   // Power = root + 5th
                // ゆっくり cutoff うねり
                setP (a, PacificParams::BassLfoRate,   0.3f);
                setP (a, PacificParams::BassLfoDepth,  0.35f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // 巨大 delay + reverb (Dub の核)
                setP (a, PacificParams::DelMix,     0.55f);   // MAX 近く
                setP (a, PacificParams::RevMix,     0.27f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall
                setP (a, PacificParams::Dist,       0.08f);
                setP (a, PacificParams::ScAmt,      0.35f);
                setP (a, PacificParams::ScRel,      0.25f);

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, H, { 6, 14 }, 0.40f);
                // chord stab (root のみ指定、Power mode で 5th 自動追加)
                bass (pat, 0,  0, 1);                          // C1 + G1
                bass (pat, 6,  5, 1, false, true);             // F1 (+ C2) slide
                bass (pat, 8,  3, 1);                          // D#1 (+ A#1)
                bass (pat, 14, 0, 1, false, true);
            }
        });

        // ─ 03: Coastal Dub 80 — 超スロー、ambient と dub の橋渡し ─
        p.push_back ({ "03 Coastal Dub 80", "Dub",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        80.0f);
                setP (a, PacificParams::KickPitch,  40.0f);
                setP (a, PacificParams::KickDec,    0.55f);
                setP (a, PacificParams::KickDrive,  0.0f);
                // Bass: 持続音、サブ重視
                setP (a, PacificParams::BassWave,   0.0f);   // Saw
                setP (a, PacificParams::Cutoff,     350.0f);
                setP (a, PacificParams::Reso,       1.8f);
                setP (a, PacificParams::EnvMod,     800.0f);
                setP (a, PacificParams::BassDec,    1.30f);   // 超ロング
                setP (a, PacificParams::BassGlide,  0.15f);
                setP (a, PacificParams::BassSub,    0.55f);
                setP (a, PacificParams::BassDrive,  0.0f);
                // +Oct chord で厚み
                setP (a, PacificParams::BassChord,  1.0f);   // +Oct
                // 超ゆっくり LFO
                setP (a, PacificParams::BassLfoRate,   0.15f);
                setP (a, PacificParams::BassLfoDepth,  0.55f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // 巨大空間
                setP (a, PacificParams::DelMix,     0.50f);
                setP (a, PacificParams::RevMix,     0.33f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall
                setP (a, PacificParams::Dist,       0.0f);

                pat.clear();
                drum (pat, K, { 0 }, 0.65f);
                drum (pat, H, { 10 }, 0.30f);
                // 持続的 dub bass
                bass (pat, 0,  0, 1);                          // C1 + C2
                bass (pat, 8,  7, 1, false, true);             // G1 (+ G2) slide
            }
        });

        // ─ 03: Dub House 120 — Deep House × Dub の交差点 ─
        p.push_back ({ "03 Dub House 120", "House",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        120.0f);
                setP (a, PacificParams::KickPitch,  45.0f);
                setP (a, PacificParams::KickDec,    0.40f);
                setP (a, PacificParams::KickDrive,  0.10f);
                // Clap (Deep House の象徴)
                setP (a, PacificParams::ClapFreq,   1300.0f);
                setP (a, PacificParams::ClapDec,    0.18f);
                setP (a, PacificParams::ClapStyle,  0.5f);    // 707 (2 bursts) で深い
                // Bass: warm + sub heavy
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     500.0f);
                setP (a, PacificParams::Reso,       2.8f);
                setP (a, PacificParams::EnvMod,     1400.0f);
                setP (a, PacificParams::BassDec,    0.65f);
                setP (a, PacificParams::BassGlide,  0.06f);
                setP (a, PacificParams::BassSub,    0.45f);
                setP (a, PacificParams::BassDrive,  0.15f);
                // chord stab を Maj で
                setP (a, PacificParams::BassChord,  3.0f);   // Maj
                // ゆっくり cutoff
                setP (a, PacificParams::BassLfoRate,   0.25f);
                setP (a, PacificParams::BassLfoDepth,  0.30f);
                setP (a, PacificParams::BassLfoTarget, 0.0f);
                // dub と house の中間
                setP (a, PacificParams::DelMix,     0.40f);
                setP (a, PacificParams::RevMix,     0.252f);
                setP (a, PacificParams::RevMode,    2.0f);     // Hall
                setP (a, PacificParams::Dist,       0.06f);
                setP (a, PacificParams::ScAmt,      0.45f);
                setP (a, PacificParams::ScRel,      0.22f);
                setP (a, PacificParams::SnareCrisp, 0.15f);

                pat.clear();
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, C, { 4, 12 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.55f);
                // Maj chord stab で warm
                bass (pat, 0,  0, 1);                          // C1 + E1 + G1
                bass (pat, 6,  10, 0, false, true);            // A#0 slide
                bass (pat, 8,  5, 0);                          // F0
                bass (pat, 14, 7, 0, false, true);             // G0 slide
            }
        });

        // ─ 01: TR-808 90 — クラシック 808 (ヒップホップ系の deep sub kick + sparse pattern) ─
        p.push_back ({ "01 TR-808 90", "Machines",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        90.0f);
                // 808 Kick: 深い sub, 長い decay, クリーン
                setP (a, PacificParams::KickPitch,  45.0f);
                setP (a, PacificParams::KickAmt,    4.20f);
                setP (a, PacificParams::KickDec,    0.85f);
                setP (a, PacificParams::KickDrive,  0.10f);
                setP (a, PacificParams::KickVol,    0.30f);
                // 808 Snare: snappy, mid tone
                setP (a, PacificParams::SnareDec,   0.15f);
                setP (a, PacificParams::SnareSnap,  0.45f);
                setP (a, PacificParams::SnareCrisp, 0.20f);
                setP (a, PacificParams::SnareTone,  0.40f);
                // 808 HiHat: 短い closed metal 寄り
                setP (a, PacificParams::HhFreq,     8500.0f);
                setP (a, PacificParams::HhQ,        3.0f);
                setP (a, PacificParams::HhDec,      0.040f);
                setP (a, PacificParams::HhMetal,    0.30f);
                // 808 Clap: シングルバースト
                setP (a, PacificParams::ClapFreq,   1200.0f);
                setP (a, PacificParams::ClapQ,      1.5f);
                setP (a, PacificParams::ClapDec,    0.16f);
                setP (a, PacificParams::ClapStyle,  0.0f);
                // 808 Sub Bass: Saw + 低 cutoff + サブ重め
                setP (a, PacificParams::BassWave,   0.0f);   // Saw
                setP (a, PacificParams::Cutoff,     320.0f);
                setP (a, PacificParams::Reso,       1.5f);
                setP (a, PacificParams::EnvMod,     800.0f);
                setP (a, PacificParams::BassDec,    0.80f);
                setP (a, PacificParams::BassSub,    0.60f);
                setP (a, PacificParams::BassDrive,  0.10f);
                setP (a, PacificParams::BassGlide,  0.02f);
                // FX: 軽め
                setP (a, PacificParams::DelMix,     0.10f);
                setP (a, PacificParams::RevMix,     0.108f);
                setP (a, PacificParams::RevMode,    1.0f);   // Room
                setP (a, PacificParams::ScAmt,      0.35f);
                setP (a, PacificParams::ScRel,      0.25f);

                pat.clear();
                // ヒップホップ感: kick が後ノリ
                drum (pat, K, { 0, 7 });
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 0, 2, 4, 6, 8, 10, 12, 14 }, 0.55f);
                bass (pat, 0,  0, 1);                // C1
                bass (pat, 7,  3, 1);                // D#1
                bass (pat, 10, 7, 1);                // G1
            }
        });

        // ─ 02: TR-909 126 — クラシック House/Techno (4 つ打ち、パンチィキック) ─
        p.push_back ({ "02 TR-909 126", "Machines",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        126.0f);
                // 909 Kick: クリッキー、ミディアム decay
                setP (a, PacificParams::KickPitch,  60.0f);
                setP (a, PacificParams::KickAmt,    4.80f);
                setP (a, PacificParams::KickDec,    0.40f);
                setP (a, PacificParams::KickDrive,  0.35f);
                setP (a, PacificParams::KickVol,    0.32f);
                // 909 Snare: snappy & crispy
                setP (a, PacificParams::SnareDec,   0.18f);
                setP (a, PacificParams::SnareSnap,  0.65f);
                setP (a, PacificParams::SnareCrisp, 0.45f);
                setP (a, PacificParams::SnareTone,  0.35f);
                setP (a, PacificParams::SnareNoise, 0.10f);
                // 909 HiHat: white noise base、ブライト
                setP (a, PacificParams::HhFreq,     9500.0f);
                setP (a, PacificParams::HhQ,        2.5f);
                setP (a, PacificParams::HhDec,      0.045f);
                setP (a, PacificParams::HhMetal,    0.45f);
                setP (a, PacificParams::HhDrive,    0.10f);
                // 909 Clap: 707 風 (dual burst)
                setP (a, PacificParams::ClapFreq,   1100.0f);
                setP (a, PacificParams::ClapQ,      1.6f);
                setP (a, PacificParams::ClapDec,    0.18f);
                setP (a, PacificParams::ClapStyle,  0.5f);    // 707 dual burst
                // Bass: Square + ミディアム reso
                setP (a, PacificParams::BassWave,   1.0f);   // Square
                setP (a, PacificParams::Cutoff,     500.0f);
                setP (a, PacificParams::Reso,       4.0f);
                setP (a, PacificParams::EnvMod,     2500.0f);
                setP (a, PacificParams::BassDec,    0.40f);
                setP (a, PacificParams::BassSub,    0.30f);
                setP (a, PacificParams::BassDrive,  0.30f);
                setP (a, PacificParams::BassGlide,  0.005f);
                // FX
                setP (a, PacificParams::DelMix,     0.12f);
                setP (a, PacificParams::RevMix,     0.09f);
                setP (a, PacificParams::RevMode,    0.0f);   // Plate
                setP (a, PacificParams::ScAmt,      0.50f);
                setP (a, PacificParams::ScRel,      0.20f);

                pat.clear();
                // 4 つ打ち
                drum (pat, K, { 0, 4, 8, 12 });
                drum (pat, S, { 4, 12 });
                drum (pat, C, { 4, 12 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.65f);
                bass (pat, 0,  0, 1);                          // C1
                bass (pat, 6,  7, 0);                          // G0
                bass (pat, 8,  0, 1);                          // C1
                bass (pat, 14, 10, 0, false, true);            // A#0 slide
            }
        });

        // ─ 03: TR-606 138 — Drumatix (タイト/メタリック、Squarepusher 系電子) ─
        p.push_back ({ "03 TR-606 138", "Machines",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        138.0f);
                // 606 Kick: 高めピッチ、タイト
                setP (a, PacificParams::KickPitch,  75.0f);
                setP (a, PacificParams::KickAmt,    3.50f);
                setP (a, PacificParams::KickDec,    0.18f);
                setP (a, PacificParams::KickDrive,  0.30f);
                setP (a, PacificParams::KickVol,    0.22f);
                // 606 Snare: タイト + crispy
                setP (a, PacificParams::SnareDec,   0.08f);
                setP (a, PacificParams::SnareSnap,  0.55f);
                setP (a, PacificParams::SnareCrisp, 0.60f);
                setP (a, PacificParams::SnareTone,  0.50f);
                // 606 HiHat: 短く、メタリック (Squarepusher 系)
                setP (a, PacificParams::HhFreq,     7500.0f);
                setP (a, PacificParams::HhQ,        4.5f);
                setP (a, PacificParams::HhDec,      0.035f);
                setP (a, PacificParams::HhMetal,    0.75f);
                setP (a, PacificParams::HhDrive,    0.05f);
                // Clap: シングルバースト、明るめ
                setP (a, PacificParams::ClapFreq,   1500.0f);
                setP (a, PacificParams::ClapQ,      2.0f);
                setP (a, PacificParams::ClapDec,    0.10f);
                setP (a, PacificParams::ClapStyle,  0.0f);
                // Bass: タイト/Square
                setP (a, PacificParams::BassWave,   1.0f);
                setP (a, PacificParams::Cutoff,     700.0f);
                setP (a, PacificParams::Reso,       5.5f);
                setP (a, PacificParams::EnvMod,     3200.0f);
                setP (a, PacificParams::BassDec,    0.20f);
                setP (a, PacificParams::BassSub,    0.10f);
                setP (a, PacificParams::BassDrive,  0.40f);
                setP (a, PacificParams::BassGlide,  0.002f);
                // FX: ドライ
                setP (a, PacificParams::DelMix,     0.08f);
                setP (a, PacificParams::RevMix,     0.048f);
                setP (a, PacificParams::RevMode,    0.0f);   // Plate (タイト)
                setP (a, PacificParams::ScAmt,      0.30f);
                setP (a, PacificParams::ScRel,      0.10f);

                pat.clear();
                // 606 のせわしないパターン
                drum (pat, K, { 0, 6, 8, 14 });
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 0, 2, 4, 6, 8, 10, 12, 14 }, 0.60f);   // 8 分連打
                bass (pat, 0,  0, 1);
                bass (pat, 3,  0, 1);
                bass (pat, 6,  3, 1);
                bass (pat, 10, 7, 0);
                bass (pat, 13, 10, 0);
            }
        });

        // ─ 04: TB-303 130 — 純粋 acid bassline テンプレ (Bass 主役、minimal drums) ─
        p.push_back ({ "04 TB-303 130", "Machines",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        130.0f);
                // Kick: 最小限 (303 単独でも groove するように)
                setP (a, PacificParams::KickPitch,  48.0f);
                setP (a, PacificParams::KickAmt,    4.00f);
                setP (a, PacificParams::KickDec,    0.30f);
                setP (a, PacificParams::KickDrive,  0.20f);
                setP (a, PacificParams::KickVol,    0.24f);
                // HiHat: 軽め groove
                setP (a, PacificParams::HhFreq,     8500.0f);
                setP (a, PacificParams::HhQ,        3.0f);
                setP (a, PacificParams::HhDec,      0.040f);
                setP (a, PacificParams::HhMetal,    0.30f);
                // ─ TB-303 BASS ─ 全 303 設定の "Init" 的なポジション
                setP (a, PacificParams::BassWave,   0.0f);   // Saw (303 の signature)
                setP (a, PacificParams::Cutoff,     550.0f); // 中域、自分で動かす想定
                setP (a, PacificParams::Reso,       9.5f);   // 自己発振寸前
                setP (a, PacificParams::EnvMod,     5500.0f);// 深い filter sweep
                setP (a, PacificParams::BassDec,    0.28f);  // sweet spot
                setP (a, PacificParams::AccAmt,     0.85f);  // 強い accent
                setP (a, PacificParams::BassGlide,  0.060f); // slide が効く
                setP (a, PacificParams::BassDrive,  0.55f);  // 太い acid 歪み
                setP (a, PacificParams::BassSub,    0.0f);   // 303 にサブはない
                // FX
                setP (a, PacificParams::DelMix,     0.15f);
                setP (a, PacificParams::RevMix,     0.06f);
                setP (a, PacificParams::RevMode,    0.0f);   // Plate
                setP (a, PacificParams::Dist,       0.20f);  // acid distortion
                setP (a, PacificParams::ScAmt,      0.40f);
                setP (a, PacificParams::ScRel,      0.13f);

                pat.clear();
                // Kick は控えめ、Bass を主役に
                drum (pat, K, { 0, 8 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.55f);
                // ─ クラシック 303 アシッドライン ─
                bass (pat, 0,  0, 2, true,  false);    // C2 accent
                bass (pat, 2,  0, 3, false, true);     // C3 slide up
                bass (pat, 4,  0, 2, true,  false);    // C2 accent
                bass (pat, 5,  3, 2, false, true);     // D#2 slide
                bass (pat, 7,  0, 2, false, false);    // C2
                bass (pat, 8,  0, 2, true,  false);    // C2 accent
                bass (pat, 10, 7, 2, false, true);     // G2 slide
                bass (pat, 12, 5, 2, true,  false);    // F2 accent
                bass (pat, 13, 5, 2, false, false);    // F2
                bass (pat, 14, 3, 2, false, true);     // D#2 slide
            }
        });

        // ─ 01: Breakbeat 135 — Big Beat / Amen feel (broken kick + syncopated snare) ─
        p.push_back ({ "01 Breakbeat 135", "Breaks",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        135.0f);
                // Kick: パンチィでミディアム sub
                setP (a, PacificParams::KickPitch,  55.0f);
                setP (a, PacificParams::KickAmt,    4.20f);
                setP (a, PacificParams::KickDec,    0.30f);
                setP (a, PacificParams::KickDrive,  0.30f);
                setP (a, PacificParams::KickVol,    0.26f);
                // Snare: bright break
                setP (a, PacificParams::SnareDec,   0.16f);
                setP (a, PacificParams::SnareSnap,  0.70f);
                setP (a, PacificParams::SnareCrisp, 0.50f);
                setP (a, PacificParams::SnareTone,  0.40f);
                // HiHat: 中域、breaks 感
                setP (a, PacificParams::HhFreq,     8500.0f);
                setP (a, PacificParams::HhQ,        3.0f);
                setP (a, PacificParams::HhDec,      0.050f);
                setP (a, PacificParams::HhMetal,    0.35f);
                setP (a, PacificParams::HhDrive,    0.10f);
                // Bass: rolling
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     520.0f);
                setP (a, PacificParams::Reso,       3.5f);
                setP (a, PacificParams::EnvMod,     2200.0f);
                setP (a, PacificParams::BassDec,    0.32f);
                setP (a, PacificParams::BassSub,    0.35f);
                setP (a, PacificParams::BassDrive,  0.25f);
                setP (a, PacificParams::BassGlide,  0.015f);
                // FX: 大きめ reverb
                setP (a, PacificParams::DelMix,     0.20f);
                setP (a, PacificParams::RevMix,     0.15f);
                setP (a, PacificParams::RevMode,    1.0f);   // Room
                setP (a, PacificParams::Dist,       0.10f);
                setP (a, PacificParams::ScAmt,      0.45f);
                setP (a, PacificParams::ScRel,      0.18f);

                pat.clear();
                // Amen-ish syncopation
                drum (pat, K, { 0, 6, 10 });
                drum (pat, S, { 4, 10, 14 });
                drum (pat, H, { 2, 5, 7, 9, 11, 13, 15 }, 0.55f);
                bass (pat, 0,  0, 1);                          // C1
                bass (pat, 6,  3, 1);                          // D#1
                bass (pat, 10, 7, 0);                          // G0
                bass (pat, 14, 5, 0, false, true);             // F0 slide
            }
        });

        // ─ 02: Jungle 168 — 90s ジャングル (chopped break + sub bass) ─
        p.push_back ({ "02 Jungle 168", "Breaks",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        168.0f);
                // Kick: punchy
                setP (a, PacificParams::KickPitch,  50.0f);
                setP (a, PacificParams::KickAmt,    4.40f);
                setP (a, PacificParams::KickDec,    0.22f);
                setP (a, PacificParams::KickDrive,  0.30f);
                setP (a, PacificParams::KickVol,    0.24f);
                // Snare: tight & high (break feel)
                setP (a, PacificParams::SnareDec,   0.12f);
                setP (a, PacificParams::SnareSnap,  0.75f);
                setP (a, PacificParams::SnareCrisp, 0.65f);
                setP (a, PacificParams::SnareTone,  0.45f);
                // HiHat: 短く、明るく
                setP (a, PacificParams::HhFreq,     10000.0f);
                setP (a, PacificParams::HhQ,        2.5f);
                setP (a, PacificParams::HhDec,      0.030f);
                setP (a, PacificParams::HhMetal,    0.35f);
                // Bass: heavy SUB (レゲエルーツ)
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     340.0f);
                setP (a, PacificParams::Reso,       2.0f);
                setP (a, PacificParams::EnvMod,     1300.0f);
                setP (a, PacificParams::BassDec,    0.80f);
                setP (a, PacificParams::BassSub,    0.75f);
                setP (a, PacificParams::BassDrive,  0.15f);
                setP (a, PacificParams::BassGlide,  0.05f);
                // FX: 長い reverb tail (jungle のスペース感)
                setP (a, PacificParams::DelMix,     0.25f);
                setP (a, PacificParams::RevMix,     0.18f);
                setP (a, PacificParams::RevMode,    2.0f);   // Hall (大きな空間)
                setP (a, PacificParams::Dist,       0.08f);
                setP (a, PacificParams::ScAmt,      0.45f);
                setP (a, PacificParams::ScRel,      0.15f);

                pat.clear();
                // 切り刻まれた break のフィーリング
                drum (pat, K, { 0, 8, 11 });
                drum (pat, S, { 4, 10, 14 });
                drum (pat, H, { 0, 2, 4, 6, 8, 10, 12, 14 }, 0.50f);
                // Long-held sub bass
                bass (pat, 0,  0, 1);                          // C1
                bass (pat, 8,  3, 0);                          // D#0
                bass (pat, 12, 7, 0, false, true);             // G0 slide
            }
        });

        // ─ 03: Footwork 160 — Chicago Footwork (polyrhythmic triplet kicks) ─
        p.push_back ({ "03 Footwork 160", "Breaks",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        160.0f);
                // Kick: deep sub punch
                setP (a, PacificParams::KickPitch,  46.0f);
                setP (a, PacificParams::KickAmt,    4.30f);
                setP (a, PacificParams::KickDec,    0.32f);
                setP (a, PacificParams::KickDrive,  0.20f);
                setP (a, PacificParams::KickVol,    0.30f);
                // Snare: sparse, bright
                setP (a, PacificParams::SnareDec,   0.14f);
                setP (a, PacificParams::SnareSnap,  0.60f);
                setP (a, PacificParams::SnareCrisp, 0.40f);
                setP (a, PacificParams::SnareTone,  0.35f);
                // Clap (footwork は clap も signature)
                setP (a, PacificParams::ClapFreq,   1400.0f);
                setP (a, PacificParams::ClapDec,    0.12f);
                setP (a, PacificParams::ClapStyle,  0.0f);
                // HiHat: 密集 syncopated
                setP (a, PacificParams::HhFreq,     9000.0f);
                setP (a, PacificParams::HhQ,        2.8f);
                setP (a, PacificParams::HhDec,      0.030f);
                setP (a, PacificParams::HhMetal,    0.35f);
                // Bass: rolling sub
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     400.0f);
                setP (a, PacificParams::Reso,       2.5f);
                setP (a, PacificParams::EnvMod,     1400.0f);
                setP (a, PacificParams::BassDec,    0.60f);
                setP (a, PacificParams::BassSub,    0.60f);
                setP (a, PacificParams::BassDrive,  0.18f);
                setP (a, PacificParams::BassGlide,  0.025f);
                // FX
                setP (a, PacificParams::DelMix,     0.15f);
                setP (a, PacificParams::RevMix,     0.108f);
                setP (a, PacificParams::RevMode,    1.0f);   // Room
                setP (a, PacificParams::Dist,       0.08f);
                setP (a, PacificParams::ScAmt,      0.45f);
                setP (a, PacificParams::ScRel,      0.12f);

                pat.clear();
                // Triplet/polyrhythmic kicks (16 step 内で 3 連的な配置)
                drum (pat, K, { 0, 3, 6, 10, 13 });
                drum (pat, C, { 8 });                  // clap on step 8 (footwork accent)
                drum (pat, S, { 12 });                 // snare 1 発のみ (sparse)
                drum (pat, H, { 1, 2, 4, 5, 7, 9, 11, 14, 15 }, 0.55f);
                // rolling sub bass
                bass (pat, 0,  0, 1);                          // C1
                bass (pat, 3,  0, 1);                          // C1 (roll)
                bass (pat, 8,  3, 0);                          // D#0
                bass (pat, 11, 7, 0, false, true);             // G0 slide
                bass (pat, 14, 5, 0);                          // F0
            }
        });

        // ─ 01: IDM Glitch 120 — Aphex Twin "Druqks" 風 (glitchy/syncopated) ─
        p.push_back ({ "01 IDM Glitch 120", "IDM",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        120.0f);
                // Kick: ミディアム、tight
                setP (a, PacificParams::KickPitch,  50.0f);
                setP (a, PacificParams::KickAmt,    4.20f);
                setP (a, PacificParams::KickDec,    0.30f);
                setP (a, PacificParams::KickDrive,  0.22f);
                setP (a, PacificParams::KickVol,    0.24f);
                // Snare: tight & glitchy
                setP (a, PacificParams::SnareDec,   0.085f);
                setP (a, PacificParams::SnareSnap,  0.80f);
                setP (a, PacificParams::SnareCrisp, 0.70f);
                setP (a, PacificParams::SnareTone,  0.40f);
                // HiHat: 短く鋭い
                setP (a, PacificParams::HhFreq,     10500.0f);
                setP (a, PacificParams::HhQ,        2.5f);
                setP (a, PacificParams::HhDec,      0.025f);
                setP (a, PacificParams::HhMetal,    0.55f);
                setP (a, PacificParams::HhDrive,    0.20f);
                // Bass: 歪んだ saw、ちょい detune 風 (high reso で揺れ作る)
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     800.0f);
                setP (a, PacificParams::Reso,       6.0f);
                setP (a, PacificParams::EnvMod,     3000.0f);
                setP (a, PacificParams::BassDec,    0.28f);
                setP (a, PacificParams::BassSub,    0.25f);
                setP (a, PacificParams::BassDrive,  0.45f);
                setP (a, PacificParams::BassGlide,  0.015f);
                // FX: heavy delay、軽め reverb、軽い dist
                setP (a, PacificParams::DelMix,     0.35f);
                setP (a, PacificParams::RevMix,     0.12f);
                setP (a, PacificParams::RevMode,    0.0f);    // Plate (tight)
                setP (a, PacificParams::Dist,       0.18f);
                setP (a, PacificParams::ScAmt,      0.40f);
                setP (a, PacificParams::ScRel,      0.13f);

                pat.clear();
                // 不規則 syncopated パターン (Aphex Twin 風)
                drum (pat, K, { 0, 3, 7, 11, 14 });
                drum (pat, S, { 4, 9, 12, 13 });               // 連続スネア
                drum (pat, H, { 0, 1, 4, 5, 8, 9, 11, 12, 13, 15 }, 0.55f);
                // 跳ねるベースライン
                bass (pat, 0,  0, 2, true,  false);
                bass (pat, 3,  7, 1);
                bass (pat, 7,  3, 2, false, true);
                bass (pat, 11, 0, 2, true,  false);
                bass (pat, 14, 10, 1, false, true);
            }
        });

        // ─ 02: IDM Drill 145 — Squarepusher "Drill 'n' Bass" 風 (高速 break + jazz bass) ─
        p.push_back ({ "02 IDM Drill 145", "IDM",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        145.0f);
                // Kick: tight、クリッキー
                setP (a, PacificParams::KickPitch,  60.0f);
                setP (a, PacificParams::KickAmt,    4.30f);
                setP (a, PacificParams::KickDec,    0.18f);
                setP (a, PacificParams::KickDrive,  0.32f);
                setP (a, PacificParams::KickVol,    0.24f);
                // Snare: super crisp & fast
                setP (a, PacificParams::SnareDec,   0.07f);
                setP (a, PacificParams::SnareSnap,  0.75f);
                setP (a, PacificParams::SnareCrisp, 0.70f);
                setP (a, PacificParams::SnareTone,  0.45f);
                // HiHat: ロール対応 (短く密集)
                setP (a, PacificParams::HhFreq,     11000.0f);
                setP (a, PacificParams::HhQ,        2.2f);
                setP (a, PacificParams::HhDec,      0.025f);
                setP (a, PacificParams::HhMetal,    0.40f);
                // Bass: jazzy melodic、明るめ
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     900.0f);
                setP (a, PacificParams::Reso,       4.0f);
                setP (a, PacificParams::EnvMod,     2400.0f);
                setP (a, PacificParams::BassDec,    0.22f);
                setP (a, PacificParams::BassSub,    0.30f);
                setP (a, PacificParams::BassDrive,  0.28f);
                setP (a, PacificParams::BassGlide,  0.012f);
                // FX: clean が drill 'n' bass の特徴
                setP (a, PacificParams::DelMix,     0.15f);
                setP (a, PacificParams::RevMix,     0.08f);
                setP (a, PacificParams::RevMode,    0.0f);    // Plate
                setP (a, PacificParams::Dist,       0.10f);
                setP (a, PacificParams::ScAmt,      0.45f);
                setP (a, PacificParams::ScRel,      0.12f);

                pat.clear();
                // Drill 'n' bass の break-ish パターン
                drum (pat, K, { 0, 4, 7, 10, 12 });
                drum (pat, S, { 4, 11, 13 });
                drum (pat, H, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }, 0.50f);
                // Jazzy bass run
                bass (pat, 0,  0, 1, true,  false);
                bass (pat, 4,  5, 1);
                bass (pat, 7,  7, 1, false, true);
                bass (pat, 10, 10, 1);
                bass (pat, 13, 3, 1, false, true);
            }
        });

        // ─ 03: IDM Warm 88 — Boards of Canada 風 (warm/dusty/melodic、ノスタルジック) ─
        p.push_back ({ "03 IDM Warm 88", "IDM",
            [] (auto& proc, auto& a, auto& pat)
            {
                resetAll (proc);
                setP (a, PacificParams::Bpm,        88.0f);
                // Kick: warm、長め
                setP (a, PacificParams::KickPitch,  58.0f);
                setP (a, PacificParams::KickAmt,    3.80f);
                setP (a, PacificParams::KickDec,    0.45f);
                setP (a, PacificParams::KickDrive,  0.18f);
                setP (a, PacificParams::KickVol,    0.26f);
                // Snare: dusty、控えめ snap
                setP (a, PacificParams::SnareDec,   0.18f);
                setP (a, PacificParams::SnareSnap,  0.30f);
                setP (a, PacificParams::SnareCrisp, 0.20f);
                setP (a, PacificParams::SnareTone,  0.50f);
                setP (a, PacificParams::SnareNoise, 0.35f);   // pink-ish
                // HiHat: muted、warm
                setP (a, PacificParams::HhFreq,     6500.0f);
                setP (a, PacificParams::HhQ,        2.0f);
                setP (a, PacificParams::HhDec,      0.055f);
                setP (a, PacificParams::HhMetal,    0.10f);
                // Bass: warm pad、サブ heavy
                setP (a, PacificParams::BassWave,   0.0f);
                setP (a, PacificParams::Cutoff,     350.0f);
                setP (a, PacificParams::Reso,       1.5f);
                setP (a, PacificParams::EnvMod,     1000.0f);
                setP (a, PacificParams::BassDec,    0.75f);
                setP (a, PacificParams::BassSub,    0.55f);
                setP (a, PacificParams::BassDrive,  0.12f);
                setP (a, PacificParams::BassGlide,  0.04f);
                // FX: warm reverb (BoC の代名詞)
                setP (a, PacificParams::DelMix,     0.22f);
                setP (a, PacificParams::RevMix,     0.30f);
                setP (a, PacificParams::RevMode,    2.0f);    // Hall (warm)
                setP (a, PacificParams::Dist,       0.10f);   // テープサチュレーション
                setP (a, PacificParams::ScAmt,      0.30f);
                setP (a, PacificParams::ScRel,      0.20f);

                pat.clear();
                // シンプルだが感情的なパターン
                drum (pat, K, { 0, 8 });
                drum (pat, S, { 4, 12 });
                drum (pat, H, { 2, 6, 10, 14 }, 0.50f);
                // メロディックベース (BoC 風和音感)
                bass (pat, 0,  0, 1);                          // C1
                bass (pat, 4,  7, 0);                          // G0
                bass (pat, 8,  5, 1);                          // F1
                bass (pat, 12, 3, 1, false, true);             // D#1 slide
            }
        });

        return p;
    }
} // namespace FactoryPresets
