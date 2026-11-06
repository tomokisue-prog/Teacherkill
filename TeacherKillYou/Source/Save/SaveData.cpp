#include "SaveData.h"
#include "EnemyConfig.h"
#include <cmath>
#include <stdexcept>
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

float SaveData::Number(const nlohmann::json& data, float low, float high) {
    if (!data.is_number()) throw std::runtime_error("expected a number");
    const float value = data.get<float>();
    if (!std::isfinite(value) || value < low || value > high) throw std::runtime_error("number out of range");
    return value;
}
int SaveData::Integer(const nlohmann::json& data, int low, int high) {
    if (!data.is_number_integer()) throw std::runtime_error("expected an integer");
    const auto value = data.get<std::int64_t>();
    if (value < low || value > high) throw std::runtime_error("integer out of range");
    return static_cast<int>(value);
}
nlohmann::json SaveData::PackEnemyConfig(const demo::EnemyConfig& c) {
    return {{"type", int(c.type)}, {"maxHp", c.maxHp}, {"moveSpeed", c.moveSpeed}, {"turnSpeed", c.turnSpeed},
        {"bodyRadius", c.bodyRadius}, {"bodyHeight", c.bodyHeight}, {"detectionRange", c.detectionRange},
        {"loseRange", c.loseRange}, {"stopDistance", c.stopDistance}, {"resumeDistance", c.resumeDistance},
        {"corpseDuration", c.corpseDuration}, {"attackDamage", c.attackDamage}, {"attackRange", c.attackRange},
        {"attackHitFraction", c.attackHitFraction}, {"attackRecovery", c.attackRecovery}, {"animationSpeed", c.animationSpeed}};
}
demo::EnemyConfig SaveData::ParseEnemyConfig(const nlohmann::json& j) {
    demo::EnemyConfig c;
    c.type = static_cast<demo::EnemyType>(Integer(j.at("type"), int(demo::EnemyType::FemaleZombie), int(demo::EnemyType::MonsterMale2)));
    c.maxHp = Integer(j.at("maxHp"), 1, 100000);
    c.moveSpeed = Number(j.at("moveSpeed"), 0, 100);
    c.turnSpeed = Number(j.at("turnSpeed"), 0, 100);
    c.bodyRadius = Number(j.at("bodyRadius"), .01f, 100);
    c.bodyHeight = Number(j.at("bodyHeight"), .01f, 100);
    c.detectionRange = Number(j.at("detectionRange"), 0, 10000);
    c.loseRange = Number(j.at("loseRange"), 0, 10000);
    c.stopDistance = Number(j.at("stopDistance"), 0, 100);
    c.resumeDistance = Number(j.at("resumeDistance"), 0, 100);
    c.corpseDuration = Number(j.at("corpseDuration"), 0, 10000);
    c.attackDamage = Integer(j.at("attackDamage"), 0, 100000);
    c.attackRange = Number(j.at("attackRange"), 0, 100);
    c.attackHitFraction = Number(j.at("attackHitFraction"), 0, 1);
    c.attackRecovery = Number(j.at("attackRecovery"), 0, 10000);
    c.animationSpeed = Number(j.at("animationSpeed"), .01f, 100);
    c.Validate();
    return c;
}
