#pragma once
#include <string>
class PlayerCamera;

struct GameSettings {
    float bgmVolume = 0.35f;
    float seVolume = 0.75f;
    float ambienceVolume = 0.3f;
    float mouseSensitivity = 1.0f;

    bool Load(const std::string& path = "Data/Save/settings.json");
    bool Save(const std::string& path = "Data/Save/settings.json") const;
    void Apply(PlayerCamera& camera) const;
};
