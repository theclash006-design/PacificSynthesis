#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PacificColors.h"
#include "../PatternState.h"

// ─────────────────────────────────────────────────────────────
//  StepGridComponent — 元HTMLのレイアウトを再現:
//
//   [LED row]      ●  ●  ●  ●  ●  ●  ●  ●  ●  ●  ●  ●  ●  ●  ●  ●
//                   1                2                3                4
//
//   DRUMS ──────────────────────────────────────────────────────
//   ● KICK    [ □ ][ ■ ][ ■ ][ □ ][ □ ][ □ ]...
//   ● SNARE   ...
//   ● HIHAT   ...
//   ● CLAP    ...
//
//   BASSLINE ───────────────────────────────────────────────────
//             [   ][ C2 ][   ][   ][ G1 ][ A#2 ]...
//
//   ┌────────────────────────────────────────────────────────────┐
//   │ STEP 1   [OFF/ON]  ▭▭▭mini keys▭▭▭  OCT 1 2 3  ACC SLD  ◀▶│
//   └────────────────────────────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────
class StepGridComponent : public juce::Component, private juce::Timer
{
public:
    enum class ViewMode { Both = 0, DrumOnly, BassOnly };

    using GetStepFn = std::function<int()>;
    // パターン編集を Undo 対象として外側に委譲するためのコールバック。
    //   name : 操作名 (Undo 履歴に表示される)
    //   edit : 実際の編集処理 (これを呼ぶ前後で外側が ValueTree をスナップショット)
    using EditCallback = std::function<void (const juce::String& name,
                                              std::function<void()> edit)>;

    StepGridComponent (PatternState& patternRef, GetStepFn getCurrent)
        : pattern (patternRef), getCurrentStep (std::move (getCurrent))
    {
        startTimerHz (30);
    }
    ~StepGridComponent() override { stopTimer(); }

    void setEditCallback (EditCallback cb) { onEdit = std::move (cb); }

    void setViewMode (ViewMode vm)
    {
        if (viewMode == vm) return;
        viewMode = vm;
        resized();
        repaint();
    }
    ViewMode getViewMode() const noexcept { return viewMode; }

    // ── 各トラック行の bounds ゲッター (PluginEditor が INT/EXT ボタンを配置するため) ──
    juce::Rectangle<int> getDrumRowBounds (int r) const noexcept
    {
        if (r < 0 || r >= PatternState::NumDrumRows) return {};
        return drumRows[r];
    }
    juce::Rectangle<int> getBassRowBounds() const noexcept { return bassRow; }
    int getCellsRightX() const noexcept { return cellsRight; }
    int getRightMargin() const noexcept { return kRightMargin; }

    void resized() override
    {
        auto area = getLocalBounds().reduced (2);

        // 左の "ラベル列" 幅 (KICK / SNARE / HIHAT / CLAP / BASSLINE が入る)
        const int labelW = 62;

        // 全行の「セル領域 = ラベル右側」を覚えておく
        // 右側に INT/EXT ボタン用の余白を確保
        cellsLeft  = labelW + 4;
        cellsRight = getWidth() - 2 - kRightMargin;

        // 隠れているパーツは空 Rectangle にしておく (mouseDown が無視するため)
        drumHeaderRow = {};
        bassHeaderRow = {};
        bassRow       = {};
        bassEditorArea = {};
        for (auto& r : drumRows) r = {};

        // ── LED row (上端)
        ledRow = area.removeFromTop (12);
        area.removeFromTop (2);
        // LEDの下に "1 2 3 4" マーカ
        markerRow = area.removeFromTop (10);
        area.removeFromTop (6);

        const bool showDrums = (viewMode != ViewMode::BassOnly);
        const bool showBass  = (viewMode != ViewMode::DrumOnly);

        // ── 固定サイズ: ViewMode に関係なく BOTH と同じ縦横比を維持 ──
        // (隠した側の領域は空白として残る)
        constexpr int drumHeaderH = 16;
        constexpr int drumRowH    = 32;
        constexpr int drumRowGap  = 3;
        constexpr int bassHeaderH = 16;
        constexpr int bassRowH    = 46;
        constexpr int bassEditorH = 70;  // 鍵盤 36px + padding、余白詰めて compact に

        if (showDrums)
        {
            // ── DRUMS 見出し
            drumHeaderRow = area.removeFromTop (drumHeaderH);
            area.removeFromTop (4);

            for (int r = 0; r < PatternState::NumDrumRows; ++r)
            {
                drumRows[r] = area.removeFromTop (drumRowH);
                if (r < PatternState::NumDrumRows - 1) area.removeFromTop (drumRowGap);
            }
            area.removeFromTop (8);
        }

        if (showBass)
        {
            // ── BASSLINE 見出し
            bassHeaderRow = area.removeFromTop (bassHeaderH);
            area.removeFromTop (4);

            bassRow = area.removeFromTop (bassRowH);
            area.removeFromTop (8);

            // bass editor: 固定高さ (余り領域は使わない)
            bassEditorArea = area.removeFromTop (juce::jmin (area.getHeight(), bassEditorH));
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::transparentBlack); // 親の cream を透過

        drawLedRow (g);
        drawMarkerRow (g);
        if (viewMode != ViewMode::BassOnly)
        {
            drawSectionHeader (g, drumHeaderRow, "DRUMS", PacificColors::red);
            drawDrumRows (g);
        }
        if (viewMode != ViewMode::DrumOnly)
        {
            drawSectionHeader (g, bassHeaderRow, "BASSLINE", PacificColors::blue);
            drawBassRow (g);
            drawBassEditor (g);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pos = e.position;

        for (int r = 0; r < PatternState::NumDrumRows; ++r)
        {
            if (drumRows[r].toFloat().contains (pos))
            {
                const int s = stepFromX (pos.x);
                if (s >= 0)
                    applyEdit ("Toggle drum", [this, r, s] { pattern.toggleDrum (r, s); });
                return;
            }
        }
        if (bassRow.toFloat().contains (pos))
        {
            const int s = stepFromX (pos.x);
            if (s >= 0)
            {
                if (selectedBassStep == s)
                    applyEdit ("Toggle bass step", [this, s] { pattern.toggleBass (s); });
                else
                {
                    selectedBassStep = s;
                    repaint();
                }
            }
            return;
        }
        handleBassEditorClick (pos);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto pos = e.position;
        for (int r = 0; r < PatternState::NumDrumRows; ++r)
        {
            if (drumRows[r].toFloat().contains (pos))
            {
                const int s = stepFromX (pos.x);
                if (s >= 0)
                {
                    auto c = pattern.getDrum (r, s);
                    if (! c.on)
                        applyEdit ("Drag drum", [this, r, s] {
                            auto cc = pattern.getDrum (r, s);
                            cc.on = true;
                            pattern.setDrum (r, s, cc);
                        });
                }
                return;
            }
        }
    }

private:
    void timerCallback() override
    {
        const int cur = getCurrentStep ? getCurrentStep() : -1;
        if (cur != lastDrawnStep) { lastDrawnStep = cur; repaint(); }
    }

    // セル幅 (16等分)
    float cellWidth() const { return (float)(cellsRight - cellsLeft) / (float) PatternState::NumSteps; }

    int stepFromX (float x) const
    {
        const float relX = x - (float) cellsLeft;
        if (relX < 0) return -1;
        const int s = (int) (relX / cellWidth());
        return (s >= 0 && s < PatternState::NumSteps) ? s : -1;
    }

    juce::Rectangle<float> cellBounds (juce::Rectangle<int> row, int step, float pad = 1.5f) const
    {
        const float cw = cellWidth();
        return juce::Rectangle<float> ((float) cellsLeft + cw * step + pad,
                                       (float) row.getY() + pad,
                                       cw - pad * 2.0f,
                                       (float) row.getHeight() - pad * 2.0f);
    }

    void drawLedRow (juce::Graphics& g)
    {
        const int curStep = getCurrentStep ? getCurrentStep() : -1;
        const float cw = cellWidth();
        for (int s = 0; s < PatternState::NumSteps; ++s)
        {
            const float cx = (float) cellsLeft + cw * (s + 0.5f);
            const float cy = (float) ledRow.getCentreY();
            const bool quarterMark = (s % 4 == 0);
            const auto col = quarterMark ? PacificColors::red : PacificColors::redLight;
            const bool on  = (s == curStep);
            g.setColour (on ? col : juce::Colour (0xffa09888));
            g.fillEllipse (cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
            if (on)
            {
                g.setColour (col.withAlpha (0.45f));
                g.fillEllipse (cx - 5.5f, cy - 5.5f, 11.0f, 11.0f);
            }
        }
    }

    void drawMarkerRow (juce::Graphics& g)
    {
        const float cw = cellWidth();
        g.setColour (PacificColors::textLight);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 7.5f, juce::Font::bold)));
        for (int s = 0; s < PatternState::NumSteps; s += 4)
        {
            const float cx = (float) cellsLeft + cw * (s + 0.5f);
            g.drawText (juce::String (s / 4 + 1),
                        juce::Rectangle<int> ((int) cx - 8, markerRow.getY(), 16, markerRow.getHeight()),
                        juce::Justification::centred, false);
        }
    }

    void drawSectionHeader (juce::Graphics& g, juce::Rectangle<int> headerRow,
                            const juce::String& title, juce::Colour color)
    {
        // 左寄せのテキスト
        g.setColour (color);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 9.0f, juce::Font::bold)));
        g.drawText (title, headerRow.getX(), headerRow.getY(),
                    140, headerRow.getHeight(), juce::Justification::centredLeft, false);

        // 横線 (全幅)
        g.setColour (color);
        g.drawLine ((float) headerRow.getX(), (float) headerRow.getBottom() - 1.5f,
                    (float) headerRow.getRight(), (float) headerRow.getBottom() - 1.5f, 1.5f);
    }

    void drawDrumRows (juce::Graphics& g)
    {
        static const char* labels[] = { "KICK", "SNARE", "HIHAT", "CLAP" };
        static const juce::Colour cols[] = {
            PacificColors::drumKick, PacificColors::drumSnare,
            PacificColors::drumHiHat, PacificColors::drumClap
        };
        const int curStep = getCurrentStep ? getCurrentStep() : -1;

        for (int r = 0; r < PatternState::NumDrumRows; ++r)
        {
            // ラベル領域 (左)
            const auto labelArea = juce::Rectangle<int> (drumRows[r].getX(), drumRows[r].getY(),
                                                         cellsLeft - 4, drumRows[r].getHeight());
            // 小さい色付きドット
            const float dotR = 2.5f;
            const float dotX = (float) labelArea.getRight() - 56.0f;
            const float dotY = (float) labelArea.getCentreY();
            g.setColour (cols[r]);
            g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
            // テキストラベル
            g.setColour (cols[r]);
            g.setFont (juce::Font (juce::FontOptions ("Courier New", 9.0f, juce::Font::bold)));
            g.drawText (labels[r], dotX + 6.0f, (float) labelArea.getY(),
                        50.0f, (float) labelArea.getHeight(),
                        juce::Justification::centredLeft, false);

            // セル群
            for (int s = 0; s < PatternState::NumSteps; ++s)
            {
                const auto cb = cellBounds (drumRows[r], s);
                const auto cell = pattern.getDrum (r, s);
                const bool isCur = (s == curStep);
                const bool quarterMark = (s % 4 == 0);

                if (cell.on)
                {
                    auto fill = isCur ? cols[r] : cols[r].withAlpha (0.85f);
                    g.setColour (fill);
                    g.fillRoundedRectangle (cb, 2.0f);
                    g.setColour (cols[r].darker (0.2f));
                    g.drawRoundedRectangle (cb, 2.0f, 1.0f);
                    // 中央ドット
                    g.setColour (juce::Colours::white.withAlpha (isCur ? 0.95f : 0.7f));
                    const float r2 = isCur ? 3.0f : 2.2f;
                    g.fillEllipse (cb.getCentreX() - r2, cb.getCentreY() - r2, r2 * 2.0f, r2 * 2.0f);
                }
                else
                {
                    g.setColour (isCur ? PacificColors::creamLight : PacificColors::cream);
                    g.fillRoundedRectangle (cb, 2.0f);
                    g.setColour (quarterMark ? PacificColors::aluminumLight : PacificColors::panelDark);
                    g.drawRoundedRectangle (cb, 2.0f, 1.0f);
                }
            }
        }
    }

    void drawBassRow (juce::Graphics& g)
    {
        const int curStep = getCurrentStep ? getCurrentStep() : -1;

        for (int s = 0; s < PatternState::NumSteps; ++s)
        {
            const auto cb = cellBounds (bassRow, s);
            const auto cell = pattern.getBass (s);
            const bool isCur = (s == curStep);
            const bool isSel = (s == selectedBassStep);

            juce::Colour fill;
            if (cell.on) fill = isSel ? PacificColors::blue
                                       : (isCur ? PacificColors::navyDark : PacificColors::navy);
            else         fill = isSel ? PacificColors::panelDark
                                       : (isCur ? PacificColors::creamLight : PacificColors::cream);
            g.setColour (fill);
            g.fillRoundedRectangle (cb, 2.0f);

            g.setColour (isSel ? PacificColors::blue : PacificColors::panelDark);
            g.drawRoundedRectangle (cb, 2.0f, isSel ? 1.6f : 1.0f);

            if (cell.on)
            {
                g.setColour (juce::Colours::white);
                g.setFont (juce::Font (juce::FontOptions ("Courier New", 9.5f, juce::Font::bold)));
                const juce::String txt = juce::String (PatternState::noteName (cell.note))
                                       + juce::String (cell.octave);
                g.drawText (txt, cb.toNearestInt().withTrimmedTop (4),
                            juce::Justification::centredTop, false);

                if (cell.accent)
                {
                    g.fillEllipse (cb.getCentreX() - 4.0f, cb.getBottom() - 7.0f, 3.0f, 3.0f);
                }
                if (cell.slide)
                {
                    g.fillRect (cb.getCentreX() + 1.0f, cb.getBottom() - 6.0f, 7.0f, 1.5f);
                }
            }
        }
    }

    // ─── Bass Editor ───
    void drawBassEditor (juce::Graphics& g)
    {
        if (bassEditorArea.isEmpty()) return;

        // 背景パネル
        g.setColour (PacificColors::creamLight);
        g.fillRoundedRectangle (bassEditorArea.toFloat(), 4.0f);
        g.setColour (PacificColors::panelDark);
        g.drawRoundedRectangle (bassEditorArea.toFloat(), 4.0f, 1.0f);

        // 縦中央に配置: bassEditorH=70 に対して、最大コンテンツ高さ=36 (鍵盤)
        // → 上下 padding 17 ずつで中央寄せ
        auto area = bassEditorArea.reduced (10, 17);
        const auto cell = pattern.getBass (selectedBassStep);

        // STEP 番号
        g.setColour (PacificColors::textDim);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 7.5f, juce::Font::bold)));
        g.drawText ("STEP " + juce::String (selectedBassStep + 1),
                    area.getX(), area.getY(), 60, 11, juce::Justification::left, false);

        // 大きい note 表示
        g.setColour (cell.on ? PacificColors::blue : PacificColors::textDim);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 16.0f, juce::Font::bold)));
        g.drawText (cell.on ? juce::String (PatternState::noteName (cell.note))
                              + juce::String (cell.octave)
                            : juce::String ("OFF"),
                    area.getX(), area.getY() + 11, 60, 22, juce::Justification::left, false);

        // OFF/ON ボタン
        onOffBtnBounds = juce::Rectangle<int> (area.getX() + 64, area.getY() + 12, 50, 22);
        g.setColour (cell.on ? PacificColors::red : PacificColors::panelDark);
        g.fillRoundedRectangle (onOffBtnBounds.toFloat(), 2.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 9.5f, juce::Font::bold)));
        g.drawText (cell.on ? "ON" : "OFF", onOffBtnBounds, juce::Justification::centred);

        // ミニ鍵盤
        const int kbX = onOffBtnBounds.getRight() + 14;
        const int kbY = area.getY();
        const int kbW = 7 * 22;  // 7白鍵 × 22px
        const int kbH = 36;
        keyboardArea = juce::Rectangle<int> (kbX, kbY, kbW, kbH);
        drawMiniKeys (g, cell);

        // OCT 1/2/3
        octBtnArea = juce::Rectangle<int> (keyboardArea.getRight() + 12, kbY + 7, 66, 20);
        g.setColour (PacificColors::textDim);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 7.5f, juce::Font::bold)));
        g.drawText ("OCT", octBtnArea.getX(), kbY - 8, 60, 10, juce::Justification::left, false);
        for (int o = 1; o <= 3; ++o)
        {
            const auto bb = octBtnBounds (o);
            g.setColour (cell.octave == o ? PacificColors::blue : juce::Colours::white);
            g.fillRoundedRectangle (bb.toFloat(), 2.0f);
            g.setColour (PacificColors::panelDark);
            g.drawRoundedRectangle (bb.toFloat(), 2.0f, 0.8f);
            g.setColour (cell.octave == o ? juce::Colours::white : PacificColors::text);
            g.setFont (juce::Font (juce::FontOptions ("Courier New", 9.5f, juce::Font::bold)));
            g.drawText (juce::String (o), bb, juce::Justification::centred);
        }

        // ACC / SLD
        accBtnBounds = juce::Rectangle<int> (octBtnArea.getRight() + 10, kbY + 7, 36, 20);
        sldBtnBounds = juce::Rectangle<int> (accBtnBounds.getRight() + 6, kbY + 7, 36, 20);
        drawToggle (g, accBtnBounds, cell.accent, PacificColors::red,  "ACC");
        drawToggle (g, sldBtnBounds, cell.slide,  PacificColors::blue, "SLD");

        // ◀ ▶ ナビゲーション (右側、SLD と過剰な空白を埋めるため少し左寄り)
        const int navY = kbY + 7;
        prevBtnBounds = juce::Rectangle<int> (bassEditorArea.getRight() - 110, navY, 22, 20);
        nextBtnBounds = juce::Rectangle<int> (bassEditorArea.getRight() -  84, navY, 22, 20);
        drawNavBtn (g, prevBtnBounds, "<");
        drawNavBtn (g, nextBtnBounds, ">");
    }

    void drawNavBtn (juce::Graphics& g, juce::Rectangle<int> bb, const juce::String& s)
    {
        g.setColour (PacificColors::cream);
        g.fillRoundedRectangle (bb.toFloat(), 2.0f);
        g.setColour (PacificColors::panelDark);
        g.drawRoundedRectangle (bb.toFloat(), 2.0f, 0.8f);
        g.setColour (PacificColors::text);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 11.0f, juce::Font::bold)));
        g.drawText (s, bb, juce::Justification::centred);
    }

    void drawToggle (juce::Graphics& g, juce::Rectangle<int> bb, bool on,
                     juce::Colour onColor, const juce::String& label)
    {
        g.setColour (on ? onColor.withAlpha (0.18f) : juce::Colours::transparentBlack);
        g.fillRoundedRectangle (bb.toFloat(), 2.0f);
        g.setColour (on ? onColor : PacificColors::panelDark);
        g.drawRoundedRectangle (bb.toFloat(), 2.0f, 1.5f);
        g.setColour (on ? onColor : PacificColors::textDim);
        g.setFont (juce::Font (juce::FontOptions ("Courier New", 9.5f, juce::Font::bold)));
        g.drawText (label, bb, juce::Justification::centred);
    }

    juce::Rectangle<int> octBtnBounds (int o) const
    {
        return juce::Rectangle<int> (octBtnArea.getX() + (o - 1) * 22,
                                     octBtnArea.getY(), 20, octBtnArea.getHeight());
    }

    void drawMiniKeys (juce::Graphics& g, const PatternState::BassCell& cell)
    {
        const int wkW = keyboardArea.getWidth() / 7;
        const int wkH = keyboardArea.getHeight();
        static const int whiteNotes[] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int blackNotes[] = { 1, 3, -1, 6, 8, 10, -1 };

        // 白鍵
        for (int i = 0; i < 7; ++i)
        {
            const int n = whiteNotes[i];
            juce::Rectangle<int> bb (keyboardArea.getX() + i * wkW,
                                     keyboardArea.getY(), wkW - 1, wkH);
            const bool sel = (cell.note == n && cell.on);
            g.setColour (sel ? PacificColors::red : juce::Colours::white);
            g.fillRoundedRectangle (bb.toFloat(), 1.5f);
            g.setColour (PacificColors::panelDark);
            g.drawRoundedRectangle (bb.toFloat(), 1.5f, 0.7f);
            g.setColour (sel ? juce::Colours::white : PacificColors::textDim);
            g.setFont (juce::Font (juce::FontOptions ("Courier New", 6.5f, juce::Font::bold)));
            g.drawText (juce::String (PatternState::noteName (n)),
                        bb.getX(), bb.getBottom() - 9, wkW, 9,
                        juce::Justification::centred, false);
        }
        // 黒鍵
        for (int i = 0; i < 7; ++i)
        {
            const int n = blackNotes[i];
            if (n < 0) continue;
            juce::Rectangle<int> bb (keyboardArea.getX() + i * wkW + (int) (wkW * 0.6f),
                                     keyboardArea.getY(),
                                     (int) (wkW * 0.7f), (int) (wkH * 0.6f));
            const bool sel = (cell.note == n && cell.on);
            g.setColour (sel ? PacificColors::red : juce::Colour (0xff303030));
            g.fillRoundedRectangle (bb.toFloat(), 1.5f);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.setFont (juce::Font (juce::FontOptions ("Courier New", 5.5f, juce::Font::bold)));
            g.drawText (juce::String (PatternState::noteName (n)),
                        bb.getX(), bb.getBottom() - 7, bb.getWidth(), 7,
                        juce::Justification::centred, false);
        }
    }

    void handleBassEditorClick (juce::Point<float> pos)
    {
        // DrumOnly 表示中はベース編集を一切受け付けない
        if (bassEditorArea.isEmpty()) return;

        const int s = selectedBassStep;

        if (onOffBtnBounds.toFloat().contains (pos))
        {
            applyEdit ("Toggle bass on/off", [this, s] {
                auto c = pattern.getBass (s); c.on = ! c.on;
                pattern.setBass (s, c);
            });
            return;
        }
        if (prevBtnBounds.toFloat().contains (pos))
        {
            selectedBassStep = juce::jmax (0, selectedBassStep - 1); repaint(); return;
        }
        if (nextBtnBounds.toFloat().contains (pos))
        {
            selectedBassStep = juce::jmin (PatternState::NumSteps - 1, selectedBassStep + 1);
            repaint(); return;
        }
        if (keyboardArea.toFloat().contains (pos))
        {
            const int wkW = keyboardArea.getWidth() / 7;
            static const int whiteNotes[] = { 0, 2, 4, 5, 7, 9, 11 };
            static const int blackNotes[] = { 1, 3, -1, 6, 8, 10, -1 };
            // 黒鍵優先
            for (int i = 0; i < 7; ++i)
            {
                const int n = blackNotes[i];
                if (n < 0) continue;
                juce::Rectangle<int> bb (keyboardArea.getX() + i * wkW + (int) (wkW * 0.6f),
                                         keyboardArea.getY(),
                                         (int) (wkW * 0.7f),
                                         (int) (keyboardArea.getHeight() * 0.6f));
                if (bb.toFloat().contains (pos)) {
                    applyEdit ("Set bass note", [this, s, n] {
                        auto c = pattern.getBass (s); c.note = n; c.on = true;
                        pattern.setBass (s, c);
                    });
                    return;
                }
            }
            for (int i = 0; i < 7; ++i)
            {
                juce::Rectangle<int> bb (keyboardArea.getX() + i * wkW,
                                         keyboardArea.getY(), wkW - 1, keyboardArea.getHeight());
                if (bb.toFloat().contains (pos)) {
                    const int n = whiteNotes[i];
                    applyEdit ("Set bass note", [this, s, n] {
                        auto c = pattern.getBass (s); c.note = n; c.on = true;
                        pattern.setBass (s, c);
                    });
                    return;
                }
            }
        }
        for (int o = 1; o <= 3; ++o)
        {
            if (octBtnBounds (o).toFloat().contains (pos)) {
                applyEdit ("Set bass octave", [this, s, o] {
                    auto c = pattern.getBass (s); c.octave = o;
                    pattern.setBass (s, c);
                });
                return;
            }
        }
        if (accBtnBounds.toFloat().contains (pos)) {
            applyEdit ("Toggle accent", [this, s] {
                auto c = pattern.getBass (s); c.accent = ! c.accent;
                pattern.setBass (s, c);
            });
            return;
        }
        if (sldBtnBounds.toFloat().contains (pos)) {
            applyEdit ("Toggle slide", [this, s] {
                auto c = pattern.getBass (s); c.slide = ! c.slide;
                pattern.setBass (s, c);
            });
            return;
        }
    }

    // パターン編集を外側 (Editor) に委譲。コールバックが無ければ即実行。
    void applyEdit (const juce::String& name, std::function<void()> edit)
    {
        if (onEdit) onEdit (name, std::move (edit));
        else        edit();
        repaint();
    }

    PatternState& pattern;
    GetStepFn     getCurrentStep;
    EditCallback  onEdit;

    juce::Rectangle<int> ledRow, markerRow;
    juce::Rectangle<int> drumHeaderRow, bassHeaderRow;
    juce::Rectangle<int> drumRows[PatternState::NumDrumRows];
    juce::Rectangle<int> bassRow;
    juce::Rectangle<int> bassEditorArea;

    juce::Rectangle<int> onOffBtnBounds;
    juce::Rectangle<int> keyboardArea;
    juce::Rectangle<int> octBtnArea;
    juce::Rectangle<int> accBtnBounds, sldBtnBounds;
    juce::Rectangle<int> prevBtnBounds, nextBtnBounds;

    int cellsLeft = 70, cellsRight = 800;
    int selectedBassStep = 0;
    int lastDrawnStep = -2;

    ViewMode viewMode = ViewMode::Both;

    // 右余白 (SEQ/MIDI トグルボタン 1 個分)
    static constexpr int kRightMargin = 70;
};
