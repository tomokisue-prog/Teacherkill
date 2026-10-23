#include "GameSettings.h"
#include "AudioManager.h"
#include "PlayerCamera.h"
#include "SaveData.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>

namespace {
    float ReadSetting(const nlohmann::json& data, const char* key, float fallback, float low, float high) {
        if (!data.contains(key) || !data[key].is_number()) return fallback;
        const float value = data[key].get<float>();
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    }
}

bool GameSettings::Load(const std::string& path)
{
    nlohmann::json data;
    if (!SaveData::Read(path, data) || !data.contains("version")
        || !data["version"].is_number_integer() || data["version"] != 1) return false;
    bgmVolume = ReadSetting(data, "bgm", bgmVolume, 0, 1);
    seVolume = ReadSetting(data, "se", seVolume, 0, 1);
    ambienceVolume = ReadSetting(data, "ambience", ambienceVolume, 0, 1);
    mouseSensitivity = ReadSetting(data, "mouseSensitivity", mouseSensitivity, 0.1f, 3.0f);
    return true;
}

bool GameSettings::Save(const std::string& path) const
{
    return SaveData::Write(path, {{"version", 1}, {"bgm", bgmVolume}, {"se", seVolume},
        {"ambience", ambienceVolume}, {"mouseSensitivity", mouseSensitivity}});
}

void GameSettings::Apply(PlayerCamera& camera) const
{
    AM().SetBgmVolume(bgmVolume);
    AM().SetSeVolume(seVolume);
    AM().SetAmbienceVolume(ambienceVolume);
    camera.SetSensitivity(mouseSensitivity);
}
