#pragma once
#include "DialogueEntry.h"
#include <string>
#include <unordered_map>

class DialogueManager
{
public:
    static DialogueManager& GetInstance()
    {
        static DialogueManager instance;
        return instance;
    }

    // JSONファイルを読み込んでメモリに保持する
    bool LoadFromJson(const std::string& filepath);

    // 指定したIDのセリフデータを取得する
    const DialogueEntry* GetDialogue(const std::string& id) const;

private:
    DialogueManager() = default;
    ~DialogueManager() = default;

    DialogueManager(const DialogueManager&) = delete;
    DialogueManager& operator=(const DialogueManager&) = delete;

    // IDをキーにしてセリフデータを保持するマップ
    std::unordered_map<std::string, DialogueEntry> dialogues_;
};

// ショートカット関数
inline DialogueManager& DM() { return DialogueManager::GetInstance(); }