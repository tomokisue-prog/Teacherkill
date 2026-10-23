#include "SaveData.h"
#include "nlohmann/json.hpp"
#include "raylib.h"
#include <filesystem>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOUSER
#include <Windows.h>

bool SaveData::Read(const std::string& path, nlohmann::json& data)
{
    try {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error || size > 1024 * 1024) return false;
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;
        auto parsed = nlohmann::json::parse(file);
        if (!parsed.is_object()) return false;
        data = std::move(parsed);
        return true;
    }
    catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "SAVE: Cannot read %s: %s", path.c_str(), error.what());
        return false;
    }
}

bool SaveData::Write(const std::string& path, const nlohmann::json& data)
{
    try {
        const std::filesystem::path target(path);
        if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path());
        auto temporary = target;
        temporary += ".tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (!file) return false;
            file << data.dump(2);
            file.flush();
            if (!file) return false;
            file.close();
            if (file.fail()) return false;
        }
        // Windows rename cannot replace an existing file. Commit only a complete save.
        if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            TraceLog(LOG_WARNING, "SAVE: Cannot replace %s (error %lu)", path.c_str(), GetLastError());
            return false;
        }
        return true;
    }
    catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "SAVE: Cannot write %s: %s", path.c_str(), error.what());
        return false;
    }
}
