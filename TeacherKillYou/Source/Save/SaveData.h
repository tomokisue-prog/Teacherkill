#pragma once
#include "nlohmann/json_fwd.hpp"
#include <string>

namespace demo { struct EnemyConfig; }
namespace SaveData {
    bool Read(const std::string& path, nlohmann::json& data);
    bool Write(const std::string& path, const nlohmann::json& data);

    float Number(const nlohmann::json& data, float low, float high);
    int Integer(const nlohmann::json& data, int low, int high);
    nlohmann::json PackEnemyConfig(const demo::EnemyConfig& config);
    demo::EnemyConfig ParseEnemyConfig(const nlohmann::json& data);
}
