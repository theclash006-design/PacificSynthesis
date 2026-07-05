# Git チートシート — PacificSynthesis 用

このファイルはいつでも `~/Downloads/PacificSynthesisVST3/GIT_チートシート.md` で見られます。

---

## 0. 前提

ターミナルで毎回最初に：
```bash
cd ~/Downloads/PacificSynthesisVST3
```

これ以降のコマンドはすべてこのフォルダの中で実行。

---

## 1. 日常の3コマンド (これだけで7割OK)

```bash
git status                  # 今、何が変わってる？
git add .                   # 変更を全部ステージング
git commit -m "メッセージ"   # スナップショット作成
```

**ショートカット**: 上2つを一発で:
```bash
git commit -am "メッセージ"
```

### コミットメッセージの書き方の目安

短く現在形で。後から自分が読んで分かれば十分。

良い例：
- `"VOLノブのフォントサイズ変更"`
- `"Kickのpitchパラメータを追加"`
- `"WAVEボタンのバグ修正"`

避ける例：
- `"修正"` ← 何を？
- `"てすと"` ← 後で何のコミットか分からない

---

## 2. 履歴を見る

```bash
git log --oneline           # 1行ずつ全コミット表示
git log --oneline -10       # 直近10件だけ
git log                     # 詳しく (Q キーで終了)
```

出力例：
```
a3b4c5d (HEAD -> main) VOLノブ追加
3f458a9 snapshot: VOLノブあり + 枠線消し
```

左の `a3b4c5d` がコミットの ID (ハッシュ)。これを使って「あの時点」を指す。

---

## 3. 変更内容を見る

```bash
git diff                    # まだコミットしてない変更を表示
git diff Source/PluginEditor.cpp   # 特定のファイルだけ
git show a3b4c5d            # ある時点で何が変わったか
git log --oneline Source/PluginEditor.cpp   # ファイル単位の履歴
```

---

## 4. 過去に戻る (3つのレベル)

### A. 一部のファイルだけ昔の状態にしたい (一番安全)

```bash
git checkout 3f458a9 -- Source/PluginEditor.cpp
```
→ PluginEditor.cpp だけ昔の状態に戻る。他はそのまま。

確認して気に入ったら `git commit -am "PluginEditorを巻き戻し"`。
やっぱり戻したいなら `git checkout HEAD -- Source/PluginEditor.cpp` (今の最新に戻す)。

### B. 全ファイルを昔の状態にしたい

```bash
git checkout 3f458a9 -- .
```
→ 全部が昔に戻る。まだコミットしてないので：
- 気に入った → `git commit -am "全部巻き戻し"`
- やっぱりキャンセル → `git checkout HEAD -- .`

### C. 完全にその時点に戻す (危険)

```bash
git reset --hard 3f458a9
```
→ それ以降のコミットが**全消し**。確実に戻したい時だけ。

---

## 5. よくある場面別レシピ

### 「Claude に変更してもらった、ビルドして動いた」

```bash
git commit -am "今回の変更内容を一言で"
```

### 「Claude に変更してもらった、ビルドが失敗した」

戻りたい場合：
```bash
git checkout HEAD -- .      # 直前のコミット状態に全部戻す
```

### 「コミットしちゃったけど取り消したい (まだ動作確認してない)」

```bash
git reset --soft HEAD~1     # 直前のコミットを取り消す (ファイルはそのまま)
```

### 「コミット履歴を見てどの時点が一番良かったか思い出したい」

```bash
git log --oneline -20
```

### 「ファイルの中身を昔のと比較したい」

```bash
git diff 3f458a9 -- Source/PluginEditor.cpp
```

### 「うっかり削除しちゃったファイルを復活」

```bash
git checkout HEAD -- 消したファイル.h
```

---

## 6. ビルド成果物 (.vst3) もバージョン保存したい

git は `build/` フォルダを無視しているので、ビルドした .vst3 自体は記録されません。
バージョンごとに残したい時は手動で：

```bash
# 例: バージョン v0.3 として保存
cp -R "~/Library/Audio/Plug-Ins/VST3/Pacific Synthesis.vst3" \
      "~/Library/Audio/Plug-Ins/VST3/Pacific Synthesis v0.3.vst3"
```

DAW のプラグインリストに別物として並びます。

---

## 7. 困った時の三種の神器

```bash
git status                  # ① 今の状況を見る
git log --oneline           # ② 履歴を見る
git diff                    # ③ 変更を見る
```

この3つを叩けば 8 割の状況は理解できます。

---

## 8. 出てきても無視していい警告

```
warning: unable to access '/Users/bunnymen/.config/git/attributes': Permission denied
```

→ サンドボックス環境の都合。git の動作には影響ないので無視してOK。

---

## 9. もう少し凝りたくなったら

- **ブランチ** = 並行で別バージョンを試す
  ```bash
  git branch experiment   # "experiment" ブランチ作成
  git checkout experiment # そっちに切り替え
  # ... いろいろ実験 ...
  git checkout main       # 元のmainに戻る
  ```

- **タグ** = 特定のコミットに名前をつける
  ```bash
  git tag v0.3            # 今のコミットを "v0.3" と名付ける
  git checkout v0.3       # 後で "v0.3" の時点に行ける
  ```

ただし当面は **commit / log / checkout** の3つで十分です。
