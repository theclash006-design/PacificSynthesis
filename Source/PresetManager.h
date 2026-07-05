#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>

// ─────────────────────────────────────────────────────────────
//  PresetManager
//  ~/Documents/Pacific Synthesis/Presets/ 以下に .preset ファイルを
//  保存・読込する。
//  - getPresetFolder()   : 保存先ディレクトリ (なければ自動作成)
//  - listPresets()       : .preset ファイルの一覧 (名前のみ、アルファベット順)
//  - savePreset(name)    : 現在のプラグイン状態を name.preset として書き出し
//  - loadPreset(name)    : ファイルを読み込んでプラグインに適用
//  - deletePreset(name)  : ファイル削除
//  - loadPrev / loadNext : リストの前後に移動
// ─────────────────────────────────────────────────────────────
class PresetManager
{
public:
    static constexpr const char* PresetExtension = ".preset";

    explicit PresetManager (juce::AudioProcessor& procRef)
        : proc (procRef)
    {
        ensurePresetFolderExists();
    }

    juce::File getPresetFolder() const
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Pacific Synthesis")
                 .getChildFile ("Presets");
    }

    void ensurePresetFolderExists() const
    {
        const auto folder = getPresetFolder();
        if (! folder.exists())
            folder.createDirectory();
    }

    // 拡張子を取り除いたプリセット名のリスト
    juce::StringArray listPresets() const
    {
        juce::StringArray result;
        ensurePresetFolderExists();
        for (const auto& f : getPresetFolder().findChildFiles (
                 juce::File::findFiles, false,
                 juce::String ("*") + PresetExtension))
        {
            result.add (f.getFileNameWithoutExtension());
        }
        result.sortNatural();
        return result;
    }

    bool savePreset (const juce::String& name)
    {
        if (name.isEmpty()) return false;
        ensurePresetFolderExists();
        const auto file = getPresetFile (name);

        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        if (file.replaceWithData (mb.getData(), mb.getSize()))
        {
            currentName = name;
            return true;
        }
        return false;
    }

    bool loadPreset (const juce::String& name)
    {
        const auto file = getPresetFile (name);
        if (! file.existsAsFile()) return false;

        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb)) return false;
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        currentName = name;
        return true;
    }

    bool deletePreset (const juce::String& name)
    {
        const auto file = getPresetFile (name);
        if (! file.existsAsFile()) return false;
        if (file.deleteFile())
        {
            if (currentName == name) currentName.clear();
            return true;
        }
        return false;
    }

    bool loadPrev()  { return stepBy (-1); }
    bool loadNext()  { return stepBy ( 1); }

    juce::String getCurrentName() const { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n; }
    void clearCurrentName() { currentName.clear(); }

    // Finder で開く
    void revealFolder() const
    {
        ensurePresetFolderExists();
        getPresetFolder().revealToUser();
    }

private:
    juce::File getPresetFile (const juce::String& name) const
    {
        return getPresetFolder().getChildFile (name + PresetExtension);
    }

    bool stepBy (int delta)
    {
        const auto list = listPresets();
        if (list.isEmpty()) return false;
        const int idx = list.indexOf (currentName);
        int next = (idx < 0) ? (delta > 0 ? 0 : list.size() - 1)
                             : (idx + delta + list.size()) % list.size();
        return loadPreset (list[next]);
    }

    juce::AudioProcessor& proc;
    juce::String          currentName;
};
