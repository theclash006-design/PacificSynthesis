# Pacific Synthesis

> **Coast Machine — Drum + Bass Synthesizer**
> 16ステップ内蔵シーケンサー付き、303風ベース + 808/909系ドラム VST3 プラグイン

![Pacific Synthesis Screenshot](docs/screenshots/main.png)

*↑ ここにスクリーンショットが入ります (`docs/screenshots/main.png` を追加してください)*

---

## これは何？

**Pacific Synthesis** は Buchla / Roland TR-808 / TB-303 にインスパイアされた
ローファイ・ドラム&ベースシンセサイザーです。

- **16ステップシーケンサ**内蔵 - ホスト同期 + スタンドアロン再生対応
- **303 風ベース**: ladder filter + accent + slide + sub-osc + drive + LFO (cutoff/pitch)
- **アナログ風ドラム** 4音: Kick / Snare / HiHat (Closed + Open) / Clap
- **FX**: Delay (テンポ同期) + Reverb + Distortion (2x oversampled)
- **Master Bus Compressor**: VCA-style、Threshold/Ratio/Attack/Release
- **Kick→Bass サイドチェイン**: ダンス系で定番のダック効果
- **パート別 Pan**: ステレオ広がりを自由に設定
- **音色プリセット**: `~/Documents/Pacific Synthesis/Presets/`
- **MIDI エクスポート**: ファイル保存 or DAW トラックへ直接ドラッグ&ドロップ
- **Buchla 風 UI**: Cream / Navy / Red パレット、Courier フォント、カスタムノブ

> このプラグインは [元 Web 版](https://github.com/...) からの C++ 移植です。

---

## ステータス

🟢 **v1.0.0 — Public Release**

PluginVal Strictness 10 通過済み (192kHz / 32 sample blocksize まで検証)。
本格運用に耐える品質。バグ報告・フィードバック歓迎。

---

## 動作環境

- **macOS 10.15+** (Catalina 以降)
- **VST3 対応 DAW** (Logic Pro は AU 経由)
  - 確認済: Logic Pro, Ableton Live, Cubase, Studio One, Reaper, Bitwig
- **Intel Mac / Apple Silicon (M1/M2/M3/M4)** どちらも OK

---

## インストール (バイナリ配布の場合)

`.vst3` バンドルを以下にコピーするだけ:

```bash
cp -R "Pacific Synthesis.vst3" ~/Library/Audio/Plug-Ins/VST3/
```

AU 版 (Logic Pro 用) なら:

```bash
cp -R "Pacific Synthesis.component" ~/Library/Audio/Plug-Ins/Components/
```

DAW を再起動すれば認識されます。

---

## ソースからビルドする

### 必要なもの
- **Xcode 14+** (App Store からインストール)
- **CMake 3.22+** (`brew install cmake`)
- **Git**

### 手順

```bash
# 1. このリポをクローン
git clone https://github.com/<YOUR_GITHUB_USERNAME>/PacificSynthesisVST3.git
cd PacificSynthesisVST3

# 2. JUCE を取得 (親フォルダに置く)
cd ..
git clone --depth 1 https://github.com/juce-framework/JUCE
cd PacificSynthesisVST3

# 3. Xcode プロジェクト生成
cmake -B build -G Xcode

# 4. ビルド
cmake --build build --config Release
```

完了すると以下にコピーされます:

- VST3 → `~/Library/Audio/Plug-Ins/VST3/Pacific Synthesis.vst3`
- AU → `~/Library/Audio/Plug-Ins/Components/Pacific Synthesis.component`
- Standalone → `build/PacificSynthesis_artefacts/Release/Standalone/`

---

## 使い方

### 基本

1. DAW で MIDI トラックを作る
2. Pacific Synthesis をインサート
3. パターンを編集 (グリッドのセルをクリック) → ▶ ボタンで再生
4. DAW の再生に同期させたい場合は DAW のトランスポートを使う

### MIDI ノートマッピング

外部 MIDI から鳴らす場合 (channel 10 で GM 規格、その他は bass + 高領域ドラム):

| ノート | 音色 |
|---|---|
| C1  (36) | Kick |
| D1  (38) | Snare |
| D#1 (39) | Clap |
| F#1 (42) | HiHat (Closed) |
| A#1 (46) | HiHat (Open) — Closed と排他 (チョーク) |
| その他 (channel ≠ 10) | Bass |

高領域 (C5–G#5) でも同じ音色がトリガーされるので、MIDI 書き出し後に DAW で扱いやすい:

| ノート | 音色 |
|---|---|
| C5  (84) / D5 (86) / D#5 (87) / F#5 (90) / G#5 (92) | Kick / Snare / Clap / HiHat / Open HH |

**Velocity ≥ 100** でベースのアクセント、**ノートを重ねる** とスライド。

### タブ

下部のタブで音色を詳細に編集:

| タブ | 内容 |
|---|---|
| **MAIN** | 各パートの VOL ミックス + FX (Delay/Reverb/Dist) + SC AMT/REL |
| **KICK** | Pitch / Amount / Decay / Drive / Pan / Vol |
| **SNARE** | Noise / Tone / Decay / Snap / Pan / Vol |
| **HIHAT** | Freq / Q / Closed Dec / Open Dec / Metal / Pan / Vol |
| **CLAP** | Freq / Q / Decay / Spread / Pan / Vol |
| **BASS** | Wave / Cutoff / Reso / Env / Decay / Accent / Glide / Drive / Sub / LFO (Rate/Depth/Dst) / Pan / Vol |
| **COMP** | Master Bus Compressor (Threshold / Ratio / Attack / Release) |

### プリセット

- 右上の `PRESET` ボタンをクリック → メニューから選択
- 保存先: `~/Documents/Pacific Synthesis/Presets/`
- Finder で開く: `PRESET` → `Reveal Presets Folder...`

### MIDI エクスポート

- `↓ MIDI` ボタン:
  - **クリック**: MIDI ファイルとして保存
  - **ドラッグ**: DAW の MIDI トラックに直接ドロップ

---

## 既知の問題 / TODO

- [ ] Windows 版未対応 (macOS のみ)
- [ ] 3-band Master EQ 未実装
- [ ] Reverb mode 切替 (Plate/Room/Hall) 未実装
- [x] 内蔵ファクトリープリセット 6 種 (v1.1)
- [x] エディタのリサイズ対応 (v1.0)
- [x] アンチエイリアシング PolyBLEP (v1.0)

詳細な開発ロードマップは [ROADMAP.md](./ROADMAP.md) を参照。

---

## フィードバックの送り方

このリポのアクセス権がある場合:
- GitHub Issues を立てる (バグ報告・要望)
- Pull Request 歓迎

それ以外: SHUH まで直接連絡してください。

特に欲しいフィードバック:
1. **クラッシュ・フリーズの有無** (どの DAW で、何をしてた時か)
2. **音の感想** (好き嫌い、足りないキャラクターなど)
3. **UI の使いやすさ** (迷ったポイント、欲しい機能)

---

## 技術スタック

- [JUCE 8](https://juce.com/) — Audio plugin framework
- C++17
- CMake build system
- macOS native (Cocoa / Audio Unit / Core Audio)

---

## ライセンス

GNU General Public License v3.0

詳細は [LICENSE](./LICENSE) と [COPYING.md](./COPYING.md) を参照してください。

このプラグインを商用配布する場合、JUCE Indie / Pro ライセンスの購入が
追加で必要になります (https://juce.com/get-juce/)。

---

## クレジット

- **Concept & Development**: SHUH (Coast Machine)
- **Built on**: [JUCE](https://juce.com/) by Raw Material Software
- **Inspired by**: Roland TR-808, TB-303, Buchla, Sonic Charge

---

*Made with ☕ and 🎛️ on the West Coast (of Japan)*
