#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <functional>

// ─────────────────────────────────────────────────────────────
//  Sequencer
//  ホストの PPQ 位置から 16分音符単位の現在ステップを算出し、
//  ステップが変わったタイミングで onStep を発火する。
// ─────────────────────────────────────────────────────────────
class Sequencer
{
public:
    static constexpr int NumSteps = 16;

    using StepTrigger = std::function<void(int stepIndex)>;

    void setOnStep (StepTrigger fn) { onStep = std::move (fn); }

    // ブロック開始時に呼ぶ
    void beginBlock (bool isPlaying)
    {
        if (! isPlaying) {
            lastStep.store (-1, std::memory_order_relaxed);
            wasPlaying = false;
            return;
        }
        wasPlaying = true;
    }

    // サンプル単位で進める。PPQ は16分音符換算の位置 = ppqPosition * 4
    void processSample (double ppq16th)
    {
        if (! wasPlaying) return;
        const int rawStep = (int) std::floor (ppq16th);
        const int step = ((rawStep % NumSteps) + NumSteps) % NumSteps;
        const int prev = lastStep.load (std::memory_order_relaxed);
        if (step != prev)
        {
            lastStep.store (step, std::memory_order_relaxed);
            if (onStep) onStep (step);
        }
    }

    int getCurrentStep() const noexcept { return lastStep.load (std::memory_order_relaxed); }
    void reset() noexcept { lastStep.store (-1); wasPlaying = false; }

private:
    StepTrigger onStep;
    std::atomic<int> lastStep { -1 };
    bool wasPlaying = false;
};
