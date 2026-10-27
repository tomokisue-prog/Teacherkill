#pragma once
#include <string>
struct GameSettings;

enum class PauseAction { None, Resume, SaveCheckpoint, LoadCheckpoint, Title, SettingsChanged, SaveSettings, Debug };

class PauseMenu {
public:
    void Open(bool debugEnabled = false);
    PauseAction Update(float deltaTime, GameSettings& settings, bool canLoad, bool canSave);
    void Draw(const GameSettings& settings, bool canLoad, bool canSave) const;
    void SetMessage(const std::string& japanese, const std::string& english);
private:
    bool settingsPage_ = false;
    bool debugEnabled_ = false;
    int selectedOption_ = 0;
    int draggingSlider_ = -1;
    float elapsed_ = 0;
    std::string message_;
    std::string fallbackMessage_;
};
