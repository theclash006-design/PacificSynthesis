#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PacificColors.h"

// ─────────────────────────────────────────────────────────────
//  PacificLookAndFeel
//  元HTMLのSVGノブを再現する。270度のアーク、ラジアルグラデーション、
//  ポインタ線、cream/navy配色。各 Slider に
//      slider.setComponentID("knob:teal")
//  などでカラータグを付けるとそのカラーで描画する。
// ─────────────────────────────────────────────────────────────
class PacificLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PacificLookAndFeel()
    {
        // 全体配色
        setColour (juce::Slider::textBoxTextColourId,      PacificColors::text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId,   juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId,              PacificColors::textDim);
        setColour (juce::ComboBox::backgroundColourId,     juce::Colours::white);
        setColour (juce::ComboBox::textColourId,           PacificColors::text);
        setColour (juce::ComboBox::outlineColourId,        PacificColors::panelDark);
        setColour (juce::ComboBox::arrowColourId,          PacificColors::navy);
        setColour (juce::PopupMenu::backgroundColourId,    PacificColors::cream);
        setColour (juce::PopupMenu::textColourId,          PacificColors::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, PacificColors::navy);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId,       PacificColors::cream);
        setColour (juce::TextButton::textColourOffId,      PacificColors::text);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions ("Courier New", 10.5f, juce::Font::bold));
    }

    // ノブの数値テキスト等の Label をボーダー無しで描画
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll (label.findColour (juce::Label::backgroundColourId));

        if (! label.isBeingEdited())
        {
            const float alpha = label.isEnabled() ? 1.0f : 0.5f;
            const auto font = getLabelFont (label);
            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
            g.setFont (font);
            const auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
            g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                              juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                              label.getMinimumHorizontalScale());
        }
        // 外側の枠線は描かない (元: g.drawRect(label.getLocalBounds()))
    }

    // Slider の value テキストのフォントを少し大きく
    juce::Font getSliderPopupFont (juce::Slider&) override
    {
        return juce::Font (juce::FontOptions ("Courier New", 11.0f, juce::Font::bold));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::Font (juce::FontOptions ("Courier New", 10.0f, juce::Font::bold));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions ("Courier New", 10.5f, juce::Font::bold));
    }

    // 元HTMLの "Knob" コンポーネントを移植
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float /*rotaryStartAngle*/, float /*rotaryEndAngle*/,
                           juce::Slider& slider) override
    {
        // カラータグ
        const auto color = readColorTag (slider);

        const float diameter = (float) juce::jmin (width, height) - 6.0f;
        const float radius   = diameter * 0.5f;
        const float cx = (float) x + width  * 0.5f;
        const float cy = (float) y + height * 0.5f;

        // 270度のアーク (-135 度 から +135 度)
        const float startAngle = juce::degreesToRadians (-135.0f);
        const float endAngle   = juce::degreesToRadians (135.0f);
        const float valueAngle = startAngle + sliderPosProportional * (endAngle - startAngle);

        // 1) 背景アーク (panelDark)
        {
            juce::Path bg;
            bg.addCentredArc (cx, cy, radius * 0.85f, radius * 0.85f,
                              0.0f, startAngle, endAngle, true);
            g.setColour (PacificColors::panelDark);
            g.strokePath (bg, juce::PathStrokeType (2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));
        }

        // 2) 値アーク (パラメータカラー)
        {
            juce::Path fg;
            fg.addCentredArc (cx, cy, radius * 0.85f, radius * 0.85f,
                              0.0f, startAngle, valueAngle, true);
            g.setColour (color);
            g.strokePath (fg, juce::PathStrokeType (2.5f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));
        }

        // 3) ノブ本体 (ラジアルグラデーション)
        const float bodyR = radius * 0.62f;
        {
            const auto top    = color.brighter (0.35f);
            const auto bottom = color.darker   (0.5f);
            juce::ColourGradient grad (top, cx - bodyR * 0.4f, cy - bodyR * 0.6f,
                                       bottom, cx + bodyR * 0.4f, cy + bodyR * 0.6f, true);
            grad.addColour (0.55, color);
            g.setGradientFill (grad);
            g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

            // 縁取り
            g.setColour (juce::Colour::fromFloatRGBA (0, 0, 0, 0.18f));
            g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);
        }

        // 4) ハイライト (左上に楕円)
        {
            const float hx = cx - bodyR * 0.20f;
            const float hy = cy - bodyR * 0.35f;
            const float hw = bodyR * 0.55f;
            const float hh = bodyR * 0.30f;
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 1.0f, 1.0f, 0.18f));
            g.fillEllipse (hx - hw * 0.5f, hy - hh * 0.5f, hw, hh);
        }

        // 5) ポインタ線
        {
            const float pointerLen = bodyR * 0.85f;
            const float pointerInner = bodyR * 0.50f;
            const float cosA = std::cos (valueAngle - juce::MathConstants<float>::halfPi);
            const float sinA = std::sin (valueAngle - juce::MathConstants<float>::halfPi);
            const float x1 = cx + cosA * pointerInner;
            const float y1 = cy + sinA * pointerInner;
            const float x2 = cx + cosA * pointerLen;
            const float y2 = cy + sinA * pointerLen;
            g.setColour (juce::Colours::white.withAlpha (0.88f));
            g.drawLine (x1, y1, x2, y2, 1.8f);
        }
    }

    // TextButton (sequencer の各種ボタン) を Buchla 風に
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& /*bgColour*/,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const auto id = button.getComponentID();
        const auto isOn = button.getToggleState();

        // Tab button: トグル選択でnavy、未選択はcream
        if (id == "btn:tab")
        {
            juce::Colour fill = isOn ? PacificColors::navy : PacificColors::cream;
            juce::Colour border = isOn ? PacificColors::navy : PacificColors::panelDark;
            if (shouldDrawButtonAsHighlighted && ! isOn) fill = PacificColors::creamLight;
            if (shouldDrawButtonAsDown) fill = fill.darker (0.08f);
            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 3.0f);
            g.setColour (border);
            g.drawRoundedRectangle (bounds, 3.0f, 1.4f);
            return;
        }

        // SRC button (3-cycle): SEQ → MIDI → OFF
        //   SEQ  → cream + teal ボーダー + teal テキスト
        //   MIDI → cream + 青 (Snare) ボーダー + 青 テキスト
        //   OFF  → cream + 赤 (Kick) ボーダー + 赤 テキスト
        if (id == "btn:src")
        {
            const auto text = button.getButtonText();
            const bool isOff  = (text == "OFF");
            const bool isMidi = (text == "MIDI");

            juce::Colour fill = PacificColors::cream;
            juce::Colour border;
            if      (isMidi) border = PacificColors::blue;    // Snare の青
            else if (isOff)  border = PacificColors::red;     // Kick の赤
            else             border = PacificColors::teal;    // SEQ
            if (shouldDrawButtonAsHighlighted) fill = PacificColors::creamLight;
            if (shouldDrawButtonAsDown)        fill = fill.darker (0.08f);
            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 3.0f);
            g.setColour (border);
            g.drawRoundedRectangle (bounds, 3.0f, 1.4f);
            return;
        }

        // WAVE button: 白背景にテキスト
        if (id == "btn:wave")
        {
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (bounds, 2.0f);
            g.setColour (PacificColors::panelDark);
            g.drawRoundedRectangle (bounds, 2.0f, 1.0f);
            if (shouldDrawButtonAsDown) {
                g.setColour (juce::Colours::black.withAlpha (0.08f));
                g.fillRoundedRectangle (bounds, 2.0f);
            }
            return;
        }

        // Play button: 常にカラー塗り (停止=navy / 再生=red のグラデーション)
        if (id == "btn:play")
        {
            const auto color = isOn ? PacificColors::red : PacificColors::navy;
            juce::ColourGradient grad (color.brighter (0.2f), 0, bounds.getY(),
                                       color.darker   (0.4f), 0, bounds.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (color.darker (0.5f));
            g.drawRoundedRectangle (bounds, 4.0f, 1.2f);
            if (shouldDrawButtonAsDown) {
                g.setColour (juce::Colours::black.withAlpha (0.15f));
                g.fillRoundedRectangle (bounds, 4.0f);
            }
            return;
        }

        const auto accent = readColorTag (button);
        juce::Colour fill = PacificColors::cream;
        juce::Colour border = PacificColors::panelDark;

        if (isOn) {
            fill = accent;
            border = accent.darker (0.3f);
        }
        if (shouldDrawButtonAsHighlighted) fill = fill.brighter (0.05f);
        if (shouldDrawButtonAsDown)        fill = fill.darker (0.08f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (border);
        g.drawRoundedRectangle (bounds, 2.0f, 1.4f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool /*shouldDrawButtonAsHighlighted*/,
                         bool /*shouldDrawButtonAsDown*/) override
    {
        const auto id = button.getComponentID();
        const bool isPlay = (id == "btn:play");
        const bool isTab  = (id == "btn:tab");
        const bool isWave = (id == "btn:wave");
        const bool isSrc  = (id == "btn:src");
        const bool isOn = button.getToggleState();

        // テキスト色
        juce::Colour textCol;
        if (isPlay)        textCol = juce::Colours::white;
        else if (isTab)    textCol = isOn ? juce::Colours::white : PacificColors::textDim;
        else if (isSrc)
        {
            const auto t = button.getButtonText();
            if      (t == "MIDI") textCol = PacificColors::blue;    // Snare の青
            else if (t == "OFF")  textCol = PacificColors::red;     // Kick の赤
            else                  textCol = PacificColors::teal;    // SEQ
        }
        else if (isWave)   textCol = PacificColors::text;
        else if (isOn)     textCol = juce::Colours::white;
        else               textCol = PacificColors::text;
        g.setColour (textCol);

        // フォントサイズ
        float fontSize = 10.0f;
        if (isPlay) fontSize = 18.0f;
        else if (isTab) fontSize = 9.5f;
        else if (isSrc) fontSize = 9.5f;
        else if (isWave) fontSize = 11.0f;

        g.setFont (juce::Font (juce::FontOptions ("Courier New", fontSize, juce::Font::bold)));
        g.drawText (button.getButtonText(), button.getLocalBounds(),
                    juce::Justification::centred, false);
    }

private:
    // setComponentID("knob:red") のようなタグから色を読む
    static juce::Colour readColorTag (const juce::Component& c)
    {
        const auto id = c.getComponentID();
        if (id.startsWith ("knob:") || id.startsWith ("btn:"))
        {
            const auto tag = id.fromFirstOccurrenceOf (":", false, false);
            if (tag == "red")    return PacificColors::red;
            if (tag == "navy")   return PacificColors::navy;
            if (tag == "blue")   return PacificColors::blue;
            if (tag == "teal")   return PacificColors::teal;
            if (tag == "orange") return PacificColors::orange;
        }
        return PacificColors::navy;
    }
};
