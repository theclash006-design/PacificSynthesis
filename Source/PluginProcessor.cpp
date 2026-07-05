#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>   // std::isfinite

PacificSynthesisProcessor::PacificSynthesisProcessor()
  : AudioProcessor (BusesProperties()
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
    // patternUndoManager をパラメータ変更にも紐付け → ノブ操作も Undo 可能に
    apvts (*this, &patternUndoManager, "Params", PacificParams::createLayout())
{
    sequencer.setOnStep ([this] (int s) { triggerPatternStep (s); });
}

void PacificSynthesisProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    bass.prepare (sampleRate, samplesPerBlock);
    kick.prepare (sampleRate);
    snare.prepare (sampleRate);
    hihat.prepare (sampleRate);
    clap.prepare (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());

    cacheParameterPointers();
    pullParameters();

    // ─ ホストにレイテンシ報告 (Oversampling 由来、常時固定値)
    setLatencySamples (fx.getLatencySamples());

    // ─ サイドチェイン envelope を初期化
    scEnv = 0.0f;
}

void PacificSynthesisProcessor::cacheParameterPointers()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    params.bassWave   = get (PacificParams::BassWave);
    params.cutoff     = get (PacificParams::Cutoff);
    params.reso       = get (PacificParams::Reso);
    params.envMod     = get (PacificParams::EnvMod);
    params.bassDec    = get (PacificParams::BassDec);
    params.accAmt     = get (PacificParams::AccAmt);
    params.delMix     = get (PacificParams::DelMix);
    params.revMix     = get (PacificParams::RevMix);
    params.revMode    = get (PacificParams::RevMode);
    params.dist       = get (PacificParams::Dist);
    params.masterGain = get (PacificParams::MasterGain);
    params.bpm        = get (PacificParams::Bpm);
    params.swing      = get (PacificParams::Swing);
    params.kickDec    = get (PacificParams::KickDec);
    params.snareDec   = get (PacificParams::SnareDec);
    params.hhDec      = get (PacificParams::HhDec);
    params.kickPitch  = get (PacificParams::KickPitch);
    params.kickAmt    = get (PacificParams::KickAmt);
    params.kickDrive  = get (PacificParams::KickDrive);
    params.snareNoise = get (PacificParams::SnareNoise);
    params.snareTone  = get (PacificParams::SnareTone);
    params.snareSnap  = get (PacificParams::SnareSnap);
    params.snareCrisp = get (PacificParams::SnareCrisp);
    params.hhFreq     = get (PacificParams::HhFreq);
    params.hhQ        = get (PacificParams::HhQ);
    params.hhMetal    = get (PacificParams::HhMetal);
    params.hhDrive    = get (PacificParams::HhDrive);
    params.clapFreq   = get (PacificParams::ClapFreq);
    params.clapQ      = get (PacificParams::ClapQ);
    params.clapDec    = get (PacificParams::ClapDec);
    params.clapSpread = get (PacificParams::ClapSpread);
    params.clapStyle  = get (PacificParams::ClapStyle);
    params.bassGlide  = get (PacificParams::BassGlide);
    params.bassDrive  = get (PacificParams::BassDrive);
    params.bassSub    = get (PacificParams::BassSub);
    params.kickVol    = get (PacificParams::KickVol);
    params.snareVol   = get (PacificParams::SnareVol);
    params.hhVol      = get (PacificParams::HhVol);
    params.clapVol    = get (PacificParams::ClapVol);
    params.bassVol    = get (PacificParams::BassVol);
    params.kickPan    = get (PacificParams::KickPan);
    params.snarePan   = get (PacificParams::SnarePan);
    params.hhPan      = get (PacificParams::HhPan);
    params.clapPan    = get (PacificParams::ClapPan);
    params.bassPan    = get (PacificParams::BassPan);
    params.scAmt      = get (PacificParams::ScAmt);
    params.scRel      = get (PacificParams::ScRel);
    params.hhOpenDec  = get (PacificParams::HhOpenDec);
    params.compThresh = get (PacificParams::CompThresh);
    params.compRatio  = get (PacificParams::CompRatio);
    params.compAtk    = get (PacificParams::CompAtk);
    params.compRel    = get (PacificParams::CompRel);
    params.bassLfoRate   = get (PacificParams::BassLfoRate);
    params.bassLfoDepth  = get (PacificParams::BassLfoDepth);
    params.bassLfoTarget = get (PacificParams::BassLfoTarget);
    params.bassChord     = get (PacificParams::BassChord);
}

bool PacificSynthesisProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PacificSynthesisProcessor::pullParameters()
{
    // キャッシュ済みポインタを使用 (RT-safe、文字列ルックアップ無し)
    bass.setWaveform     ((int) params.bassWave->load());
    bass.setCutoff       (params.cutoff->load());
    bass.setResonance    (params.reso->load());
    bass.setEnvMod       (params.envMod->load());
    bass.setDecay        (params.bassDec->load());
    bass.setAccentAmount (params.accAmt->load());

    kick.setDecay  (params.kickDec->load());
    snare.setDecay (params.snareDec->load());
    hihat.setDecay (params.hhDec->load());

    // Kick
    kick.setPitch    (params.kickPitch->load());
    kick.setPitchAmt (params.kickAmt->load());
    kick.setDrive    (params.kickDrive->load());
    // Snare
    snare.setNoiseMix (params.snareNoise->load());
    snare.setTone     (params.snareTone->load());
    snare.setSnap     (params.snareSnap->load());
    snare.setCrisp    (params.snareCrisp->load());
    // HiHat
    hihat.setFreq  (params.hhFreq->load());
    hihat.setQ     (params.hhQ->load());
    hihat.setMetal (params.hhMetal->load());
    hihat.setDrive (params.hhDrive->load());
    // Clap
    clap.setFreq   (params.clapFreq->load());
    clap.setQ      (params.clapQ->load());
    clap.setDecay  (params.clapDec->load());
    clap.setSpread (params.clapSpread->load());
    clap.setStyle  (params.clapStyle->load());
    // Bass extras
    bass.setGlideTime (params.bassGlide->load());
    bass.setDrive     (params.bassDrive->load());
    bass.setSubLevel  (params.bassSub->load());
    // Bass LFO
    bass.setLfoRate   (params.bassLfoRate->load());
    bass.setLfoDepth  (params.bassLfoDepth->load());
    bass.setLfoTarget ((int) params.bassLfoTarget->load());

    // 個別ボリューム
    kickVol  = params.kickVol->load();
    snareVol = params.snareVol->load();
    hhVol    = params.hhVol->load();
    clapVol  = params.clapVol->load();
    bassVol  = params.bassVol->load();

    // パン (constant-power 係数を事前計算)
    kickPanG  = calcPan (params.kickPan->load());
    snarePanG = calcPan (params.snarePan->load());
    hhPanG    = calcPan (params.hhPan->load());
    clapPanG  = calcPan (params.clapPan->load());
    bassPanG  = calcPan (params.bassPan->load());

    // サイドチェイン
    scAmt = juce::jlimit (0.0f, 1.0f, params.scAmt->load());
    const float rel = juce::jmax (0.001f, params.scRel->load());
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    scReleaseCoeff = std::exp (-1.0f / (rel * (float) sr));

    // HiHat open decay
    hihat.setOpenDecay (params.hhOpenDec->load());

    fx.setDelayMix   (params.delMix->load());
    fx.setReverbMix  (params.revMix->load());
    fx.setReverbMode ((int) params.revMode->load());
    fx.setDistortion (params.dist->load());
    fx.setCompThreshold (params.compThresh->load());
    fx.setCompRatio     (params.compRatio->load());
    fx.setCompAttack    (params.compAtk->load());
    fx.setCompRelease   (params.compRel->load());
    fx.setMasterGain (params.masterGain->load());
}

void PacificSynthesisProcessor::triggerPatternStep (int s)
{
    const bool dMute = drumsMuted.load();
    const bool bMute = bassMuted .load();
    // SRC トグル: voice 個別の EXT モード状態
    // 各 voice の発音ソース (0=SEQ, 1=MIDI, 2=OFF)
    const int kickSrc  = voiceSrcMode[kKickVoice ].load();
    const int snareSrc = voiceSrcMode[kSnareVoice].load();
    const int hihatSrc = voiceSrcMode[kHiHatVoice].load();
    const int clapSrc  = voiceSrcMode[kClapVoice ].load();
    const int bassSrc  = voiceSrcMode[kBassVoice ].load();
    // 内蔵パターンが鳴るのは SEQ モード (= 0) の時のみ
    const bool kickFromSeq  = (kickSrc  == kSrcSeq);
    const bool snareFromSeq = (snareSrc == kSrcSeq);
    const bool hihatFromSeq = (hihatSrc == kSrcSeq);
    const bool clapFromSeq  = (clapSrc  == kSrcSeq);
    const bool bassFromSeq  = (bassSrc  == kSrcSeq);

    // ─ ドラム (voice ごとに EXT モード判定)
    if (! dMute)
    {
        for (int row = 0; row < PatternState::NumDrumRows; ++row)
        {
            const auto cell = pattern.getDrum (row, s);
            if (! cell.on) continue;
            switch (row) {
                case 0: if (kickFromSeq)  { kick.trigger  (cell.velocity); triggerSidechain(); } break;
                case 1: if (snareFromSeq) snare.trigger (cell.velocity); break;
                case 2: if (hihatFromSeq) hihat.trigger (cell.velocity); break;
                case 3: if (clapFromSeq)  clap.trigger  (cell.velocity); break;
                default: break;
            }
        }
    }
    // ─ ベース (SEQ モード時のみ内蔵パターン再生)
    if (! bMute && bassFromSeq)
    {
        const auto bc = pattern.getBass (s);
        if (bc.on)
        {
            const int midi = PatternState::toMidiNote (bc.note, bc.octave);
            const bool slide = (activeBassPatternNote >= 0) || bc.slide;
            const float vel  = bc.accent ? 1.0f : 0.78f;
            triggerBassWithChord (midi, vel, bc.accent, slide);
            activeBassPatternNote = midi;
        }
        else
        {
            activeBassPatternNote = -1;
        }
    }
    else
    {
        // ミュート/外部 MIDI hold 中は持続音を切っておく
        if (activeBassPatternNote >= 0)
        {
            bass.noteOff (activeBassPatternNote);
            activeBassPatternNote = -1;
        }
    }
}

// ─── Bass Chord: root + ハーモニー intervals でまとめてトリガー ───
void PacificSynthesisProcessor::triggerBassWithChord (int rootMidi, float velocity,
                                                       bool accent, bool slideRoot)
{
    // Root を最初にトリガー (slide が有効なら voice 0 を mono 的に使う)
    bass.noteOn (rootMidi, velocity, accent, slideRoot);

    const int chordMode = (int) params.bassChord->load();
    if (chordMode <= 0) return;

    // 半音単位のインターバル (root から上に積む)
    //   1=+Oct(12)、2=Power(7)、3=Maj(4,7)、4=Min(3,7)、5=Sus4(5,7)
    static const std::array<std::vector<int>, 6> intervalsByMode = {{
        {},          // 0 Off
        {12},        // 1 +Oct
        {7},         // 2 Power
        {4, 7},      // 3 Maj
        {3, 7},      // 4 Min
        {5, 7},      // 5 Sus4
    }};

    if (chordMode >= 1 && chordMode < (int) intervalsByMode.size())
    {
        // ハーモニー音は root より少し控えめ (混濁回避)、slide=false で別ボイスへ
        const float harmonyVel = velocity * 0.70f;
        for (int interval : intervalsByMode[(size_t) chordMode])
        {
            const int harmonyMidi = rootMidi + interval;
            if (harmonyMidi >= 0 && harmonyMidi <= 127)
                bass.noteOn (harmonyMidi, harmonyVel, false, false);
        }
    }
}

// ─── MIDI CC マッピング ───
void PacificSynthesisProcessor::startMidiLearn (const juce::String& paramID) noexcept
{
    learnTargetParam = paramID;
}

void PacificSynthesisProcessor::cancelMidiLearn () noexcept
{
    learnTargetParam.clear();
}

void PacificSynthesisProcessor::clearMidiMapping (const juce::String& paramID) noexcept
{
    for (auto& s : ccToParam)
        if (s == paramID) s.clear();
}

int PacificSynthesisProcessor::getCCForParam (const juce::String& paramID) const noexcept
{
    for (int i = 0; i < (int) ccToParam.size(); ++i)
        if (ccToParam[(size_t) i] == paramID) return i;
    return -1;
}

void PacificSynthesisProcessor::handleMidiEvent (const juce::MidiMessage& m)
{
    // ─ MIDI CC: Learn 中なら割り当て、そうでなければマップ済み param を更新
    if (m.isController())
    {
        const int cc  = m.getControllerNumber();   // 0–127
        const int val = m.getControllerValue();    // 0–127

        if (cc >= 0 && cc < 128)
        {
            // Learn 中: 現在の Learn target にこの CC を割り当て
            if (learnTargetParam.isNotEmpty())
            {
                // 既に同じ param が他の CC に割り当てられていたらクリア (1 param 1 CC)
                for (auto& s : ccToParam)
                    if (s == learnTargetParam) s.clear();
                ccToParam[(size_t) cc] = learnTargetParam;
                learnTargetParam.clear();
                return;  // この CC は param 更新には使わない
            }

            // 既にマップされた CC → param 更新
            const auto& pid = ccToParam[(size_t) cc];
            if (pid.isNotEmpty())
            {
                if (auto* p = apvts.getParameter (pid))
                {
                    const float norm = (float) val / 127.0f;
                    p->setValueNotifyingHost (norm);
                }
            }
        }
        return;  // CC は note 処理にフォールスルーしない
    }

    const bool dMute = drumsMuted.load();
    const bool bMute = bassMuted .load();

    if (m.isNoteOn())
    {
        const int  note    = m.getNoteNumber();
        const int  channel = m.getChannel();          // 1-16
        const float vel    = m.getFloatVelocity();
        const bool accent  = m.getVelocity() >= 100;

        // チャンネル別ルーティング:
        //   ・channel 10           → 高領域 + GM 低領域どちらもドラム
        //   ・channel != 10        → 高領域 (84+) のみドラム、その他はベース
        const bool isDrum = PacificParams::isDrumNoteForChannel (note, channel);

        if (isDrum)
        {
            if (dMute) return;
            // 高領域 (C6+) と GM 互換低領域 (C1+) の両方を同じドラムに振り分け
            // ★ MIDI モードの voice のみトリガー (SEQ モードは DAW MIDI を無視)
            // MIDI 入力は voice mode == MIDI の時のみ受け付け (OFF / SEQ は無視)
            auto isMidiMode = [this] (int idx) { return voiceSrcMode[(size_t) idx].load() == kSrcMidi; };
            if (note == PacificParams::KickNote  || note == PacificParams::KickNoteGM)
            {
                if (isMidiMode (kKickVoice)) { kick.trigger (vel); triggerSidechain(); }
            }
            else if (note == PacificParams::SnareNote || note == PacificParams::SnareNoteGM)
            {
                if (isMidiMode (kSnareVoice)) snare.trigger (vel);
            }
            else if (note == PacificParams::HiHatNote || note == PacificParams::HiHatNoteGM)
            {
                if (isMidiMode (kHiHatVoice)) hihat.trigger (vel);
            }
            else if (note == PacificParams::OpenHiHatNote || note == PacificParams::OpenHiHatNoteGM)
            {
                if (isMidiMode (kHiHatVoice)) hihat.triggerOpen (vel);
            }
            else if (note == PacificParams::ClapNote  || note == PacificParams::ClapNoteGM)
            {
                if (isMidiMode (kClapVoice)) clap.trigger (vel);
            }
        }
        else
        {
            if (bMute) return;
            // Bass MIDI モード時のみ受け付け (SEQ/OFF は無視)
            if (voiceSrcMode[kBassVoice].load() != kSrcMidi) return;

            // MIDI モードに切替わったので、進行中のパターン由来 bass note を切る (重音回避)
            if (activeBassPatternNote >= 0)
            {
                bass.noteOff (activeBassPatternNote);
                activeBassPatternNote = -1;
            }

            // MIDI 入力は常にポリフォニック (各 note を別 voice へ、slide=false)
            // → 和音演奏が可能。BassChord 設定が Off 以外なら各 note にハーモニーも乗る。
            triggerBassWithChord (note, vel, accent, false);
        }
    }
    else if (m.isNoteOff())
    {
        const int note    = m.getNoteNumber();
        const int channel = m.getChannel();
        // Bass MIDI モードのときだけ noteOff を受ける
        if (! PacificParams::isDrumNoteForChannel (note, channel)
            && voiceSrcMode[kBassVoice].load() == kSrcMidi)
        {
            bass.noteOff (note);
        }
    }
}

void PacificSynthesisProcessor::setInternalPlaying (bool playing) noexcept
{
    if (! playing) internalPpq = 0.0;
    internalPlaying.store (playing);
}

void PacificSynthesisProcessor::setVoiceSrc (int voiceIndex, int mode) noexcept
{
    if (voiceIndex < 0 || voiceIndex >= (int) voiceSrcMode.size()) return;
    mode = juce::jlimit (0, 2, mode);
    voiceSrcMode[(size_t) voiceIndex].store (mode);

    // Bass を SEQ から外した瞬間に進行中のパターン由来 bass note を切る (重音回避)
    if (voiceIndex == kBassVoice && mode != kSrcSeq && activeBassPatternNote >= 0)
    {
        bass.noteOff (activeBassPatternNote);
        activeBassPatternNote = -1;
    }
}

int PacificSynthesisProcessor::getVoiceSrc (int voiceIndex) const noexcept
{
    if (voiceIndex < 0 || voiceIndex >= (int) voiceSrcMode.size()) return kSrcSeq;
    return voiceSrcMode[(size_t) voiceIndex].load();
}

bool PacificSynthesisProcessor::isCurrentlyPlaying() const noexcept
{
    return hostWasPlaying || internalPlaying.load();
}

void PacificSynthesisProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int numSamples = buffer.getNumSamples();
    const double sr = getSampleRate();

    pullParameters();

    // ── ホスト情報 ──
    double hostBpm = 128.0;
    double hostPpqStart = 0.0;
    bool   hostPlaying = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())          hostBpm = *b;
            if (auto p = pos->getPpqPosition())  hostPpqStart = *p;
            hostPlaying = pos->getIsPlaying();
        }
    }
    hostWasPlaying = hostPlaying;

    // 内蔵BPM (キャッシュ済みポインタを使用)
    const double paramBpm = (double) params.bpm->load();

    // 採用するBPM: ホスト再生中はホストBPM、それ以外はパラメータBPM
    const double bpm = hostPlaying ? hostBpm : paramBpm;
    displayBpm.store (bpm);

    fx.setTempo (bpm);

    // 再生中フラグ: ホスト or 内蔵
    const bool intPlay = internalPlaying.load();
    const bool playing = hostPlaying || intPlay;
    sequencer.beginBlock (playing);

    // 16分音符位置の開始点
    double sixteenthStart;
    if (hostPlaying) {
        sixteenthStart = hostPpqStart * 4.0;
    } else {
        sixteenthStart = internalPpq * 4.0;
    }
    const double ppqPerSample = (bpm / 60.0) / sr;
    const double sixteenthPerSample = ppqPerSample * 4.0;

    // ─ 出力バッファ直書き (各ボイスを Pan して L/R 別に積算)
    const int outChannels = buffer.getNumChannels();
    auto* L = buffer.getWritePointer (0);
    auto* R = (outChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    juce::FloatVectorOperations::clear (L, numSamples);
    if (R != nullptr)
        juce::FloatVectorOperations::clear (R, numSamples);

    // パン係数 (pullParameters 済みのキャッシュをローカル変数へ)
    const auto kp = kickPanG;
    const auto sp = snarePanG;
    const auto hp = hhPanG;
    const auto cp = clapPanG;
    const auto bp = bassPanG;
    const float scA = scAmt;
    const float scR = scReleaseCoeff;

    auto it  = midi.cbegin();
    auto end = midi.cend();

    for (int i = 0; i < numSamples; ++i)
    {
        // MIDI イベント
        while (it != end && (*it).samplePosition <= i)
        {
            handleMidiEvent ((*it).getMessage());
            ++it;
        }

        // シーケンサー (パターン由来のトリガー)
        sequencer.processSample (sixteenthStart + sixteenthPerSample * (double) i);

        // 音源レンダ (個別 Vol を適用)
        const float k  = kick.render()  * kickVol;
        const float sn = snare.render() * snareVol;
        const float hh = hihat.render() * hhVol;
        const float cl = clap.render()  * clapVol;
        float bs = 0.0f;
        bass.render (&bs, 1);
        bs *= bassVol;

        // ─ サイドチェイン: scEnv を release 係数で減衰、Bass を duck
        scEnv *= scR;
        // (1 - scAmt * env)。scAmt=0 なら完全パススルー
        bs *= (1.0f - scA * scEnv);

        // ─ Pan して L/R 積算 (constant-power)
        float ll = k * kp.gL + sn * sp.gL + hh * hp.gL + cl * cp.gL + bs * bp.gL;
        float rr = k * kp.gR + sn * sp.gR + hh * hp.gR + cl * cp.gR + bs * bp.gR;

        // NaN/Inf ガード: 不正値が出たらサイレントに置換
        if (! std::isfinite (ll)) ll = 0.0f;
        if (! std::isfinite (rr)) rr = 0.0f;
        // ハードクリップ防止 (FXチェーン前の余裕分)
        ll = juce::jlimit (-4.0f, 4.0f, ll);
        rr = juce::jlimit (-4.0f, 4.0f, rr);

        if (R != nullptr) { L[i] = ll; R[i] = rr; }
        else              { L[i] = (ll + rr) * 0.5f;            }  // mono 出力時は L+R サム
    }
    while (it != end) { handleMidiEvent ((*it).getMessage()); ++it; }

    // 内蔵play時はインターナルPPQを進める
    if (intPlay && ! hostPlaying)
        internalPpq += ppqPerSample * (double) numSamples;

    // ─ NaN/Inf ガード後の最終リーク防止 (mono 出力時は L のみ使用)
    // monoBus は廃止 (ステレオ直書きに変更したため未使用)

    fx.process (buffer);
}

juce::AudioProcessorEditor* PacificSynthesisProcessor::createEditor()
{
    return new PacificSynthesisEditor (*this);
}

// ── 状態シリアライズ ──
void PacificSynthesisProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("PacificState");
    if (auto s = apvts.copyState(); s.isValid())
        root.appendChild (s.createCopy(), nullptr);
    root.appendChild (pattern.toValueTree(), nullptr);
    // SRC トグル (voice 個別 INT/EXT) 状態の保存
    // 新形式: int で保存 (0=SEQ, 1=MIDI, 2=OFF)
    // 旧形式 bool との互換維持のため "Src" suffix で別キーに保存
    root.setProperty ("kickSrc",  voiceSrcMode[kKickVoice ].load(), nullptr);
    root.setProperty ("snareSrc", voiceSrcMode[kSnareVoice].load(), nullptr);
    root.setProperty ("hihatSrc", voiceSrcMode[kHiHatVoice].load(), nullptr);
    root.setProperty ("clapSrc",  voiceSrcMode[kClapVoice ].load(), nullptr);
    root.setProperty ("bassSrc",  voiceSrcMode[kBassVoice ].load(), nullptr);
    // VIEW モード (BOTH/DRUM/BASS) も保存
    root.setProperty ("viewMode", savedViewMode.load(), nullptr);

    // ─ MIDI CC マッピング保存 (空文字以外の項目のみ)
    juce::ValueTree ccMap ("MidiCCMap");
    for (int i = 0; i < (int) ccToParam.size(); ++i)
    {
        if (ccToParam[(size_t) i].isNotEmpty())
        {
            juce::ValueTree e ("Map");
            e.setProperty ("cc",    i,                       nullptr);
            e.setProperty ("param", ccToParam[(size_t) i],   nullptr);
            ccMap.appendChild (e, nullptr);
        }
    }
    root.appendChild (ccMap, nullptr);

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void PacificSynthesisProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto root = juce::ValueTree::fromXml (*xml);
        if (root.hasType ("PacificState"))
        {
            // パラメータ
            for (auto child : root)
            {
                if (child.hasType (apvts.state.getType()))
                    apvts.replaceState (child);
                else if (child.hasType ("Pattern"))
                    pattern.fromValueTree (child);
            }
            // SRC 状態を復元
            //   1. 新形式 "...Src" (int 0=SEQ/1=MIDI/2=OFF)
            //   2. 旧形式 "...External" (bool: false→SEQ, true→MIDI)
            auto loadVoiceSrc = [&] (const char* newKey, const char* oldKey, int idx)
            {
                if (root.hasProperty (newKey))
                {
                    const int v = juce::jlimit (0, 2, (int) root.getProperty (newKey));
                    voiceSrcMode[(size_t) idx].store (v);
                }
                else if (root.hasProperty (oldKey))
                {
                    const bool ext = (bool) root.getProperty (oldKey);
                    voiceSrcMode[(size_t) idx].store (ext ? kSrcMidi : kSrcSeq);
                }
            };
            loadVoiceSrc ("kickSrc",  "kickExternal",  kKickVoice );
            loadVoiceSrc ("snareSrc", "snareExternal", kSnareVoice);
            loadVoiceSrc ("hihatSrc", "hihatExternal", kHiHatVoice);
            loadVoiceSrc ("clapSrc",  "clapExternal",  kClapVoice );
            loadVoiceSrc ("bassSrc",  "bassExternal",  kBassVoice );
            // VIEW モードを復元
            if (root.hasProperty ("viewMode"))
                savedViewMode.store ((int) root.getProperty ("viewMode"));

            // MIDI CC マッピング復元
            for (auto& s : ccToParam) s.clear();
            auto ccMapNode = root.getChildWithName ("MidiCCMap");
            if (ccMapNode.isValid())
            {
                for (auto e : ccMapNode)
                {
                    if (e.hasType ("Map"))
                    {
                        const int cc = (int) e.getProperty ("cc", -1);
                        const juce::String pid = e.getProperty ("param", juce::String()).toString();
                        if (cc >= 0 && cc < 128 && pid.isNotEmpty())
                            ccToParam[(size_t) cc] = pid;
                    }
                }
            }
            // 旧形式 (drumExternal: 全ドラム共通) からのマイグレーション
            if (root.hasProperty ("drumExternal"))
            {
                const bool drumExt = (bool) root.getProperty ("drumExternal");
                const int v = drumExt ? kSrcMidi : kSrcSeq;
                voiceSrcMode[kKickVoice ].store (v);
                voiceSrcMode[kSnareVoice].store (v);
                voiceSrcMode[kHiHatVoice].store (v);
                voiceSrcMode[kClapVoice ].store (v);
            }
        }
        else if (root.hasType (apvts.state.getType()))
        {
            // 旧フォーマット: パラメータのみ
            apvts.replaceState (root);
        }
    }

    // ホスト/プロジェクトからの state 復元時に Undo 履歴を残さない
    // (プロジェクト開いた直後に ⌘Z で全パラメータが「前回未保存状態」へ吹き飛ぶ事故防止)
    patternUndoManager.clearUndoHistory();
}

// ── パターン → MIDIファイル ──
namespace
{
    constexpr int kTicksPerQuarter = 480;
    constexpr int kStepTicks       = kTicksPerQuarter / 4;  // 16分音符 = 120 tick

    juce::MidiMessageSequence makeTempoTrack (double bpm, const char* name)
    {
        juce::MidiMessageSequence t;
        // トラック名 (= シーケンス名) メタイベント
        t.addEvent (juce::MidiMessage::textMetaEvent (3, name), 0);
        t.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)), 0);
        return t;
    }

    juce::MidiMessageSequence makeDrumSequence (const PatternState& pattern)
    {
        juce::MidiMessageSequence track;
        // トラック名 → DAW が "Drums" トラックとして扱える
        track.addEvent (juce::MidiMessage::textMetaEvent (3, "PACIFIC Drums"), 0);

        static const int drumNotes[] = {
            PacificParams::KickNote, PacificParams::SnareNote,
            PacificParams::HiHatNote, PacificParams::ClapNote
        };
        for (int s = 0; s < PatternState::NumSteps; ++s)
        {
            for (int r = 0; r < PatternState::NumDrumRows; ++r)
            {
                const auto c = pattern.getDrum (r, s);
                if (! c.on) continue;
                const int tick = s * kStepTicks;
                const int vel  = juce::jlimit (1, 127, (int) (c.velocity * 127.0f));
                track.addEvent (juce::MidiMessage::noteOn  (10, drumNotes[r], (juce::uint8) vel), (double) tick);
                track.addEvent (juce::MidiMessage::noteOff (10, drumNotes[r]), (double) (tick + kStepTicks / 2));
            }
        }
        return track;
    }

    juce::MidiMessageSequence makeBassSequence (const PatternState& pattern)
    {
        juce::MidiMessageSequence track;
        track.addEvent (juce::MidiMessage::textMetaEvent (3, "PACIFIC Bass"), 0);

        for (int s = 0; s < PatternState::NumSteps; ++s)
        {
            const auto b = pattern.getBass (s);
            if (! b.on) continue;
            const int midi = PatternState::toMidiNote (b.note, b.octave);
            const int tick = s * kStepTicks;
            const int vel  = b.accent ? 120 : 90;
            const int dur  = b.slide ? (int) (kStepTicks * 1.1) : (int) (kStepTicks * 0.75);
            track.addEvent (juce::MidiMessage::noteOn  (1, midi, (juce::uint8) vel), (double) tick);
            track.addEvent (juce::MidiMessage::noteOff (1, midi), (double) (tick + dur));
        }
        return track;
    }
}

juce::MidiFile PacificSynthesisProcessor::buildMidiFile() const
{
    // buildMidiFile はオーディオスレッド外 (UIから) しか呼ばれないので、
    // キャッシュ未初期化 (= prepareToPlay 前) でも安全な経路に
    const double paramBpm = params.bpm != nullptr
        ? (double) params.bpm->load()
        : (double) apvts.getRawParameterValue (PacificParams::Bpm)->load();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (kTicksPerQuarter);
    mf.addTrack (makeTempoTrack (paramBpm, "PACIFIC"));
    mf.addTrack (makeDrumSequence (pattern));
    mf.addTrack (makeBassSequence (pattern));
    return mf;
}

juce::MidiFile PacificSynthesisProcessor::buildDrumMidiFile() const
{
    const double paramBpm = params.bpm != nullptr
        ? (double) params.bpm->load()
        : (double) apvts.getRawParameterValue (PacificParams::Bpm)->load();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (kTicksPerQuarter);
    mf.addTrack (makeTempoTrack (paramBpm, "PACIFIC Drums"));
    mf.addTrack (makeDrumSequence (pattern));
    return mf;
}

juce::MidiFile PacificSynthesisProcessor::buildBassMidiFile() const
{
    const double paramBpm = params.bpm != nullptr
        ? (double) params.bpm->load()
        : (double) apvts.getRawParameterValue (PacificParams::Bpm)->load();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (kTicksPerQuarter);
    mf.addTrack (makeTempoTrack (paramBpm, "PACIFIC Bass"));
    mf.addTrack (makeBassSequence (pattern));
    return mf;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PacificSynthesisProcessor();
}
