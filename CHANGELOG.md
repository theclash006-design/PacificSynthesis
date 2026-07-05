# Changelog

このプロジェクトの主要な変更履歴。新しいバージョンが上に追加されます。

形式: [Keep a Changelog](https://keepachangelog.com/ja/1.1.0/) に準拠。
バージョニング: [Semantic Versioning](https://semver.org/lang/ja/) (MAJOR.MINOR.PATCH)。

---

## [Unreleased]

### Added
- **内蔵ファクトリープリセット** 16 種 — PRESET メニュー → Factory Presets:
  - 01 Init / 02 Acid Tech 128 / 03 Deep House 122 / 04 Techno 135 /
    05 Ambient 70 / 06 Industrial 140 / 07 Dub Techno 128 / 08 UK Garage 132 /
    09 Drum & Bass 174 / 10 Lofi Hip Hop 85 / 11 Trance 138 /
    12 Synthwave 110 / 13 Detroit Techno 130 / 14 Acid House 125 /
    15 Berlin Minimal 126 / 16 Hard Techno 145

### Coming
- Windows 版
- 3-band Master EQ
- Reverb mode 切替 (Plate/Room/Hall)

---

## [1.0.0] — 2026-05-29 (Public Release)

初の一般公開リリース。DSP 拡張一式 + 商用品質の音作り機能を全て搭載。
PluginVal Strictness 10 通過済み。

### Added
- **Kick→Bass サイドチェイン** — SC AMT / SC REL でダック量と戻りを制御 (MAIN タブ)
- **Master Bus Compressor** — VCA-style コンプ (THRESH/RATIO/ATTACK/RELEASE、COMP タブ)
- **Distortion 2x オーバーサンプリング** — tanh waveshape のエイリアシング軽減
  (juce::dsp::Oversampling、IIR halfband、整数レイテンシ報告)
- **HiHat Open/Closed チョーク** — Open HH 用に hhOpenDec パラメータ、
  MIDI note A#1 (GM) / G#5 で Open HH トリガー、Closed と排他的にチョーク
- **パート別 Pan** — Kick/Snare/HiHat/Clap/Bass に constant-power pan、
  デフォルト値でステレオ感あり (Snare -0.15 / HiHat +0.25 / Clap -0.25)
- **Bass LFO** — sin LFO で Cutoff (±2 oct) または Pitch (±1 semi) をモジュレート
  (BASS タブ、Rate/Depth/Target)
- **COMP タブ** 新設 (7 タブ目)
- **PolyBLEP オシレータ** — Bass saw/square のアンチエイリアシング
- **マスターリミッタ** — tanh ソフトクリップ (0dBFS 直前のみ)
- **音色詳細ノブ群** — Kick (Pitch/Amount/Drive)、Snare (Noise/Tone/Snap)、
  HiHat (Freq/Q/Metal)、Clap (Freq/Q/Spread)、Bass (Glide/Drive/Sub)
- **Undo/Redo** (⌘Z / ⌘⇧Z) — パターン編集向け
- **ステップごとの drag-to-paint** — ドラム行のドラッグで一気に塗れる
- **VIEW モード** (Both / Drum-only / Bass-only) — エディタを動的縮小

### Changed
- processBlock を mono バス経由 → ステレオ直書きに改修 (Pan 対応のため)
- Kick voice にアタックノイズクリック層を追加 (パンチ感向上)
- HiHat / Clap / Snare の内部ゲインを調整 (リミッタ緩和に伴う再バランス)
- std::pow(2, x) → std::exp2(x) に最適化 (per-sample 呼び出し)

### Technical
- ホストにオーバーサンプル由来のレイテンシ報告 (`setLatencySamples`)
- 全パラメータ atomic ポインタキャッシュ (RT-safe、文字列ルックアップ無し)
- PluginVal Strictness 10 で 192kHz / 32 sample blocksize まで通過
- macOS 10.15+ (Intel & Apple Silicon)

### Known Issues
- pluginval を `--randomise` 付きで実行すると `Plugin state restoration` テストが
  失敗することがある — `AudioParameterChoice` (2値) と pluginval のトレランス
  比較の組合せに起因する既知の偽陽性。実 DAW での state save/load は正常。

---

## [0.1.0] — 2026-05-20 (Friends-Only Beta)

初期 Friends-Only リリース。PluginVal Strictness 10 通過済み。

### Added
- 16ステップシーケンサ (ホスト同期 + 内蔵Play)
- ドラム 4音 (Kick / Snare / HiHat / Clap) — 各音色詳細パラメータ付き
- 303 風ベース (Saw/Square, Ladder filter, Accent, Slide, Glide, Drive, Sub)
- FX チェーン (Delay tempo-sync / Reverb / Distortion)
- 個別 VOL ノブ (5パート) + Master Gain
- プリセット保存・読込 (`~/Documents/Pacific Synthesis/Presets/`)
- MIDI エクスポート (ファイル保存 + DAW ドラッグ&ドロップ)
- パターンエディタ UI (ステップグリッド + Bass Step Editor)
- 6 タブの音色エディタ (MAIN / KICK / SNARE / HIHAT / CLAP / BASS)
- Buchla 風 LookAndFeel (Cream/Navy/Red パレット, カスタムノブ, Courier フォント)

### Technical
- JUCE 8 + CMake ベース
- VST3 + AU + Standalone
- macOS 10.15+ (Intel & Apple Silicon)
- PluginVal Strictness 10 通過
- パラメータポインタキャッシュ (RT-safe)
- PatternState の double-buffer 化
- NaN/Inf 出力ガード
