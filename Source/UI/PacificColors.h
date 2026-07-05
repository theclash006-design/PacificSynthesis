#pragma once
#include <juce_graphics/juce_graphics.h>

// 元 HTML の Buchla パレット
namespace PacificColors
{
    inline const juce::Colour cream        { 0xFFF0E8D8 };
    inline const juce::Colour creamLight   { 0xFFF7F2E8 };
    inline const juce::Colour creamDark    { 0xFFD8CEB8 };
    inline const juce::Colour panel        { 0xFFE8DCC8 };
    inline const juce::Colour panelDark    { 0xFFD4C8AC };
    inline const juce::Colour navy         { 0xFF2A3A6A };
    inline const juce::Colour navyLight    { 0xFF3A4E8A };
    inline const juce::Colour navyDark     { 0xFF1C2848 };
    inline const juce::Colour red          { 0xFFC23030 };
    inline const juce::Colour redLight     { 0xFFD84040 };
    inline const juce::Colour redDark      { 0xFF8A1818 };
    inline const juce::Colour blue         { 0xFF3868B8 };
    inline const juce::Colour teal         { 0xFF2A7878 };
    inline const juce::Colour orange       { 0xFFD07028 };
    inline const juce::Colour aluminum     { 0xFFB8B0A0 };
    inline const juce::Colour aluminumLight{ 0xFFCCC4B4 };
    inline const juce::Colour text         { 0xFF2A2820 };
    inline const juce::Colour textDim      { 0xFF8A8070 };
    inline const juce::Colour textLight    { 0xFFA09888 };

    // ドラムトラックの色 (元HTML: KICK=red, SNARE=blue, HIHAT=orange, CLAP=teal)
    inline const juce::Colour drumKick    = red;
    inline const juce::Colour drumSnare   = blue;
    inline const juce::Colour drumHiHat   = orange;
    inline const juce::Colour drumClap    = teal;
}
