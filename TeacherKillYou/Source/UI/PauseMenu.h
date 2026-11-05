#pragma once
#include <string>
#include "raylib.h"
struct GameSettings;

enum class PauseAction { None, Resume, SaveCheckpoint, LoadCheckpoint, Title, SettingsChanged, SaveSettings, Debug };

class PauseMenu {
public:
    void Open(bool debugEnabled = false);
    void Init();
    void End();
    PauseAction Update(float deltaTime, GameSettings& settings, bool canLoad, bool canSave);
    void Draw(const GameSettings& settings, bool canLoad, bool canSave) const;
    void SetMessage(const std::string& japanese, const std::string& english);
private:
    Texture2D background_{};
    bool settingsPage_ = false;
    bool debugEnabled_ = false;
    int selectedOption_ = 0;
    int draggingSlider_ = -1;
    float elapsed_ = 0;
    std::string message_;
    std::string fallbackMessage_;
};
