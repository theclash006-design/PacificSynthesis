#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>
#include <functional>
#include <cmath>

#include "PluginProcessor.h"

// ─────────────────────────────────────────────────────────────
//  WavExporter
//  PacificSynthesisProcessor の state を別インスタンス (クローン) に
//  コピーし、オフラインで N 小節 + FX テールをレンダリングして
//  .wav ファイルへ書き出す。
//
//  - 2MIX : 全パート (現在の voiceSrc 状態を尊重) をレンダリング
//  - STEMS: 5 つのボイス (Kick/Snare/HiHat/Clap/Bass) を 1 つずつ
//           solo してレンダリングし 5 ファイルに分ける。
//           FX (Delay/Reverb/Comp) は各ステムに適用される。
//
//  ノーマライズ:
//    FxChain の masterGain は live 用ヘッドルーム前提で 0.7 (-3dB) に
//    なっており、書き出した WAV を単体再生すると小さく感じるため、
//    既定で書き出し時にピーク -1 dBFS まで線形ブーストする。
//    Stems の場合は 5 ファイル間で同じスケールを使うので、DAW で
//    サミングしたときの相対バランスが保たれる。
// ─────────────────────────────────────────────────────────────
class WavExporter
{
public:
    enum class Type { Mix, Stems };

    struct Options
    {
        int    bars         = 1;        // 1 / 2 / 4 / 8
        int    sampleRate   = 48000;    // 44100 or 48000
        int    bitDepth     = 24;       // 16 / 24
        double tailSeconds  = 3.0;      // FX tail (reverb/delay 残響)
        int    blockSize    = 512;

        // ピークノーマライズ: true なら targetPeakDb までブーストして書き出す。
        bool   normalize    = true;
        float  targetPeakDb = -1.0f;    // 例: -1.0f = ピークを -1 dBFS に合わせる
    };

    // Returns the list of files actually written. Empty array = failure.
    static juce::Array<juce::File> exportPattern (
        PacificSynthesisProcessor& srcProc,
        const juce::File& outputFolder,
        const juce::String& baseName,
        Type type,
        const Options& opts,
        std::function<void(double progress01)> progressCb = {})
    {
        juce::Array<juce::File> written;

        if (! outputFolder.isDirectory())
        {
            const auto r = outputFolder.createDirectory();
            if (r.failed()) return written;
        }

        // ─ src の state を取得 (apvts / pattern / voiceSrcMode 全部含まれる) ─
        juce::MemoryBlock state;
        srcProc.getStateInformation (state);

        const double bpm = [&]() -> double {
            if (auto* p = srcProc.apvts.getRawParameterValue (PacificParams::Bpm))
                return juce::jmax (20.0, (double) p->load());
            return 128.0;
        }();

        // 1 bar = 240 / bpm 秒。N bars + tail = 録音時間
        const double barSec    = 240.0 / bpm;
        const double recSec    = opts.bars * barSec;
        const int    recSamps  = (int) std::ceil (recSec * opts.sampleRate);
        const int    tailSamps = (int) std::ceil (opts.tailSeconds * opts.sampleRate);
        const int    totalSamps = recSamps + tailSamps;

        const juce::String safeName = baseName.isEmpty() ? juce::String ("Pacific") : sanitize (baseName);
        const juce::String bpmTag   = juce::String ((int) std::round (bpm)) + "bpm";
        const juce::String barsTag  = juce::String (opts.bars) + "bar";

        // ピーク → 線形スケールへ
        auto scaleFromPeak = [&] (float peak) -> float
        {
            if (! opts.normalize || peak < 1.0e-6f) return 1.0f;
            const float targetLin = std::pow (10.0f, opts.targetPeakDb / 20.0f);
            return targetLin / peak;
        };

        if (type == Type::Mix)
        {
            // 1) メモリへフルレンダ
            juce::AudioBuffer<float> rendered (2, totalSamps);
            const bool ok = renderToBuffer (rendered, state, -1, recSamps,
                                              totalSamps, opts,
                                              [&] (double p) { if (progressCb) progressCb (p * 0.9); });
            if (! ok) return written;

            // 2) ピーク検出 → スケール
            const float peak = rendered.getMagnitude (0, totalSamps);
            const float scale = scaleFromPeak (peak);

            // 3) WAV へ書き出し
            const auto outFile = outputFolder.getChildFile (
                safeName + "_" + bpmTag + "_" + barsTag + "_mix.wav")
                .getNonexistentSibling (false);

            if (writeBuffer (outFile, rendered, scale, opts))
                written.add (outFile);

            if (progressCb) progressCb (1.0);
        }
        else // Stems
        {
            static const char* voiceNames[] = { "kick", "snare", "hihat", "clap", "bass" };
            constexpr int numVoices = 5;

            // 1) 5 ボイスをそれぞれメモリへフルレンダ
            std::vector<juce::AudioBuffer<float>> stems;
            stems.reserve (numVoices);

            for (int v = 0; v < numVoices; ++v)
            {
                stems.emplace_back (2, totalSamps);
                auto& buf = stems.back();

                const double base = (double) v       / (double) (numVoices + 1);
                const double span = 1.0              / (double) (numVoices + 1);
                auto wrapProg = [&, base, span] (double p)
                {
                    if (progressCb) progressCb (base + p * span);
                };

                if (! renderToBuffer (buf, state, v, recSamps, totalSamps, opts, wrapProg))
                    return written;   // 失敗時は中断
            }

            // 2) 全 stems を通したグローバルピークを検出 → 同じスケールを共有
            float globalPeak = 0.0f;
            for (const auto& buf : stems)
                globalPeak = juce::jmax (globalPeak,
                                          buf.getMagnitude (0, totalSamps));
            const float scale = scaleFromPeak (globalPeak);

            // 3) 各 stem を WAV へ書き出し
            for (int v = 0; v < numVoices; ++v)
            {
                const auto outFile = outputFolder.getChildFile (
                    safeName + "_" + bpmTag + "_" + barsTag + "_" + voiceNames[v] + ".wav")
                    .getNonexistentSibling (false);

                if (writeBuffer (outFile, stems[(size_t) v], scale, opts))
                    written.add (outFile);
            }

            if (progressCb) progressCb (1.0);
        }

        return written;
    }

private:
    // soloVoice: -1 = mix, 0..4 = そのボイスのみ SEQ、他は OFF
    static bool renderToBuffer (juce::AudioBuffer<float>& dest,
                                  const juce::MemoryBlock& state,
                                  int  soloVoice,
                                  int  recSamps,
                                  int  totalSamps,
                                  const Options& opts,
                                  std::function<void(double)> progressCb)
    {
        if (dest.getNumChannels() < 2) dest.setSize (2, totalSamps, false, true, false);
        dest.clear();

        // ── クローン processor を作って state を流し込む
        auto proc = std::make_unique<PacificSynthesisProcessor>();
        proc->setStateInformation (state.getData(), (int) state.getSize());
        proc->setPlayConfigDetails (0, 2, opts.sampleRate, opts.blockSize);
        proc->prepareToPlay ((double) opts.sampleRate, opts.blockSize);

        // ステム時: 指定ボイスだけ SEQ、他は OFF
        if (soloVoice >= 0)
        {
            for (int v = 0; v < 5; ++v)
                proc->setVoiceSrc (v, v == soloVoice
                                      ? PacificSynthesisProcessor::kSrcSeq
                                      : PacificSynthesisProcessor::kSrcOff);
        }

        // 再生開始 (internalPpq を 0 にリセットしてから true へ)
        proc->setInternalPlaying (false);
        proc->setInternalPlaying (true);

        juce::AudioBuffer<float> block (2, opts.blockSize);
        juce::MidiBuffer         midi;

        int rendered = 0;
        while (rendered < totalSamps)
        {
            const int thisBlock = juce::jmin (opts.blockSize, totalSamps - rendered);
            block.setSize (2, thisBlock, false, false, true);
            block.clear();
            midi.clear();

            // 記録長を超えたら sequencer を停止 (テール部分は無音入力)
            if (rendered >= recSamps && proc->isInternalPlaying())
                proc->setInternalPlaying (false);

            proc->processBlock (block, midi);

            // 出力バッファへコピー
            for (int c = 0; c < 2; ++c)
                dest.copyFrom (c, rendered, block, c, 0, thisBlock);

            rendered += thisBlock;
            if (progressCb)
                progressCb ((double) rendered / (double) totalSamps);
        }

        return true;
    }

    // dest を scale 倍して WAV ファイルへ書き出し (元バッファは変更しない)
    static bool writeBuffer (const juce::File& outFile,
                              const juce::AudioBuffer<float>& src,
                              float scale,
                              const Options& opts)
    {
        outFile.deleteFile();
        auto fos = std::make_unique<juce::FileOutputStream> (outFile);
        if (! fos->openedOk()) return false;

        juce::WavAudioFormat fmt;
        // JUCE 8 で createWriterFor(stream, sr, ch, bits, ...) は AudioFormatWriterOptions
        // 経由の新 API に置き換え推奨だが旧 API もそのまま動くため、局所的に警告抑制。
        JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
        std::unique_ptr<juce::AudioFormatWriter> writer (
            fmt.createWriterFor (fos.get(),
                                  (double) opts.sampleRate,
                                  2,
                                  opts.bitDepth,
                                  {}, 0));
        JUCE_END_IGNORE_WARNINGS_GCC_LIKE
        if (writer == nullptr) return false;
        fos.release();   // writer がオーナーシップ取得

        // scale=1.0 のときは余計なコピーをしない
        if (std::abs (scale - 1.0f) < 1.0e-4f)
        {
            if (! writer->writeFromAudioSampleBuffer (src, 0, src.getNumSamples()))
                return false;
        }
        else
        {
            // 部分コピーへ scale を適用しつつ書き出し (メモリ二重化を抑える)
            juce::AudioBuffer<float> tmp (2, opts.blockSize);
            const int total = src.getNumSamples();
            int pos = 0;
            while (pos < total)
            {
                const int n = juce::jmin (opts.blockSize, total - pos);
                tmp.setSize (2, n, false, false, true);
                for (int c = 0; c < 2; ++c)
                    tmp.copyFromWithRamp (c, 0, src.getReadPointer (c, pos), n,
                                            scale, scale);
                if (! writer->writeFromAudioSampleBuffer (tmp, 0, n))
                    return false;
                pos += n;
            }
        }

        writer->flush();
        return true;
    }

    // ファイル名に使えない文字を除去
    static juce::String sanitize (const juce::String& s)
    {
        auto trimmed = s.trim();
        auto safe = trimmed.removeCharacters ("/\\:*?\"<>|");
        return safe.isEmpty() ? juce::String ("Pacific") : safe;
    }
};
