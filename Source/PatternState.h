#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include <vector>

// ─────────────────────────────────────────────────────────────
//  PatternState
//  16ステップのドラム4トラック + ベース1トラックのパターンデータ。
//  UIスレッドと audio スレッドの双方からアクセスされるため、
//  軽量な SpinLock で保護する。
// ─────────────────────────────────────────────────────────────
class PatternState
{
public:
    static constexpr int NumSteps   = 16;
    static constexpr int NumDrumRows = 4;  // KICK / SNARE / HIHAT / CLAP

    struct DrumCell {
        bool  on = false;
        float velocity = 0.85f;
    };

    struct BassCell {
        bool  on     = false;
        int   note   = 0;     // 0=C ... 11=B
        int   octave = 2;
        bool  accent = false;
        bool  slide  = false;
    };

    // ─ 編集 (UI スレッド)
    void toggleDrum (int row, int step) noexcept
    {
        if (! inRange (row, NumDrumRows) || ! inRange (step, NumSteps)) return;
        const juce::SpinLock::ScopedLockType l (lock);
        drum[(size_t) row][(size_t) step].on = ! drum[(size_t) row][(size_t) step].on;
    }
    void setDrum (int row, int step, DrumCell c) noexcept
    {
        if (! inRange (row, NumDrumRows) || ! inRange (step, NumSteps)) return;
        const juce::SpinLock::ScopedLockType l (lock);
        drum[(size_t) row][(size_t) step] = c;
    }
    DrumCell getDrum (int row, int step) const noexcept
    {
        if (! inRange (row, NumDrumRows) || ! inRange (step, NumSteps)) return {};
        const juce::SpinLock::ScopedLockType l (lock);
        return drum[(size_t) row][(size_t) step];
    }

    void toggleBass (int step) noexcept
    {
        if (! inRange (step, NumSteps)) return;
        const juce::SpinLock::ScopedLockType l (lock);
        bass[(size_t) step].on = ! bass[(size_t) step].on;
    }
    void setBass (int step, BassCell c) noexcept
    {
        if (! inRange (step, NumSteps)) return;
        const juce::SpinLock::ScopedLockType l (lock);
        bass[(size_t) step] = c;
    }
    BassCell getBass (int step) const noexcept
    {
        if (! inRange (step, NumSteps)) return {};
        const juce::SpinLock::ScopedLockType l (lock);
        return bass[(size_t) step];
    }

    void clear() noexcept
    {
        const juce::SpinLock::ScopedLockType l (lock);
        for (auto& row : drum) for (auto& c : row) c = {};
        for (auto& c : bass) c = {};
    }

    // BPM 帯ごとの Kick テンプレを重み付きで選ぶランダム化。
    // bpm=128 (default) は従来挙動の互換。明示すれば genre 適合パターンが優先される。
    void randomize (juce::Random& rng, float bpm = 128.0f) noexcept
    {
        const juce::SpinLock::ScopedLockType l (lock);

        // ── Kick: BPM 帯ごとのテンプレートを重み付き抽選 ──
        // ジャンル想定:
        //   < 95  : Lofi/Synthwave (boom-bap 系)
        //   95-115: Trance slow / Synthwave fast / Detroit slow
        //   115-140: House / Techno / Acid / Detroit 中庸
        //   140-160: Hard Techno (8分キック中心)
        //   160+ : DnB / Footwork (2-step or 1+10)
        struct KickTpl { std::vector<int> steps; float weight; };
        std::vector<KickTpl> tpls;
        if (bpm < 95.0f) {
            tpls = {
                {{0, 8},           3.0f},   // Boom-bap 基本
                {{0, 8, 11},       1.5f},   // Boom-bap + ghost
                {{0, 4, 8, 12},    0.6f},   // ゆっくり 4-on-floor
                {{0},              0.4f},   // 超スパース
                {{0, 6, 8, 14},    0.5f},   // Synthwave 変則
            };
        } else if (bpm < 115.0f) {
            tpls = {
                {{0, 4, 8, 12},    3.0f},   // 4-on-floor 標準
                {{0, 8},           1.0f},   // ハーフタイム
                {{0, 4, 6, 8, 12, 14}, 1.0f}, // Trance pump
                {{0, 4, 8, 12, 14}, 0.7f},  // 4-on + ghost
            };
        } else if (bpm < 140.0f) {
            tpls = {
                {{0, 4, 8, 12},    3.5f},   // Classic 4-on-floor
                {{0, 4, 8, 12, 14}, 1.2f},  // + 14 ghost
                {{0, 10},          0.8f},   // UK Garage 2-step
                {{0, 4, 7, 8, 12}, 0.6f},   // off-beat ghost
                {{0, 4, 8, 11, 12, 14}, 0.5f}, // 細かい acid pump
            };
        } else if (bpm < 160.0f) {
            tpls = {
                {{0, 2, 4, 6, 8, 10, 12, 14}, 2.5f}, // 8-on-floor hard
                {{0, 4, 6, 8, 12, 14}, 1.8f},        // Hard pump
                {{0, 4, 8, 12},    1.0f},            // 標準 4-on-floor
                {{0, 4, 8, 12, 15}, 0.6f},           // 4-on + roll
            };
        } else {
            tpls = {
                {{0, 10},          3.0f},   // DnB 典型
                {{0, 8},           1.2f},   // 半テンポ DnB
                {{0, 10, 14},      0.8f},   // DnB + roll
                {{0, 5, 10},       0.5f},   // 3 amen
            };
        }

        // 重み付き抽選
        float totalW = 0.0f;
        for (auto& t : tpls) totalW += t.weight;
        const float pick = rng.nextFloat() * totalW;
        std::vector<int>* chosen = &tpls.back().steps;
        float cum = 0.0f;
        for (auto& t : tpls) {
            cum += t.weight;
            if (pick <= cum) { chosen = &t.steps; break; }
        }

        // Kick 行をクリアしてから template 適用
        for (int s = 0; s < NumSteps; ++s) drum[0][(size_t) s] = {};
        for (int s : *chosen)
            if (s >= 0 && s < NumSteps)
                drum[0][(size_t) s] = { true, 0.80f + rng.nextFloat() * 0.20f };

        // ── Snare/HiHat/Clap: 確率ベースで従来挙動 (Kick だけ genre 化)
        const float drumProb[NumDrumRows] = { 0.0f, 0.2f, 0.45f, 0.15f };
        for (int r = 1; r < NumDrumRows; ++r)
            for (int s = 0; s < NumSteps; ++s)
                drum[(size_t) r][(size_t) s] = { rng.nextFloat() < drumProb[r],
                                                 0.55f + rng.nextFloat() * 0.45f };

        // Bass: A-minor pentatonic 風
        static const int notes[] = { 0, 3, 5, 7, 10 };
        for (int s = 0; s < NumSteps; ++s)
        {
            BassCell c;
            c.on     = rng.nextFloat() < 0.55f;
            c.note   = notes[rng.nextInt (5)];
            c.octave = rng.nextFloat() < 0.3f ? 1 : (rng.nextFloat() < 0.85f ? 2 : 3);
            c.accent = rng.nextFloat() < 0.2f;
            c.slide  = rng.nextFloat() < 0.2f;
            bass[(size_t) s] = c;
        }
    }

    // ─ 状態シリアライズ
    juce::ValueTree toValueTree() const
    {
        const juce::SpinLock::ScopedLockType l (lock);
        juce::ValueTree v ("Pattern");
        for (int r = 0; r < NumDrumRows; ++r)
        {
            juce::ValueTree rv ("Drum");
            rv.setProperty ("row", r, nullptr);
            for (int s = 0; s < NumSteps; ++s)
            {
                const auto& d = drum[(size_t) r][(size_t) s];
                juce::ValueTree cv ("C");
                cv.setProperty ("s", s, nullptr);
                cv.setProperty ("on", d.on, nullptr);
                cv.setProperty ("v", d.velocity, nullptr);
                rv.appendChild (cv, nullptr);
            }
            v.appendChild (rv, nullptr);
        }
        juce::ValueTree bv ("Bass");
        for (int s = 0; s < NumSteps; ++s)
        {
            const auto& b = bass[(size_t) s];
            juce::ValueTree cv ("C");
            cv.setProperty ("s",  s,        nullptr);
            cv.setProperty ("on", b.on,     nullptr);
            cv.setProperty ("n",  b.note,   nullptr);
            cv.setProperty ("o",  b.octave, nullptr);
            cv.setProperty ("a",  b.accent, nullptr);
            cv.setProperty ("l",  b.slide,  nullptr);
            bv.appendChild (cv, nullptr);
        }
        v.appendChild (bv, nullptr);
        return v;
    }

    void fromValueTree (const juce::ValueTree& v)
    {
        if (! v.hasType ("Pattern")) return;

        // ── ロック外で新パターンを構築 (ValueTree走査は時間がかかるため) ──
        decltype (drum) newDrum {};
        decltype (bass) newBass {};

        for (auto rv : v)
        {
            if (rv.hasType ("Drum"))
            {
                const int row = (int) rv.getProperty ("row", 0);
                if (! inRange (row, NumDrumRows)) continue;
                for (auto cv : rv)
                {
                    const int s = (int) cv.getProperty ("s", -1);
                    if (! inRange (s, NumSteps)) continue;
                    newDrum[(size_t) row][(size_t) s].on       = (bool)  cv.getProperty ("on", false);
                    newDrum[(size_t) row][(size_t) s].velocity = (float) cv.getProperty ("v", 0.85);
                }
            }
            else if (rv.hasType ("Bass"))
            {
                for (auto cv : rv)
                {
                    const int s = (int) cv.getProperty ("s", -1);
                    if (! inRange (s, NumSteps)) continue;
                    newBass[(size_t) s].on     = (bool) cv.getProperty ("on", false);
                    newBass[(size_t) s].note   = (int)  cv.getProperty ("n", 0);
                    newBass[(size_t) s].octave = (int)  cv.getProperty ("o", 2);
                    newBass[(size_t) s].accent = (bool) cv.getProperty ("a", false);
                    newBass[(size_t) s].slide  = (bool) cv.getProperty ("l", false);
                }
            }
        }

        // ── ロックを取得して swap だけ ──
        // オーディオスレッドが待つ時間は数十命令程度
        const juce::SpinLock::ScopedLockType l (lock);
        drum = newDrum;
        bass = newBass;
    }

    static const char* noteName (int n) noexcept
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        if (n < 0 || n > 11) return "C";
        return names[n];
    }

    static int toMidiNote (int note, int oct) noexcept { return note + (oct + 1) * 12; }

private:
    static bool inRange (int v, int max) noexcept { return v >= 0 && v < max; }

    mutable juce::SpinLock lock;
    std::array<std::array<DrumCell, (size_t) NumSteps>, (size_t) NumDrumRows> drum {};
    std::array<BassCell,             (size_t) NumSteps>                       bass {};
};

// ─────────────────────────────────────────────────────────────
//  PatternUndoAction
//  パターン編集前後の状態を ValueTree として保持し、Undo/Redo で復元する。
//  「編集」自体は perform() / undo() で fromValueTree を呼ぶだけ。
// ─────────────────────────────────────────────────────────────
class PatternUndoAction : public juce::UndoableAction
{
public:
    PatternUndoAction (PatternState& p,
                       juce::ValueTree beforeStateIn,
                       juce::ValueTree afterStateIn)
        : pattern (p),
          beforeState (std::move (beforeStateIn)),
          afterState  (std::move (afterStateIn)) {}

    bool perform() override { pattern.fromValueTree (afterState);  return true; }
    bool undo()    override { pattern.fromValueTree (beforeState); return true; }

    // メモリ使用量の概算 (UndoManager のサイズ管理用)
    int getSizeInUnits() override
    {
        // ValueTree 2つ分。実測ベースで概算
        return 2048;
    }

private:
    PatternState&   pattern;
    juce::ValueTree beforeState;
    juce::ValueTree afterState;
};
