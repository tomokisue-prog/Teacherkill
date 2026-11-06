#pragma once
#include <string>
class PlayerCamera;
class GameContext;

struct GameSettings {
    float bgmVolume = 0.35f;
    float seVolume = 0.75f;
    float ambienceVolume = 0.3f;
    float mouseSensitivity = 1.0f;
    // 1.0は従来の見え方。画面補正は各PCの設定として保存する。
    float brightness = 1.0f;
    float greenStrength = 1.0f;
    static constexpr float MinBrightness = 0.5f;
    static constexpr float MaxBrightness = 2.0f;

    bool Load(const std::string& path = "Data/Save/settings.json");
    bool Save(const std::string& path = "Data/Save/settings.json") const;
    void Apply(PlayerCamera& camera) const;
    void Apply(GameContext& context) const;
};
