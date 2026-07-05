#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "UI/PacificLookAndFeel.h"
#include "UI/StepGridComponent.h"
#include "WavExporter.h"

// ─────────────────────────────────────────────────────────────
//  MidiDragButton
//  - クリック: 通常の onClick (= ファイル保存ダイアログ)
//  - ドラッグ: 一時 .mid を書き出して OS のドラッグ&ドロップを開始
//    DAW のトラックにドロップすると MIDI が挿入される
// ─────────────────────────────────────────────────────────────
class MidiDragButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;

    // 1つだけ書き出す場合 (互換用)
    std::function<juce::MidiFile()> getMidiFile;

    // 複数ファイルを 1 回のドラッグで書き出す場合
    // (ファイル名, MIDIデータ) のリスト。getMidiFiles が設定されていれば優先。
    std::function<std::vector<std::pair<juce::String, juce::MidiFile>>()> getMidiFiles;

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const bool hasFactory = (getMidiFiles != nullptr) || (getMidiFile != nullptr);
        if (! dragStarted && hasFactory && e.getDistanceFromDragStart() > 8)
        {
            dragStarted = true;
            performMidiDrag();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const bool wasDragging = dragStarted;
        dragStarted = false;
        if (! wasDragging)
            juce::TextButton::mouseUp (e);   // 通常クリックとして処理
        else
            repaint();                       // ドラッグ後のビジュアルを戻す
    }

private:
    bool dragStarted = false;

    void performMidiDrag()
    {
        juce::StringArray paths;
        const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory);

        auto writeOne = [&] (const juce::String& fileName, const juce::MidiFile& mf)
        {
            const auto tempFile = tempDir.getChildFile (fileName);
            juce::FileOutputStream stream (tempFile);
            if (stream.openedOk())
            {
                stream.setPosition (0);
                stream.truncate();
                mf.writeTo (stream);
                stream.flush();
                paths.add (tempFile.getFullPathName());
            }
        };

        if (getMidiFiles)
        {
            for (const auto& pair : getMidiFiles())
                writeOne (pair.first, pair.second);
        }
        else if (getMidiFile)
        {
            writeOne ("Pacific_pattern.mid", getMidiFile());
        }

        if (! paths.isEmpty())
            juce::DragAndDropContainer::performExternalDragDropOfFiles (paths, true, this);
    }
};

class PacificSynthesisEditor : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit PacificSynthesisEditor (PacificSynthesisProcessor&);
    ~PacificSynthesisEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // ベース座標 (リサイズ時はこの解像度で内部レイアウトしてから scale 変換する)
    // 縦横比 1058×655 ≈ 1.615:1 (カセットケース 1.57 寄り、bass editor 詰めて compact)
    static constexpr int BaseWidth  = 1058;
    static constexpr int BaseHeight = 655;            // BOTH モード (デフォルト)
    static constexpr int GridHeightBoth    = 335;     // BOTH モードの grid 高さ
    // DRUM-only モードで Clap 行 (4 行目) が縮まないよう、
    // 30(LED+marker)+20(header)+4*32+3*3+8(trailing) = 195 px 確保。
    static constexpr int GridHeightCompact = 195;     // DRUM/BASS only モードの grid 高さ

    // 現在の VIEW モードに応じた effective サイズ
    int getEffectiveGridHeight() const noexcept;
    int getEffectiveBaseHeight() const noexcept;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct LabeledKnob
    {
        juce::Slider knob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attach;
        juce::String paramID;   // MIDI Learn のため (右クリックメニューで使う)
    };

    // 右クリック → MIDI Learn / Clear マッピング ポップアップを表示
    void showMidiCCContextMenu (const juce::String& paramID);

    // MIDI Learn 用 mouse listener の所有 (Editor dtor で解放)
    std::vector<std::unique_ptr<juce::MouseListener>> midiLearnListeners;
    juce::String lastLearnTarget;   // 前回 timer 時点の Learn target

    enum class Page { Main = 0, Kick, Snare, HiHat, Clap, Bass, Comp };

    static juce::Colour colorFromTag (const juce::String& tag);
    void setupKnob (LabeledKnob& k, const juce::String& name,
                    const juce::String& paramID, const juce::String& colorTag);

    void setPage (Page p);
    void applyPageVisibility();
    void layoutKnobRow (std::initializer_list<LabeledKnob*> knobs,
                        int x, int y, int knobW, int knobH);
    void layoutMainPage (juce::Rectangle<int> area);
    void layoutDetailPage (juce::Rectangle<int> area,
                           std::initializer_list<LabeledKnob*> knobs);

    void timerCallback() override;

    void exportMidi();
    // WAV エクスポート (PRESET メニュー → Export WAV 経由)
    void exportWavInteractive (WavExporter::Type type, int bars);
    void runWavExport (juce::File outputFolder, WavExporter::Type type, int bars);
    void showPresetMenu();
    void promptSavePresetAs();
    void refreshPresetButtonLabel();
    void resetToDefaultPreset();

    void paintHeader (juce::Graphics& g, juce::Rectangle<int> bounds);
    void paintScrews (juce::Graphics& g, juce::Rectangle<int> panel);
    void paintSeparator (juce::Graphics& g, int y, int x1, int x2);

    PacificSynthesisProcessor& proc;
    PacificLookAndFeel        laf;

    StepGridComponent grid;

    // Transport
    juce::TextButton playBtn { juce::String::charToString ((juce::juce_wchar) 0x25B6) };
    LabeledKnob      bpmK, swingK, volK;

    // Header buttons
    juce::TextButton undoBtn  { "UNDO" };
    juce::TextButton redoBtn  { "REDO" };
    juce::TextButton rndBtn   { "RND" };
    juce::TextButton clrBtn   { "CLR" };
    MidiDragButton   midiBtn  { juce::String::charToString ((juce::juce_wchar) 0x2193) + juce::String (" MIDI") };
    juce::TextButton presetBtn{ "PRESET" };

    // View mode (BOTH / DRUM / BASS) — 表示中のパートのみ発音
    juce::Label      viewLabel;
    juce::TextButton viewBothBtn { "BOTH" };
    juce::TextButton viewDrumBtn { "DRUM" };
    juce::TextButton viewBassBtn { "BASS" };
    void setViewMode (StepGridComponent::ViewMode vm);

    // SRC mode (SEQ/MIDI 切替, voice 個別) — DAW 側 MIDI で鳴らす時に内蔵パターンを停止
    // 各トラック行 (Kick/Snare/HiHat/Clap/Bass) の右端に 1 ボタン配置:
    //   "SEQ"  = 内蔵 step sequencer (青、デフォルト)
    //   "MIDI" = DAW からの MIDI (赤、ハイライト)
    std::array<juce::TextButton, 5> voiceSrcBtn;  // 0=Kick 1=Snare 2=HiHat 3=Clap 4=Bass
    void setVoiceSrc (int voiceIndex, int mode);
    // ボタン表示を mode に合わせて更新 (SEQ/MIDI/OFF)
    void refreshVoiceSrcButton (int voiceIndex);

    // Undo ヘルパー
    void editPattern (const juce::String& name, std::function<void()> edit);
    void performUndo();
    void performRedo();

    // キーボードショートカット (⌘Z / ⌘Shift+Z)
    bool keyPressed (const juce::KeyPress& key) override;

    PresetManager presetManager { proc };

    // ツールチップ (ボタンに hover した時に説明が出る)
    juce::TooltipWindow tooltipWindow { this, 600 };

    // Page tabs
    juce::TextButton tabMain  { "MAIN" };
    juce::TextButton tabKick  { "KICK" };
    juce::TextButton tabSnare { "SNARE" };
    juce::TextButton tabHiHat { "HIHAT" };
    juce::TextButton tabClap  { "CLAP" };
    juce::TextButton tabBass  { "BASS" };
    juce::TextButton tabComp  { "COMP" };

    // WAVE button (toggle SAW/SQR)
    juce::TextButton waveBtn  { "SAW" };
    juce::Label      waveLabel;
    std::unique_ptr<juce::ParameterAttachment> waveAttach;

    // ── MAIN page knobs ──
    LabeledKnob cutoffK, resoK, envModK, bassDecK, accentK;
    LabeledKnob delayK, reverbK, revModeK, distK;
    // SC (Kick→Bass サイドチェイン) — MAIN ページの FX 側に並べる
    LabeledKnob scAmtK, scRelK;

    // ── KICK page ──
    LabeledKnob kickPitchK, kickAmtK, kickDecK, kickDriveK;

    // ── SNARE page ──
    LabeledKnob snareNoiseK, snareToneK, snareDecK, snareSnapK, snareCrispK;

    // ── HIHAT page ──
    LabeledKnob hhFreqK, hhQK, hhDecK, hhMetalK, hhOpenDecK, hhDriveK;

    // ── CLAP page ──
    LabeledKnob clapFreqK, clapQK, clapDecK, clapSpreadK, clapStyleK;

    // ── BASS extras page ──
    LabeledKnob bassGlideK, bassDriveK, bassSubK;

    // ── BASS LFO (BASS タブに表示) ──
    LabeledKnob bassLfoRateK, bassLfoDepthK, bassLfoTargetK;

    // ── BASS Chord (BASS タブに表示) ──
    LabeledKnob bassChordK;

    // ── COMP page (Master Bus Compressor) ──
    LabeledKnob compThreshK, compRatioK, compAtkK, compRelK;

    // ── 個別 Vol ノブ (各ページに表示) ──
    LabeledKnob kickVolK, snareVolK, hhVolK, clapVolK, bassVolK;

    // ── 個別 Pan ノブ (各音色詳細ページに表示) ──
    LabeledKnob kickPanK, snarePanK, hhPanK, clapPanK, bassPanK;

    Page currentPage = Page::Main;

    juce::Rectangle<int> panelRect;
    juce::Rectangle<int> tabsRect;
    juce::Rectangle<int> separatorRect;
    juce::Rectangle<int> synthParamsRect;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PacificSynthesisEditor)
};
