#pragma once
#include <string>
class PlayerCamera;
class GameContext;

struct GameSettings {
    float bgmVolume = 0.35f;
    float seVolume = 0.75f;
    float ambienceVolume = 0.3f;
    float mouseSensitivity = 1.0f;
    // 明るさの基準は1.0。画面補正はPCごとに保存
    float brightness = 1.0f;
    float greenStrength = 1.0f;
    static constexpr float MinBrightness = 0.5f;
    static constexpr float MaxBrightness = 2.0f;

    bool Load(const std::string& path = "Data/Save/settings.json");
    bool Save(const std::string& path = "Data/Save/settings.json") const;
    void Apply(PlayerCamera& camera) const;
    void Apply(GameContext& context) const;
};
