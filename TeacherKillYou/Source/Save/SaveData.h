#pragma once
#include "nlohmann/json_fwd.hpp"
#include <string>

// Player saves are separate from the editor's scene.json.
namespace SaveData {
    bool Read(const std::string& path, nlohmann::json& data);
    bool Write(const std::string& path, const nlohmann::json& data);
}
