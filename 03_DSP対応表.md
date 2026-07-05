# DSP 対応表 — 元 HTML (Tone.js) ⇆ C++ (JUCE)

各音源・FXがWeb版とどう対応しているかを表にまとめました。
**音は再現を目指したが、完全一致ではない**ので、必要に応じて係数を調整してください。

---

## 1. パラメータマッピング

| Web版 (Tone.js) | 範囲 | VST3パラメータ | 範囲 | 対応 |
|---|---|---|---|---|
| `bpm` | 60–200 | (ホスト同期) | – | DAW再生に同期。VST3パラメータ化はしていない |
| `swing` | 0–1 | – | – | ホスト側の Swing 機能を使う方が筋がよい |
| `vol` | 0–1 | `masterGain` | 0–1 | ◎ |
| `wave` | saw/sqr | `bassWave` (Choice) | 0/1 | ◎ |
| `cutoff` | 100–5000 Hz | `cutoff` | 100–5000 Hz | ◎ スキュー0.35で低域に解像度 |
| `reso` | 0–20 (Tone Q) | `reso` | 0–1 (Ladder) | ⚠️ 単位が違う。0–20 と 0–1 はざっくり「強さ」が比例 |
| `envMod` | 0–6000 Hz | `envMod` | 0–6000 Hz | ◎ |
| `bassDec` | 0.05–1.5 s | `bassDec` | 0.05–1.5 s | ◎ |
| `accAmt` | 0–1 | `accAmt` | 0–1 | ◎ |
| `delMix` | 0–0.6 | `delMix` | 0–0.6 | ◎ |
| `revMix` | 0–0.6 | `revMix` | 0–0.6 | ◎ |
| `dist` | 0–1 | `dist` | 0–1 | ◎ Tone.js Distortion はwaveshaper、こちらも tanh waveshaper |
| `kickDec` | – | `kickDec` | 0.05–1.5 s | ◎ |
| `snareDec` | – | `snareDec` | 0.05–0.6 s | ◎ |
| `hhDec` | – | `hhDec` | 0.01–0.4 s | ◎ |

---

## 2. 音源対応

### Bass (303 風)

| Tone.js | C++ (JUCE) |
|---|---|
| `MonoSynth(oscillator:sawtooth/square)` | 内製の saw/square オシレーター (BassVoice) |
| `Filter({type:'lowpass', rolloff:-24, Q:8})` | `juce::dsp::LadderFilter` (`LPF24`モード) |
| `filterEnvelope({decay, octaves})` | 指数減衰の `filterEnv` を毎サンプル更新 |
| Tone.js のアクセント時 `cutoff + envMod*accAmt` → リニア戻り | アクセント時 `filterEnv` を `(1+accAmt)` 倍 |
| `note + duration` でゲート制御 | アンプエンベロープが decay 経過で勝手に消える |

### Drums

| 元 (Tone.js) | C++ |
|---|---|
| `MembraneSynth({pitchDecay:0.05, octaves:6})` for Kick | KickVoice: sine + 6oct ピッチ減衰 + AR |
| `NoiseSynth(white) + MembraneSynth(E2)` for Snare | SnareVoice: white noise + 200Hz トーナル層 |
| `NoiseSynth(white) → Filter(8kHz BP)` for HiHat | HiHatVoice: WhiteNoise + IIRBP 8kHz |
| `NoiseSynth(pink) → Filter(1.2kHz BP)` for Clap | ClapVoice: Pink noise + IIRBP 1.2kHz + マルチバーストゲート |

### FX チェーン

| 元 | C++ |
|---|---|
| `FeedbackDelay(delayTime:'8n.', feedback:0.2)` | `juce::dsp::DelayLine` × 2ch、付点8分、feedback 0.35 |
| `Reverb({decay:1.5, wet})` | `juce::Reverb` (room 0.75, damp 0.4) |
| `Distortion(amt)` | `juce::dsp::WaveShaper` で `tanh(drive*x)/tanh(drive)` |
| `Gain (master)` | `buffer.applyGain` |

---

## 3. シーケンサーの扱い

Web 版は **16-step 内蔵シーケンサー** が音源と一体ですが、VST3 版ではこれを **DAW のシーケンサーに任せる** 設計にしています。理由：

- VST3 として標準的な使い方（DAWで音色/エフェクトとして使う）に合わせるため
- 内蔵シーケンサーを実装するとUIが複雑化し、ホスト同期も実装が必要
- 元 HTML の MIDI エクスポート機能を使えば、Web版で作ったパターンをそのまま VST3 に流せる

将来内蔵シーケンサーを足したい場合は、`processBlock` 内で `getPlayHead()->getPosition()->getPpqPosition()` を読んで自前のステップカウンタを進めれば実装できます。

---

## 4. 既知の差異 (Known Differences)

1. **フィルタの音色**: Tone.js は biquad ベース、C++ 側は Moog ladder。レゾナンスの効き方が違う（ladderの方が「アシッド」らしい音になる）
2. **アクセント挙動**: 元はリニアランプ、こちらは指数減衰。実用上は近いが厳密一致ではない
3. **Tone.js Reverb (Schroeder/Freeverb系) vs JUCE Reverb**: 残響特性が若干違う
4. **MIDIエクスポートやパターン保存**: 未実装（Web版で十分代替できるため）

---

## 5. 動作確認チェックリスト

ビルドが通って音が出るまでの確認順：

- [ ] `cmake -B build -G Xcode` がエラーなく完了する
- [ ] Xcode で `PacificSynthesis_VST3` スキームをビルドできる
- [ ] `~/Library/Audio/Plug-Ins/VST3/Pacific Synthesis.vst3` が生成される
- [ ] Standalone (`Pacific Synthesis.app`) を起動して MIDI キーボードで C1 → Kick が鳴る
- [ ] D1 → Snare、F#1 → HiHat、D#1 → Clap が鳴る
- [ ] C2 などのノートでベースが鳴る
- [ ] Cutoff/Resonance ノブを動かして 303 的に音色が変わる
- [ ] Velocity を 110 にして弾くとアクセントが効く（フィルタが大きく開く）
- [ ] Delay/Reverb/Distortion ノブで FX がかかる
- [ ] DAW (Logic / Ableton / Cubase) で読み込めて、ステップシーケンサで打ち込みできる
