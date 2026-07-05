#include "PluginEditor.h"
#include "UI/PacificColors.h"
#include "Params.h"
#include "FactoryPresets.h"
#include "WavExporter.h"
#include <map>

juce::Colour PacificSynthesisEditor::colorFromTag (const juce::String& tag)
{
    if (tag == "red")    return PacificColors::red;
    if (tag == "navy")   return PacificColors::navy;
    if (tag == "blue")   return PacificColors::blue;
    if (tag == "teal")   return PacificColors::teal;
    if (tag == "orange") return PacificColors::orange;
    return PacificColors::navy;
}

int PacificSynthesisEditor::getEffectiveGridHeight() const noexcept
{
    return (grid.getViewMode() == StepGridComponent::ViewMode::Both)
           ? GridHeightBoth
           : GridHeightCompact;
}

int PacificSynthesisEditor::getEffectiveBaseHeight() const noexcept
{
    // BOTH の時は元のままだが、DRUM/BASS only ではグリッド差分を引いた高さに
    return BaseHeight - (GridHeightBoth - getEffectiveGridHeight());
}

PacificSynthesisEditor::PacificSynthesisEditor (PacificSynthesisProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      grid (p.pattern, [&p] { return p.sequencer.getCurrentStep(); })
{
    setLookAndFeel (&laf);

    // ─── リサイズ対応 ───
    // 内部は常に 940x680 のベース座標でレイアウトし、resized() で
    // 全ての子コンポーネントに AffineTransform::scale をかけて拡大縮小する。
    // この方式なら既存レイアウト・LookAndFeel コードに一切手を入れずに済み、
    // 文字も画像もベクター描画で綺麗にスケールされる (Retina/Hi-DPI 対応)。
    setResizable (true, true);

   #if JUCE_IOS
    // iOS では端末の画面サイズに合わせて editor を作る。
    // setFixedAspectRatio をかけると iOS Standalone wrapper が画面サイズに
    // リサイズできず、iPad mini (1024 pt) で右端が切れる原因になる。
    // → constrainer は緩めて、画面サイズに直接 setSize する。
    if (auto* c = getConstrainer())
    {
        c->setMinimumSize (320, 240);
        c->setMaximumSize (4096, 4096);
    }
    {
        auto& displays = juce::Desktop::getInstance().getDisplays();
        if (auto* primary = displays.getPrimaryDisplay())
        {
            const auto area = primary->userArea;
            const int w = juce::jmax (area.getWidth(), area.getHeight());   // landscape: long side = width
            const int h = juce::jmin (area.getWidth(), area.getHeight());
            setSize (w, h);
        }
        else
        {
            setSize (BaseWidth, BaseHeight);
        }
    }
   #else
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) BaseWidth / (double) BaseHeight);
        c->setMinimumSize ((int) (BaseWidth * 0.7), (int) (BaseHeight * 0.7));
        c->setMaximumSize ((int) (BaseWidth * 2.0), (int) (BaseHeight * 2.0));
    }
    setSize (BaseWidth, BaseHeight);
   #endif
    setWantsKeyboardFocus (true);   // ⌘Z 受信用

    addAndMakeVisible (grid);
    // パターン編集 → Undo Manager 経由
    grid.setEditCallback ([this] (const juce::String& name,
                                   std::function<void()> edit)
    {
        editPattern (name, std::move (edit));
    });

    // ── Play button ──
    addAndMakeVisible (playBtn);
    playBtn.setComponentID ("btn:play");
    playBtn.setClickingTogglesState (true);
    playBtn.setToggleState (proc.isInternalPlaying(), juce::dontSendNotification);
    playBtn.onClick = [this] {
        proc.setInternalPlaying (playBtn.getToggleState());
        playBtn.setButtonText (playBtn.getToggleState()
                               ? juce::String::charToString ((juce::juce_wchar) 0x25A0)
                               : juce::String::charToString ((juce::juce_wchar) 0x25B6));
    };

    // ── Transport knobs ──
    setupKnob (bpmK,   "BPM",   PacificParams::Bpm,        "red");
    setupKnob (swingK, "SWING", PacificParams::Swing,      "navy");
    setupKnob (volK,   "VOL",   PacificParams::MasterGain, "teal");

    // ── ヘッダーボタン ──
    for (auto* b : { &undoBtn, &redoBtn, &rndBtn, &clrBtn, &presetBtn })
        addAndMakeVisible (*b);
    addAndMakeVisible (midiBtn);

    // ── ビュー切替 (BOTH / DRUM / BASS) ──
    viewLabel.setText ("VIEW", juce::dontSendNotification);
    viewLabel.setFont (juce::Font (juce::FontOptions ("Courier New", 8.5f, juce::Font::bold)));
    viewLabel.setJustificationType (juce::Justification::centredRight);
    viewLabel.setColour (juce::Label::textColourId, PacificColors::textDim);
    addAndMakeVisible (viewLabel);

    for (auto* b : { &viewBothBtn, &viewDrumBtn, &viewBassBtn })
    {
        addAndMakeVisible (*b);
        b->setComponentID ("btn:tab");      // タブと同じスタイル
        b->setRadioGroupId (202);
        b->setClickingTogglesState (true);
    }
    viewBothBtn.setToggleState (true, juce::dontSendNotification);
    viewBothBtn.onClick = [this] { setViewMode (StepGridComponent::ViewMode::Both);     };
    viewDrumBtn.onClick = [this] { setViewMode (StepGridComponent::ViewMode::DrumOnly); };
    viewBassBtn.onClick = [this] { setViewMode (StepGridComponent::ViewMode::BassOnly); };
    viewBothBtn.setTooltip ("Show drums + bass, play both");
    viewDrumBtn.setTooltip ("Show drums only, mute bass");
    viewBassBtn.setTooltip ("Show bass only, mute drums");

    // ── SRC 切替 (voice 個別 3-cycle: SEQ → MIDI → OFF → SEQ) — グリッドの各トラック右端に 1 ボタン ──
    static const char* voiceLabels[5] = { "Kick", "Snare", "HiHat", "Clap", "Bass" };
    for (int i = 0; i < 5; ++i)
    {
        auto& b = voiceSrcBtn[(size_t) i];
        addAndMakeVisible (b);
        // btn:src を使うと PacificLookAndFeel が VOL/CLAP と同じ teal カラーで描画
        b.setComponentID ("btn:src");
        b.setClickingTogglesState (false);   // 3-cycle なので toggleState は使わない

        refreshVoiceSrcButton (i);

        b.onClick = [this, i] {
            // 3-cycle: SEQ (0) → MIDI (1) → OFF (2) → SEQ
            const int next = (proc.getVoiceSrc (i) + 1) % 3;
            setVoiceSrc (i, next);
        };

        b.setTooltip (juce::String (voiceLabels[i])
                      + ": Click to cycle -- SEQ (internal sequencer) -> MIDI (DAW input only) -> OFF (muted)");
    }

    undoBtn.onClick  = [this] { performUndo(); };
    redoBtn.onClick  = [this] { performRedo(); };
    rndBtn.onClick  = [this] {
        editPattern ("Randomize", [this] {
            juce::Random rng; rng.setSeedRandomly();
            // 現在の BPM パラメータを取得してジャンル適合 Kick テンプレを選ぶ
            const float bpm = proc.apvts.getRawParameterValue (PacificParams::Bpm)->load();
            proc.pattern.randomize (rng, bpm);
        });
    };
    clrBtn.onClick   = [this] {
        editPattern ("Clear pattern", [this] { proc.pattern.clear(); });
    };
    midiBtn.onClick  = [this] { exportMidi(); };
    presetBtn.onClick = [this] { showPresetMenu(); };

    // ─ MIDI ボタンのドラッグ動作と説明 ─
    //   ドラッグ時はドラム/ベースを別ファイルに分けて出力する。
    //   こうすると DAW がそれぞれ別トラックとして読み込んでくれて、
    //   ベースのノートが drum kit に当たって変な音を出すのを防げる。
    midiBtn.getMidiFiles = [this]
    {
        std::vector<std::pair<juce::String, juce::MidiFile>> files;
        files.emplace_back ("Pacific_drums.mid", proc.buildDrumMidiFile());
        files.emplace_back ("Pacific_bass.mid",  proc.buildBassMidiFile());
        return files;
    };
    // クリック時は単一ファイル保存 (旧仕様)
    midiBtn.getMidiFile  = [this] { return proc.buildMidiFile(); };
    midiBtn.setTooltip  ("Click: save MIDI file (drums + bass in one file)\n"
                         "Drag: drop onto DAW (drums and bass as separate tracks)");
    undoBtn .setTooltip  ("Undo (Cmd+Z)");
    redoBtn .setTooltip  ("Redo (Cmd+Shift+Z)");
    rndBtn  .setTooltip  ("Randomize pattern");
    clrBtn  .setTooltip  ("Clear pattern");
    presetBtn.setTooltip ("Save / load preset\nClick to open menu");
    playBtn .setTooltip  ("Internal play (sound even when the host isn't playing)");

    refreshPresetButtonLabel();

    // ── WAVE button (toggle SAW/SQR) ──
    waveLabel.setText ("WAVE", juce::dontSendNotification);
    waveLabel.setFont (juce::Font (juce::FontOptions ("Courier New", 8.5f, juce::Font::bold)));
    waveLabel.setJustificationType (juce::Justification::centred);
    waveLabel.setColour (juce::Label::textColourId, PacificColors::textDim);
    addAndMakeVisible (waveLabel);

    addAndMakeVisible (waveBtn);
    waveBtn.setComponentID ("btn:wave");
    if (auto* param = proc.apvts.getParameter (PacificParams::BassWave))
    {
        waveAttach = std::make_unique<juce::ParameterAttachment> (
            *param,
            [this] (float v) { waveBtn.setButtonText (v < 0.5f ? "SAW" : "SQR"); });
        waveAttach->sendInitialUpdate();

        waveBtn.onClick = [this, param] {
            const float curr = param->getValue();   // normalised 0/1
            waveAttach->setValueAsCompleteGesture (curr < 0.5f ? 1.0f : 0.0f);
        };
    }

    // ── Page tabs ──
    juce::TextButton* tabs[] = { &tabMain, &tabKick, &tabSnare, &tabHiHat, &tabClap, &tabBass, &tabComp };
    Page tabPages[] = { Page::Main, Page::Kick, Page::Snare, Page::HiHat, Page::Clap, Page::Bass, Page::Comp };
    for (int i = 0; i < 7; ++i)
    {
        addAndMakeVisible (*tabs[i]);
        tabs[i]->setComponentID ("btn:tab");
        tabs[i]->setRadioGroupId (101);
        tabs[i]->setClickingTogglesState (true);
        auto page = tabPages[i];
        tabs[i]->onClick = [this, page] { setPage (page); };
    }
    tabMain.setToggleState (true, juce::dontSendNotification);

    // ── MAIN page knobs ──
    setupKnob (cutoffK,  "CUTOFF",  PacificParams::Cutoff,  "red");
    setupKnob (resoK,    "RESO",    PacificParams::Reso,    "red");
    setupKnob (envModK,  "ENV MOD", PacificParams::EnvMod,  "red");
    setupKnob (bassDecK, "DECAY",   PacificParams::BassDec, "orange");
    setupKnob (accentK,  "ACCENT",  PacificParams::AccAmt,  "orange");
    setupKnob (delayK,   "DELAY",   PacificParams::DelMix,  "navy");
    setupKnob (reverbK,  "REVERB",  PacificParams::RevMix,  "navy");
    setupKnob (revModeK, "REV MODE",PacificParams::RevMode, "navy");
    setupKnob (distK,    "DIST",    PacificParams::Dist,    "teal");
    setupKnob (scAmtK,   "SC AMT",  PacificParams::ScAmt,   "red");
    setupKnob (scRelK,   "SC REL",  PacificParams::ScRel,   "red");

    // ── KICK page ──
    setupKnob (kickPitchK, "PITCH",  PacificParams::KickPitch, "red");
    setupKnob (kickAmtK,   "AMOUNT", PacificParams::KickAmt,   "red");
    setupKnob (kickDecK,   "DECAY",  PacificParams::KickDec,   "orange");
    setupKnob (kickDriveK, "DRIVE",  PacificParams::KickDrive, "teal");

    // ── SNARE page ──
    setupKnob (snareNoiseK, "NOISE", PacificParams::SnareNoise, "blue");
    setupKnob (snareToneK,  "TONE",  PacificParams::SnareTone,  "blue");
    setupKnob (snareDecK,   "DECAY", PacificParams::SnareDec,   "orange");
    setupKnob (snareSnapK,  "SNAP",  PacificParams::SnareSnap,  "teal");
    setupKnob (snareCrispK, "CRISP", PacificParams::SnareCrisp, "teal");

    // ── HIHAT page ──
    setupKnob (hhFreqK,    "FREQ",    PacificParams::HhFreq,    "orange");
    setupKnob (hhQK,       "Q",       PacificParams::HhQ,       "orange");
    setupKnob (hhDecK,     "CLOSED",  PacificParams::HhDec,     "orange");
    setupKnob (hhOpenDecK, "OPEN",    PacificParams::HhOpenDec, "orange");
    setupKnob (hhMetalK,   "METAL",   PacificParams::HhMetal,   "teal");
    setupKnob (hhDriveK,   "DRIVE",   PacificParams::HhDrive,   "teal");

    // ── CLAP page ──
    setupKnob (clapFreqK,   "FREQ",   PacificParams::ClapFreq,   "teal");
    setupKnob (clapQK,      "Q",      PacificParams::ClapQ,      "teal");
    setupKnob (clapDecK,    "DECAY",  PacificParams::ClapDec,    "orange");
    setupKnob (clapSpreadK, "SPREAD", PacificParams::ClapSpread, "teal");
    setupKnob (clapStyleK,  "STYLE",  PacificParams::ClapStyle,  "teal");

    // ── BASS extras ──
    setupKnob (bassGlideK, "GLIDE", PacificParams::BassGlide, "navy");
    setupKnob (bassDriveK, "DRIVE", PacificParams::BassDrive, "teal");
    setupKnob (bassSubK,   "SUB",   PacificParams::BassSub,   "navy");

    // ── COMP page (Master Bus Compressor) ──
    setupKnob (compThreshK, "THRESH",  PacificParams::CompThresh, "red");
    setupKnob (compRatioK,  "RATIO",   PacificParams::CompRatio,  "red");
    setupKnob (compAtkK,    "ATTACK",  PacificParams::CompAtk,    "orange");
    setupKnob (compRelK,    "RELEASE", PacificParams::CompRel,    "orange");

    // ── BASS LFO ──
    setupKnob (bassLfoRateK,   "LFO RATE",  PacificParams::BassLfoRate,   "teal");
    setupKnob (bassLfoDepthK,  "LFO DEPTH", PacificParams::BassLfoDepth,  "teal");
    setupKnob (bassLfoTargetK, "LFO DST",   PacificParams::BassLfoTarget, "teal");
    setupKnob (bassChordK,     "CHORD",     PacificParams::BassChord,     "navy");

    // ── 個別 Vol (楽器カラーで識別) ──
    setupKnob (kickVolK,  "VOL", PacificParams::KickVol,  "red");
    setupKnob (snareVolK, "VOL", PacificParams::SnareVol, "blue");
    setupKnob (hhVolK,    "VOL", PacificParams::HhVol,    "orange");
    setupKnob (clapVolK,  "VOL", PacificParams::ClapVol,  "teal");
    setupKnob (bassVolK,  "VOL", PacificParams::BassVol,  "navy");

    // ── 個別 Pan (詳細ページに表示) ──
    setupKnob (kickPanK,  "PAN", PacificParams::KickPan,  "red");
    setupKnob (snarePanK, "PAN", PacificParams::SnarePan, "blue");
    setupKnob (hhPanK,    "PAN", PacificParams::HhPan,    "orange");
    setupKnob (clapPanK,  "PAN", PacificParams::ClapPan,  "teal");
    setupKnob (bassPanK,  "PAN", PacificParams::BassPan,  "navy");

    // ─ ツールチップ (新ノブのみ。既存ノブは別 PR で追加予定)
    scAmtK.knob.setTooltip ("Sidechain amount -- duck the bass when the kick hits");
    scRelK.knob.setTooltip ("Sidechain release time (seconds) -- shorter = pumpier");
    hhDecK.knob.setTooltip ("Closed HiHat decay (seconds)");
    hhOpenDecK.knob.setTooltip ("Open HiHat decay (seconds) -- triggered by MIDI A#1 / G#5");
    for (auto* k : { &kickPanK, &snarePanK, &hhPanK, &clapPanK, &bassPanK })
        k->knob.setTooltip ("Stereo pan (constant-power) -- L <- center -> R");
    compThreshK.knob.setTooltip ("Compressor threshold in dB -- 0 dB = bypass");
    compRatioK .knob.setTooltip ("Compression ratio (1:1 = none, 4:1 = typical, 20:1 = limiter)");
    compAtkK   .knob.setTooltip ("Attack time in ms -- shorter = faster transient clamp");
    compRelK   .knob.setTooltip ("Release time in ms -- longer = smoother but more pumping");
    bassLfoRateK  .knob.setTooltip ("Bass LFO rate in Hz");
    bassLfoDepthK .knob.setTooltip ("Bass LFO depth -- 0 = bypass");
    bassLfoTargetK.knob.setTooltip ("Bass LFO destination -- 0=Cutoff (+/-2 oct), 1=Pitch (+/-1 semi)");
    snareCrispK.knob.setTooltip ("Snare crispness -- 0=lo-fi/dusty, 1=bright HP (909-style)");
    hhDriveK.knob.setTooltip ("HiHat drive -- 0=clean, 1=saturated/aggressive");
    clapStyleK.knob.setTooltip ("Clap style -- 0=single burst, 0.5=707 (2 bursts), 1=909 (3 bursts)");
    revModeK.knob.setTooltip ("Reverb mode -- 0=Plate (bright/short), 1=Room (default), 2=Hall (long/dark)");
    bassChordK.knob.setTooltip ("Bass chord mode -- Off / +Oct / Power(5th) / Maj / Min / Sus4 (auto-stack harmony from root)");

    applyPageVisibility();
    startTimerHz (10);

    // Editor 再オープン時 (DAW 上で別 VST から戻った時など) に
    // 前回の VIEW モード状態を Processor から読み戻して復元する。
    const int savedVm = proc.getSavedViewMode();
    if (savedVm != (int) StepGridComponent::ViewMode::Both)
    {
        // BOTH 以外なら明示的に切替 (setViewMode 内で window resize される)
        setViewMode ((StepGridComponent::ViewMode) savedVm);
    }
}

PacificSynthesisEditor::~PacificSynthesisEditor()
{
    setLookAndFeel (nullptr);
}

void PacificSynthesisEditor::setupKnob (LabeledKnob& k, const juce::String& name,
                                       const juce::String& paramID,
                                       const juce::String& colorTag)
{
    const auto col = colorFromTag (colorTag);

    k.knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.knob.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 64, 16);
    k.knob.setComponentID ("knob:" + colorTag);
    k.knob.setColour (juce::Slider::textBoxTextColourId, col);
    addAndMakeVisible (k.knob);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (juce::Font (juce::FontOptions ("Courier New", 8.5f, juce::Font::bold)));
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, PacificColors::textDim);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<SliderAttachment> (proc.apvts, paramID, k.knob);
    k.paramID = paramID;

    // ─ 右クリック → MIDI Learn メニュー (Mac は ctrl+click も popup 扱い)
    k.knob.setPopupMenuEnabled (false);   // JUCE 標準の popup は無効化
    // mouseListener パターンで右クリック検出
    class RClickHandler : public juce::MouseListener
    {
    public:
        RClickHandler (PacificSynthesisEditor& e, juce::String pid)
            : editor (e), paramID (std::move (pid)) {}
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())     // 右クリック or Ctrl+click
                editor.showMidiCCContextMenu (paramID);
        }
    private:
        PacificSynthesisEditor& editor;
        juce::String paramID;
    };
    // ヒープ確保した listener を knob 上に install (Editor が生きてる間有効)
    auto* h = new RClickHandler (*this, paramID);
    k.knob.addMouseListener (h, true);
    // listener の所有: Editor のリストに保存して dtor 内で解放
    midiLearnListeners.push_back (std::unique_ptr<juce::MouseListener> (h));
}

void PacificSynthesisEditor::showMidiCCContextMenu (const juce::String& paramID)
{
    juce::PopupMenu m;
    const int currentCC = proc.getCCForParam (paramID);
    const bool isLearning = (proc.getLearnTargetParam() == paramID);

    if (isLearning)
    {
        m.addItem ("Cancel MIDI Learn", [this] { proc.cancelMidiLearn(); });
    }
    else
    {
        const juce::String label = currentCC >= 0
            ? "Re-learn MIDI CC (current: CC " + juce::String (currentCC) + ")"
            : juce::String ("Learn MIDI CC...");
        m.addItem (label, [this, paramID] { proc.startMidiLearn (paramID); });
    }

    if (currentCC >= 0)
    {
        m.addItem ("Clear MIDI mapping (CC " + juce::String (currentCC) + ")",
                   [this, paramID] { proc.clearMidiMapping (paramID); });
    }

    m.showMenuAsync (juce::PopupMenu::Options());
}

void PacificSynthesisEditor::setPage (Page p)
{
    currentPage = p;
    applyPageVisibility();
    resized();
    repaint();
}

void PacificSynthesisEditor::applyPageVisibility()
{
    auto set = [] (LabeledKnob& k, bool v) {
        k.knob.setVisible (v);
        k.label.setVisible (v);
    };

    const bool main  = (currentPage == Page::Main);
    const bool kick  = (currentPage == Page::Kick);
    const bool snare = (currentPage == Page::Snare);
    const bool hh    = (currentPage == Page::HiHat);
    const bool clap  = (currentPage == Page::Clap);
    const bool bass  = (currentPage == Page::Bass);
    const bool comp  = (currentPage == Page::Comp);

    // WAVE button: BASS の時だけ
    waveBtn.setVisible   (bass);
    waveLabel.setVisible (bass);

    // BASS系 knobs (CUTOFF/RESO/ENV/DECAY/ACCENT): BASS のみ
    set (cutoffK,  bass);
    set (resoK,    bass);
    set (envModK,  bass);
    set (bassDecK, bass);
    set (accentK,  bass);

    // FX: MAIN のみ (SC も含む)
    set (delayK,   main);
    set (reverbK,  main);
    set (revModeK, main);
    set (distK,    main);
    set (scAmtK,   main);
    set (scRelK,   main);

    // ─ KICK
    set (kickPitchK, kick);
    set (kickAmtK,   kick);
    set (kickDecK,   kick);
    set (kickDriveK, kick);

    // ─ SNARE
    set (snareNoiseK, snare);
    set (snareToneK,  snare);
    set (snareDecK,   snare);
    set (snareSnapK,  snare);
    set (snareCrispK, snare);

    // ─ HIHAT (closed/open decay は別ノブ)
    set (hhFreqK,    hh);
    set (hhQK,       hh);
    set (hhDecK,     hh);
    set (hhOpenDecK, hh);
    set (hhMetalK,   hh);
    set (hhDriveK,   hh);

    // ─ CLAP
    set (clapFreqK,   clap);
    set (clapQK,      clap);
    set (clapDecK,    clap);
    set (clapSpreadK, clap);
    set (clapStyleK,  clap);

    // ─ BASS extras
    set (bassGlideK, bass);
    set (bassDriveK, bass);
    set (bassSubK,   bass);
    set (bassLfoRateK,   bass);
    set (bassLfoDepthK,  bass);
    set (bassLfoTargetK, bass);
    set (bassChordK,     bass);

    // ─ VOL knobs: MAIN と該当ページの両方で表示
    set (kickVolK,  main || kick);
    set (snareVolK, main || snare);
    set (hhVolK,    main || hh);
    set (clapVolK,  main || clap);
    set (bassVolK,  main || bass);

    // ─ PAN knobs: 各音色詳細ページのみ表示 (MAIN には出さず混雑を避ける)
    set (kickPanK,  kick);
    set (snarePanK, snare);
    set (hhPanK,    hh);
    set (clapPanK,  clap);
    set (bassPanK,  bass);

    // ─ COMP page knobs
    set (compThreshK, comp);
    set (compRatioK,  comp);
    set (compAtkK,    comp);
    set (compRelK,    comp);

    // ─ ラベル切替: MAIN では楽器名、詳細ページでは "VOL"
    kickVolK .label.setText (main ? "KICK"  : "VOL", juce::dontSendNotification);
    snareVolK.label.setText (main ? "SNARE" : "VOL", juce::dontSendNotification);
    hhVolK   .label.setText (main ? "HIHAT" : "VOL", juce::dontSendNotification);
    clapVolK .label.setText (main ? "CLAP"  : "VOL", juce::dontSendNotification);
    bassVolK .label.setText (main ? "BASS"  : "VOL", juce::dontSendNotification);
}

void PacificSynthesisEditor::timerCallback()
{
    const bool actuallyPlaying = proc.isInternalPlaying();
    if (playBtn.getToggleState() != actuallyPlaying)
    {
        playBtn.setToggleState (actuallyPlaying, juce::dontSendNotification);
        playBtn.setButtonText (actuallyPlaying
                               ? juce::String::charToString ((juce::juce_wchar) 0x25A0)
                               : juce::String::charToString ((juce::juce_wchar) 0x25B6));
    }
    // UNDO/REDO ボタンの有効/無効
    undoBtn.setEnabled (proc.patternUndoManager.canUndo());
    redoBtn.setEnabled (proc.patternUndoManager.canRedo());

    // ─ MIDI Learn 中のビジュアルフィードバック (該当 knob を赤外枠で点滅)
    // 簡素化: tooltip だけ更新 (色変更は LookAndFeel 改修が必要なので省略)
    const auto lt = proc.getLearnTargetParam();
    if (lt != lastLearnTarget)
    {
        lastLearnTarget = lt;
        repaint();
    }
}

// ─── Undo / Redo ───
void PacificSynthesisEditor::editPattern (const juce::String& name,
                                          std::function<void()> edit)
{
    if (! edit) return;
    // 編集前のパターンを ValueTree でスナップショット
    auto before = proc.pattern.toValueTree();
    edit();
    auto after  = proc.pattern.toValueTree();

    // 実際に変更がない場合は何もしない (空 transaction を作らない)
    if (before.isEquivalentTo (after))
    {
        grid.repaint();
        return;
    }

    // UndoManager にトランザクションとして登録
    auto& um = proc.patternUndoManager;
    um.beginNewTransaction (name);
    um.perform (new PatternUndoAction (proc.pattern, before, after));
    // ⭐ 直後にもう一度 beginNewTransaction を呼んで「現 transaction を確定」する。
    // これをしないと現 transaction が open のまま次の編集と統合されてしまい、
    // 結果として 1 ステップずつ undo できなくなることがある (今回の不具合の原因)。
    um.beginNewTransaction();

    grid.repaint();
}

void PacificSynthesisEditor::setViewMode (StepGridComponent::ViewMode vm)
{
    grid.setViewMode (vm);
    // 表示中のパートのみ発音させる (ミュートはオーディオスレッドが atomic で読み取り)
    proc.setDrumsMuted (vm == StepGridComponent::ViewMode::BassOnly);
    proc.setBassMuted  (vm == StepGridComponent::ViewMode::DrumOnly);

    // RadioGroup の同期 (キーボード等から呼ばれた時のため)
    auto sync = [] (juce::TextButton& b, bool on) {
        if (b.getToggleState() != on)
            b.setToggleState (on, juce::dontSendNotification);
    };
    sync (viewBothBtn, vm == StepGridComponent::ViewMode::Both);
    sync (viewDrumBtn, vm == StepGridComponent::ViewMode::DrumOnly);
    sync (viewBassBtn, vm == StepGridComponent::ViewMode::BassOnly);

    // Editor を閉じて開き直しても VIEW モードを維持できるよう Processor 側に保存
    proc.setSavedViewMode ((int) vm);

    // VIEW モードに応じてウィンドウ高さを変える (横幅は維持)
    const int newBaseH = getEffectiveBaseHeight();
    const float currentScale = (float) getWidth() / (float) BaseWidth;
    const int newWindowH = (int) std::round ((float) newBaseH * currentScale);

    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) BaseWidth / (double) newBaseH);
        // min/max も新しい base height に合わせる
        c->setMinimumSize ((int) (BaseWidth * 0.7), (int) (newBaseH * 0.7));
        c->setMaximumSize ((int) (BaseWidth * 2.0), (int) (newBaseH * 2.0));
    }

    setSize (getWidth(), newWindowH);

    // setSize は同じサイズだと resized() を呼ばないことがあるので明示
    resized();
}

void PacificSynthesisEditor::setVoiceSrc (int voiceIndex, int mode)
{
    if (voiceIndex < 0 || voiceIndex >= (int) voiceSrcBtn.size()) return;
    proc.setVoiceSrc (voiceIndex, mode);
    refreshVoiceSrcButton (voiceIndex);
}

void PacificSynthesisEditor::refreshVoiceSrcButton (int voiceIndex)
{
    if (voiceIndex < 0 || voiceIndex >= (int) voiceSrcBtn.size()) return;
    auto& b = voiceSrcBtn[(size_t) voiceIndex];
    const int mode = proc.getVoiceSrc (voiceIndex);
    // 表示: SEQ / MIDI / OFF
    const char* label = (mode == 0) ? "SEQ"
                      : (mode == 1) ? "MIDI"
                                    : "OFF";
    b.setButtonText (label);
    // OFF はトグル ON 風 (色変化が出るよう toggleState を流用)
    // SEQ=off-look, MIDI=highlight, OFF=highlight (LookAndFeel が描画)
    b.setToggleState (mode != 0, juce::dontSendNotification);
}

void PacificSynthesisEditor::performUndo()
{
    if (proc.patternUndoManager.canUndo())
    {
        proc.patternUndoManager.undo();
        grid.repaint();
    }
}

void PacificSynthesisEditor::performRedo()
{
    if (proc.patternUndoManager.canRedo())
    {
        proc.patternUndoManager.redo();
        grid.repaint();
    }
}

bool PacificSynthesisEditor::keyPressed (const juce::KeyPress& key)
{
    // ⌘Z = Undo / ⌘⇧Z = Redo (Mac標準)
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z')
    {
        if (key.getModifiers().isShiftDown()) performRedo();
        else                                  performUndo();
        return true;
    }
    // Y も Redo にしておく (Windows標準)
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Y')
    {
        performRedo();
        return true;
    }
    return false;
}

// ─── 描画 ───
void PacificSynthesisEditor::paint (juce::Graphics& g)
{
    const int effH = getEffectiveBaseHeight();

    // letterbox 背景 (スケール後にコンテンツが収まらない領域を塗りつぶす)
    g.fillAll (PacificColors::panelDark);

    // スケール係数: 幅と高さのうち小さい方 → 必ず画面内に収まる
    const float scaleX = (float) getWidth()  / (float) BaseWidth;
    const float scaleY = (float) getHeight() / (float) effH;
    const float scale  = juce::jmin (scaleX, scaleY);

    // 余白を均等に分けてセンタリング
    const float scaledW = BaseWidth * scale;
    const float scaledH = (float) effH * scale;
    const float offX    = ((float) getWidth()  - scaledW) * 0.5f;
    const float offY    = ((float) getHeight() - scaledH) * 0.5f;

    if (std::abs (scale - 1.0f) > 0.0005f || offX > 0.5f || offY > 0.5f)
        g.addTransform (juce::AffineTransform::scale (scale).translated (offX, offY));

    // 以降はベース座標 (BaseWidth × effective height) で描画
    juce::Rectangle<int> baseBounds (0, 0, BaseWidth, effH);
    juce::ColourGradient bg (PacificColors::panelDark, 0, 0,
                             PacificColors::cream, 0, (float) effH * 0.1f, false);
    bg.addColour (0.97, PacificColors::cream);
    g.setGradientFill (bg);
    g.fillRect (baseBounds);

    auto header = baseBounds.removeFromTop (54).reduced (14, 8);
    paintHeader (g, header);

    g.setColour (PacificColors::cream);
    g.fillRoundedRectangle (panelRect.toFloat(), 6.0f);
    g.setColour (PacificColors::aluminum);
    g.drawRoundedRectangle (panelRect.toFloat().reduced (1.0f), 6.0f, 2.0f);
    g.setColour (PacificColors::creamLight.withAlpha (0.6f));
    g.drawRoundedRectangle (panelRect.toFloat().reduced (2.5f), 5.0f, 1.0f);

    paintScrews (g, panelRect);

    if (! separatorRect.isEmpty())
        paintSeparator (g, separatorRect.getY(), separatorRect.getX(), separatorRect.getRight());

    // MAIN ページの MIX | FX 仕切り線 + セクションラベル
    if (currentPage == Page::Main && ! synthParamsRect.isEmpty())
    {
        const int knobW = 70;
        const int gap   = 22;
        const int totalW = knobW * 5 + gap + knobW * 6;
        const int leftX  = synthParamsRect.getX() + (synthParamsRect.getWidth() - totalW) / 2;
        const int dividerX = leftX + knobW * 5 + gap / 2;

        // 縦の仕切り線
        g.setColour (PacificColors::panelDark);
        g.drawLine ((float) dividerX, (float) synthParamsRect.getY() + 6,
                    (float) dividerX, (float) synthParamsRect.getBottom() - 6, 1.0f);

        // セクションラベル
        g.setColour (PacificColors::textDim);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 7.5f, juce::Font::bold)));
        g.drawText ("MIX BUS", leftX, synthParamsRect.getY() - 11,
                    knobW * 5, 10, juce::Justification::centred, false);
        g.drawText ("FX + SIDECHAIN", leftX + knobW * 5 + gap, synthParamsRect.getY() - 11,
                    knobW * 6, 10, juce::Justification::centred, false);
    }

    // ─ MIDI Learn 中の knob を赤い枠でハイライト
    const auto learnTarget = proc.getLearnTargetParam();
    if (learnTarget.isNotEmpty())
    {
        // すべての LabeledKnob を走査して該当 paramID の knob に枠を描く
        // ※ g には既に scale 変換が適用されているので、knob は変換前の生 bounds (getBounds)
        //   を使う。getBoundsInParent() は transform 適用済みで二重スケーリングになる。
        const auto highlight = [&] (const LabeledKnob& k)
        {
            if (k.paramID == learnTarget && k.knob.isVisible())
            {
                auto b = k.knob.getBounds().expanded (3);
                g.setColour (PacificColors::red.withAlpha (0.85f));
                g.drawRoundedRectangle (b.toFloat(), 4.0f, 2.5f);
            }
        };
        for (auto* k : { &cutoffK, &resoK, &envModK, &bassDecK, &accentK,
                          &delayK, &reverbK, &revModeK, &distK, &scAmtK, &scRelK,
                          &kickPitchK, &kickAmtK, &kickDecK, &kickDriveK,
                          &snareNoiseK, &snareToneK, &snareDecK, &snareSnapK, &snareCrispK,
                          &hhFreqK, &hhQK, &hhDecK, &hhOpenDecK, &hhMetalK, &hhDriveK,
                          &clapFreqK, &clapQK, &clapDecK, &clapSpreadK, &clapStyleK,
                          &bassGlideK, &bassDriveK, &bassSubK,
                          &bassLfoRateK, &bassLfoDepthK, &bassLfoTargetK, &bassChordK,
                          &compThreshK, &compRatioK, &compAtkK, &compRelK,
                          &bpmK, &swingK, &volK,
                          &kickVolK, &snareVolK, &hhVolK, &clapVolK, &bassVolK,
                          &kickPanK, &snarePanK, &hhPanK, &clapPanK, &bassPanK })
            highlight (*k);
    }
}

void PacificSynthesisEditor::paintHeader (juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::ColourGradient grad (PacificColors::red,     0, (float) bounds.getY(),
                               PacificColors::redDark, 0, (float) bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds.toFloat(), 5.0f);
    g.setColour (PacificColors::redDark);
    g.drawRoundedRectangle (bounds.toFloat(), 5.0f, 1.0f);

    g.setColour (juce::Colour::fromFloatRGBA (0, 0, 0, 0.2f));
    g.drawRoundedRectangle (bounds.toFloat().translated (0, 2.0f), 5.0f, 0.8f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions ("Courier New", 22.0f, juce::Font::bold)));
    g.drawText ("PACIFIC SYNTHESIS", bounds,
                juce::Justification::centred, false);
}

void PacificSynthesisEditor::paintScrews (juce::Graphics& g, juce::Rectangle<int> panel)
{
    const int s = 9;
    const int m = 6;
    for (auto pt : { juce::Point<int> (panel.getX() + m,         panel.getY() + m),
                     juce::Point<int> (panel.getRight() - m - s, panel.getY() + m),
                     juce::Point<int> (panel.getX() + m,         panel.getBottom() - m - s),
                     juce::Point<int> (panel.getRight() - m - s, panel.getBottom() - m - s) })
    {
        juce::Rectangle<float> r ((float) pt.x, (float) pt.y, (float) s, (float) s);
        juce::ColourGradient grad (PacificColors::aluminumLight, r.getX() + 2, r.getY() + 2,
                                   PacificColors::aluminum, r.getRight(), r.getBottom(), true);
        g.setGradientFill (grad);
        g.fillEllipse (r);
        g.setColour (PacificColors::textDim);
        g.drawEllipse (r, 0.6f);
        g.setColour (PacificColors::text.withAlpha (0.45f));
        g.drawLine (r.getX() + 1.5f, r.getCentreY(),
                    r.getRight() - 1.5f, r.getCentreY(), 0.6f);
    }
}

void PacificSynthesisEditor::paintSeparator (juce::Graphics& g, int y, int x1, int x2)
{
    g.setColour (PacificColors::panelDark);
    g.drawLine ((float) x1, (float) y, (float) x2, (float) y, 1.0f);
}

// ─── レイアウト ───
void PacificSynthesisEditor::layoutKnobRow (std::initializer_list<LabeledKnob*> knobs,
                                            int x, int y, int knobW, int knobH)
{
    int px = x;
    for (auto* k : knobs)
    {
        k->label.setBounds (px, y + 2,  knobW, 12);
        k->knob .setBounds (px, y + 16, knobW, knobH - 16);
        px += knobW;
    }
}

void PacificSynthesisEditor::layoutMainPage (juce::Rectangle<int> area)
{
    // MIX (5 VOLs) + GAP + FX (DELAY/REVERB/REV MODE/DIST/SC AMT/SC REL = 6)
    const int knobW = 70;     // 5+6 ノブにするので少し縮める
    const int gap   = 22;     // MIX と FX の間
    const int totalW = knobW * 5 + gap + knobW * 6;

    int px = area.getX() + (area.getWidth() - totalW) / 2;
    const int y = area.getY() + 4;
    const int h = area.getHeight() - 8;

    layoutKnobRow ({ &kickVolK, &snareVolK, &hhVolK, &clapVolK, &bassVolK },
                   px, y, knobW, h);
    px += knobW * 5 + gap;
    layoutKnobRow ({ &delayK, &reverbK, &revModeK, &distK, &scAmtK, &scRelK }, px, y, knobW, h);
}

void PacificSynthesisEditor::layoutDetailPage (juce::Rectangle<int> area,
                                               std::initializer_list<LabeledKnob*> knobs)
{
    const int n = (int) knobs.size();
    const int knobW = 90;
    const int y = area.getY() + 4;
    const int h = area.getHeight() - 8;
    const int totalW = knobW * n;
    const int px = area.getX() + (area.getWidth() - totalW) / 2;
    layoutKnobRow (knobs, px, y, knobW, h);
}

void PacificSynthesisEditor::resized()
{
    // 内部は常にベース解像度でレイアウト。最後に scale 変換を子に適用。
    // (VIEW モードに応じて H が縮む)
    const int W = BaseWidth;
    const int H = getEffectiveBaseHeight();
    panelRect = juce::Rectangle<int> (12, 60, W - 24, H - 72);

    const int innerX = panelRect.getX() + 20;
    const int innerW = panelRect.getWidth() - 40;
    int y = panelRect.getY() + 14;

    // ── Transport ──
    playBtn.setBounds (innerX, y, 44, 44);

    const int kw = 64, kbody = 50;
    auto placeTransportKnob = [&] (LabeledKnob& k, int x) {
        k.label.setBounds (x, y + 2, kw, 12);
        k.knob .setBounds (x, y + 16, kw, kbody);
    };
    placeTransportKnob (bpmK,   innerX + 50);
    placeTransportKnob (swingK, innerX + 50 + kw);
    placeTransportKnob (volK,   innerX + 50 + kw * 2);

    const int btnY = y + 12;
    const int btnH = 26;
    const int presetW = 130;
    const int midiW   = 72;
    const int smBtnW  = 50;
    const int undoW   = 42;
    int bx = innerX + innerW;
    bx -= presetW;     presetBtn.setBounds (bx, btnY, presetW, btnH);
    bx -= 6 + midiW;   midiBtn  .setBounds (bx, btnY, midiW, btnH);
    bx -= 6 + smBtnW;  clrBtn   .setBounds (bx, btnY, smBtnW, btnH);
    bx -= 6 + smBtnW;  rndBtn   .setBounds (bx, btnY, smBtnW, btnH);
    bx -= 6 + undoW;   redoBtn  .setBounds (bx, btnY, undoW, btnH);
    bx -= 4 + undoW;   undoBtn  .setBounds (bx, btnY, undoW, btnH);

    // ヘッダー領域 (Play/BPM/SWING/VOL + 右のヘッダーボタン群)
    y += 60;

    // ── View 切替バー (ヘッダーとグリッドの間, 右寄せ) ──
    const int viewBarH = 22;
    const int viewBtnW = 56;
    const int viewLabelW = 40;
    {
        const int totalW = viewLabelW + (viewBtnW + 3) * 3 - 3;
        // SEQ ボタン列と同じ右端に合わせる (innerW 右端から 8 pt 内側)。
        // 元は innerW 右端ピタリだったが、iPad の縦横比が違うと
        // ピクセル丸めで BASS ボタンが画面外に出てしまうため安全余白を取る。
        int vx = innerX + innerW - 8 - totalW;
        viewLabel  .setBounds (vx, y, viewLabelW, viewBarH); vx += viewLabelW + 4;
        viewBothBtn.setBounds (vx, y, viewBtnW, viewBarH);   vx += viewBtnW + 3;
        viewDrumBtn.setBounds (vx, y, viewBtnW, viewBarH);   vx += viewBtnW + 3;
        viewBassBtn.setBounds (vx, y, viewBtnW, viewBarH);
    }

    y += viewBarH + 6;

    // ── Step Grid ── (VIEW モードに応じて高さが変わる)
    const int gridH = getEffectiveGridHeight();
    grid.setBounds (innerX, y, innerW, gridH);

    // ── 各トラック右端の SEQ/MIDI トグル (1 ボタン) ──
    // グリッド内部の各行 bounds を取得して、その右側 (kRightMargin 領域) に配置
    auto placeVoiceSrc = [this] (int voiceIndex, juce::Rectangle<int> rowBounds)
    {
        if (rowBounds.isEmpty()) {
            voiceSrcBtn[(size_t) voiceIndex].setBounds ({});
            return;
        }
        // rowBounds は grid のローカル座標なので global へ変換
        auto gp = grid.getPosition();
        const int rowYGlobal = rowBounds.getY() + gp.y;
        const int rowH       = rowBounds.getHeight();
        // ヘッダー側の btnH/btnY と名前がかぶらないよう vs* prefix
        const int vsBtnH     = juce::jmin (24, rowH - 2);
        const int vsBtnY     = rowYGlobal + (rowH - vsBtnH) / 2;
        const int vsBtnW     = 56;
        // 右余白の中に縦中央寄せ + 右寄せ
        const int rightEdge  = grid.getRight() - 8;
        const int vsBtnX     = rightEdge - vsBtnW;
        voiceSrcBtn[(size_t) voiceIndex].setBounds (vsBtnX, vsBtnY, vsBtnW, vsBtnH);
    };

    placeVoiceSrc (0, grid.getDrumRowBounds (0));   // Kick
    placeVoiceSrc (1, grid.getDrumRowBounds (1));   // Snare
    placeVoiceSrc (2, grid.getDrumRowBounds (2));   // HiHat
    placeVoiceSrc (3, grid.getDrumRowBounds (3));   // Clap
    placeVoiceSrc (4, grid.getBassRowBounds());     // Bass

    y += gridH + 8;

    // ── Separator ──
    separatorRect = juce::Rectangle<int> (innerX, y, innerW, 1);
    y += 6;

    // ── Page tabs (7 タブ: MAIN/KICK/SNARE/HIHAT/CLAP/BASS/COMP) ──
    const int tabW = 72, tabH = 22;   // 7 タブに対応するため若干縮小
    const int tabsTotal = tabW * 7;
    int tx = innerX + (innerW - tabsTotal) / 2;
    tabsRect = juce::Rectangle<int> (tx, y, tabsTotal, tabH);
    tabMain .setBounds (tx,             y, tabW, tabH); tx += tabW;
    tabKick .setBounds (tx,             y, tabW, tabH); tx += tabW;
    tabSnare.setBounds (tx,             y, tabW, tabH); tx += tabW;
    tabHiHat.setBounds (tx,             y, tabW, tabH); tx += tabW;
    tabClap .setBounds (tx,             y, tabW, tabH); tx += tabW;
    tabBass .setBounds (tx,             y, tabW, tabH); tx += tabW;
    tabComp .setBounds (tx,             y, tabW, tabH);
    y += tabH + 14;  // MIX/FXセクションラベル分のスペース

    // ── Synth Params (page-specific) — 固定高 ──
    const int synthRowH = juce::jmin (96, panelRect.getBottom() - y - 8);
    synthParamsRect = juce::Rectangle<int> (innerX, y, innerW, synthRowH);

    switch (currentPage)
    {
        case Page::Main:
            layoutMainPage (synthParamsRect);
            break;
        case Page::Kick:
            layoutDetailPage (synthParamsRect,
                              { &kickPitchK, &kickAmtK, &kickDecK, &kickDriveK, &kickPanK, &kickVolK });
            break;
        case Page::Snare:
            layoutDetailPage (synthParamsRect,
                              { &snareNoiseK, &snareToneK, &snareDecK, &snareSnapK, &snareCrispK, &snarePanK, &snareVolK });
            break;
        case Page::HiHat:
            // FREQ/Q/CLOSED/OPEN/METAL/DRIVE/PAN/VOL = 8 knobs
            layoutDetailPage (synthParamsRect,
                              { &hhFreqK, &hhQK, &hhDecK, &hhOpenDecK, &hhMetalK, &hhDriveK, &hhPanK, &hhVolK });
            break;
        case Page::Clap:
            // FREQ/Q/DECAY/SPREAD/STYLE/PAN/VOL = 7 knobs
            layoutDetailPage (synthParamsRect,
                              { &clapFreqK, &clapQK, &clapDecK, &clapSpreadK, &clapStyleK, &clapPanK, &clapVolK });
            break;
        case Page::Bass:
        {
            // Bassページ: WAVE + 14 knobs (+ CHORD)
            const int knobW = 55;       // 14 knobs に対応するため更に縮小
            const int totalW = 52 + knobW * 14;
            int px = synthParamsRect.getX() + (synthParamsRect.getWidth() - totalW) / 2;
            const int yy = synthParamsRect.getY() + 4;
            const int hh = synthParamsRect.getHeight() - 8;

            waveLabel.setBounds (px,     yy + 2,  52, 12);
            waveBtn  .setBounds (px + 4, yy + 18, 44, 26);
            px += 52;
            layoutKnobRow ({ &cutoffK, &resoK, &envModK, &bassDecK, &accentK,
                             &bassGlideK, &bassDriveK, &bassSubK,
                             &bassLfoRateK, &bassLfoDepthK, &bassLfoTargetK,
                             &bassChordK,
                             &bassPanK, &bassVolK },
                           px, yy, knobW, hh);
            break;
        }
        case Page::Comp:
            layoutDetailPage (synthParamsRect,
                              { &compThreshK, &compRatioK, &compAtkK, &compRelK });
            break;
    }

    // ─── スケール変換を全ての子コンポーネントへ適用 ───
    // ベース BaseWidth × effH でレイアウトしてから、実サイズに合わせて
    // 拡大/縮小 + センタリング。
    // 幅と高さのうち小さい比率を使うことで、画面より大きくはみ出さない
    // (iPad の縦横比が editor と違う場合、letterbox 風に余白が出る)。
    const int effH = getEffectiveBaseHeight();
    const float scaleX = (float) getWidth()  / (float) BaseWidth;
    const float scaleY = (float) getHeight() / (float) effH;
    const float scale  = juce::jmin (scaleX, scaleY);

    const float scaledW = BaseWidth * scale;
    const float scaledH = (float) effH * scale;
    const float offX    = ((float) getWidth()  - scaledW) * 0.5f;
    const float offY    = ((float) getHeight() - scaledH) * 0.5f;

    const bool needsTransform = std::abs (scale - 1.0f) > 0.0005f
                              || offX > 0.5f || offY > 0.5f;

    if (needsTransform)
    {
        const auto t = juce::AffineTransform::scale (scale).translated (offX, offY);
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (auto* c = getChildComponent (i))
                c->setTransform (t);
    }
    else
    {
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (auto* c = getChildComponent (i))
                c->setTransform ({});
    }
}

// ─── アクション ───
void PacificSynthesisEditor::exportMidi()
{
    auto mf = proc.buildMidiFile();
    const auto userMusic = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    chooser = std::make_unique<juce::FileChooser> (
        "Export pattern as MIDI", userMusic.getChildFile ("Pacific_pattern.mid"), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                          | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
                          [mf] (const juce::FileChooser& fc) mutable {
        const auto f = fc.getResult();
        if (f == juce::File()) return;
        juce::FileOutputStream stream (f);
        if (stream.openedOk()) {
            stream.setPosition (0);
            stream.truncate();
            mf.writeTo (stream);
        }
    });
}

void PacificSynthesisEditor::refreshPresetButtonLabel()
{
    const auto name = presetManager.getCurrentName();
    presetBtn.setButtonText (name.isEmpty() ? juce::String ("PRESET") : name);
}

void PacificSynthesisEditor::resetToDefaultPreset()
{
    // editPattern 経由で pattern clear を undo 登録
    editPattern ("Init Patch", [this]
    {
        // 全パラメータをデフォルト値に
        for (auto* p : proc.getParameters())
        {
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                r->setValueNotifyingHost (r->getDefaultValue());
        }
        proc.pattern.clear();
    });
    presetManager.clearCurrentName();
    refreshPresetButtonLabel();
}

void PacificSynthesisEditor::showPresetMenu()
{
    juce::PopupMenu m;

    m.addItem ("Init Patch (Defaults)", [this] { resetToDefaultPreset(); });
    m.addSeparator();

    // ─ Factory Presets (category 別サブメニュー) ─
    {
        // category → presets の対応表を作る
        const auto allPresets = FactoryPresets::all();
        std::map<juce::String, std::vector<FactoryPresets::Preset>> byCategory;
        for (const auto& fp : allPresets)
            byCategory[fp.category].push_back (fp);

        juce::PopupMenu fac;
        // 表示順は categoryOrder() で固定。未登録カテゴリは末尾に。
        for (const auto& cat : FactoryPresets::categoryOrder())
        {
            auto it = byCategory.find (cat);
            if (it == byCategory.end() || it->second.empty()) continue;

            juce::PopupMenu sub;
            for (const auto& fp : it->second)
            {
                sub.addItem (fp.name, true, false, [this, fp]
                {
                    editPattern (juce::String ("Load ") + fp.name, [this, fp]
                    {
                        fp.apply (proc, proc.apvts, proc.pattern);
                    });
                    presetManager.setCurrentName (fp.name);
                    refreshPresetButtonLabel();
                });
            }
            // category 内件数を表示してわかりやすく
            const juce::String label = cat + "  (" + juce::String ((int) it->second.size()) + ")";
            fac.addSubMenu (label, sub);
            byCategory.erase (it);
        }
        // categoryOrder() に載ってない category があれば末尾に並べる
        for (const auto& [cat, list] : byCategory)
        {
            juce::PopupMenu sub;
            for (const auto& fp : list)
            {
                sub.addItem (fp.name, true, false, [this, fp]
                {
                    editPattern (juce::String ("Load ") + fp.name, [this, fp]
                    {
                        fp.apply (proc, proc.apvts, proc.pattern);
                    });
                    presetManager.setCurrentName (fp.name);
                    refreshPresetButtonLabel();
                });
            }
            const juce::String label = cat + "  (" + juce::String ((int) list.size()) + ")";
            fac.addSubMenu (label, sub);
        }
        m.addSubMenu ("Factory Presets", fac);
    }
    m.addSeparator();

    const auto presets = presetManager.listPresets();
    const auto current = presetManager.getCurrentName();
    if (presets.isEmpty())
    {
        m.addItem ("(No presets yet)", false, false, [] {});
    }
    else
    {
        for (const auto& name : presets)
        {
            const bool isCurrent = (name == current);
            // 引数: text, isEnabled, isTicked, action
            m.addItem (name, true, isCurrent, [this, name] {
                // editPattern 経由で undo 登録
                editPattern (juce::String ("Load ") + name, [this, name]
                {
                    presetManager.loadPreset (name);
                });
                refreshPresetButtonLabel();
            });
        }
    }

    m.addSeparator();
    m.addItem ("Save Preset As...", [this] { promptSavePresetAs(); });

    if (current.isNotEmpty())
    {
        m.addItem ("Save (overwrite \"" + current + "\")", [this, current] {
            if (presetManager.savePreset (current))
                refreshPresetButtonLabel();
        });
        m.addItem ("Delete \"" + current + "\"", [this, current] {
            if (presetManager.deletePreset (current))
                refreshPresetButtonLabel();
        });
    }

    // ─ Export WAV (2MIX / Stems) ─
    m.addSeparator();
    {
        juce::PopupMenu wavRoot;

        auto addBarsSubmenu = [this] (WavExporter::Type t) -> juce::PopupMenu
        {
            juce::PopupMenu sub;
            for (int b : { 1, 2, 4, 8 })
            {
                const juce::String label = juce::String (b) + (b == 1 ? " bar" : " bars");
                sub.addItem (label, [this, t, b] { exportWavInteractive (t, b); });
            }
            return sub;
        };

        wavRoot.addSubMenu ("2MIX",            addBarsSubmenu (WavExporter::Type::Mix));
        wavRoot.addSubMenu ("Stems (5 files)", addBarsSubmenu (WavExporter::Type::Stems));

        m.addSubMenu ("Export WAV", wavRoot);
    }

    m.addSeparator();
    m.addItem ("Reveal Presets Folder...", [this] {
        presetManager.revealFolder();
    });

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn));
}

// ── WAV エクスポート ──
// 1) フォルダ選択ダイアログ → 2) 進捗ウィンドウ付きバックグラウンドレンダリング → 3) 完了通知
void PacificSynthesisEditor::exportWavInteractive (WavExporter::Type type, int bars)
{
    const auto initialDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    chooser = std::make_unique<juce::FileChooser> (
        type == WavExporter::Type::Mix
            ? juce::String ("Choose output folder for 2MIX WAV")
            : juce::String ("Choose output folder for Stem WAVs"),
        initialDir);

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectDirectories;

    chooser->launchAsync (flags, [this, type, bars] (const juce::FileChooser& fc)
    {
        const auto folder = fc.getResult();
        if (! folder.isDirectory()) return;

        runWavExport (folder, type, bars);
    });
}

void PacificSynthesisEditor::runWavExport (juce::File outputFolder,
                                            WavExporter::Type type, int bars)
{
    // 1 bar (約 1〜2 秒) なので同期実行。
    // (進捗バー UI は JUCE 8 の API 変更で削除、必要なら今後追加)
    WavExporter::Options opts;
    opts.bars = bars;

    auto baseName = presetManager.getCurrentName();
    if (baseName.isEmpty()) baseName = "Pacific";

    // マウスポインタを wait カーソルに (短時間 UI 凍結のヒント)
    juce::MouseCursor::showWaitCursor();
    const auto written = WavExporter::exportPattern (proc, outputFolder, baseName,
                                                       type, opts);
    juce::MouseCursor::hideWaitCursor();

    // 結果通知
    const int n = written.size();
    if (n > 0)
    {
        juce::String body;
        body << "Saved " << n << " file(s) to:\n" << outputFolder.getFullPathName();
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "Export complete",
            body, "OK");
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Export failed",
            "Could not write WAV files to:\n" + outputFolder.getFullPathName(),
            "OK");
    }
}

void PacificSynthesisEditor::promptSavePresetAs()
{
    auto* aw = new juce::AlertWindow ("Save Preset",
                                       "Enter a name for this preset:",
                                       juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", presetManager.getCurrentName());
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, aw] (int result)
        {
            if (result == 1)
            {
                const auto name = aw->getTextEditorContents ("name").trim();
                // 不正な文字を除去 (ファイル名で使えない文字)
                auto safe = name.removeCharacters ("/\\:*?\"<>|");
                if (safe.isNotEmpty())
                {
                    if (presetManager.savePreset (safe))
                        refreshPresetButtonLabel();
                }
            }
        }), true);  // true = JUCE が AlertWindow を自動削除
}
