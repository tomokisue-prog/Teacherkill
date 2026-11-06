#include "DialogueManager.h"
#include "json.hpp"
#include "raylib.h"
#include <fstream>

bool DialogueManager::LoadFromJson(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        TraceLog(LOG_WARNING, "DIALOGUE: Failed to open dialogue file: %s", filepath.c_str());
        return false;
    }

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const std::exception& e)
    {
        TraceLog(LOG_ERROR, "DIALOGUE: Failed to parse JSON: %s", e.what());
        return false;
    }

    dialogues_.clear();

    // JSONの各エントリを走査してマップに格納する
    for (auto it = root.begin(); it != root.end(); ++it)
    {
        std::string id = it.key();
        const auto& val = it.value();

        DialogueEntry entry;
        entry.speaker = val.value("speaker", "");
        entry.text = val.value("text", "");

        dialogues_[id] = entry;
    }

    TraceLog(LOG_INFO, "DIALOGUE: Loaded %d dialogues from %s", (int)dialogues_.size(), filepath.c_str());
    return true;
}

const DialogueEntry* DialogueManager::GetDialogue(const std::string& id) const
{
    auto it = dialogues_.find(id);
    if (it != dialogues_.end())
    {
        return &(it->second);
    }

    TraceLog(LOG_WARNING, "DIALOGUE: Dialogue ID not found: %s", id.c_str());
    return nullptr;
}