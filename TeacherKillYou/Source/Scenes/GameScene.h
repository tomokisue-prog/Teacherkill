#pragma once
#include "Scene.h"
#include "GameOverScreen.h"
#include "GameSettings.h"
#include "PauseMenu.h"
#include <string>

class GameScene final : public Scene
{
public:
    explicit GameScene(GameContext* context);
    void Init() override;
    void Update(float deltaTime) override;
    void Render() const override;
    void End() override;
    bool IsGameOver() const;
    bool IsPaused() const;
    void RequestCheckpointContinue();

private:
    void BeginGameOver();
    void OpenPause();
    void Resume();
    bool LoadCheckpoint();
    bool SaveCheckpoint();
    bool SaveSettings();
    void ShowNotice(const std::string& japanese, const std::string& english);

    bool gameOver_ = false;
    bool paused_ = false;
    bool checkpointAvailable_ = false;
    bool loadCheckpointOnInit_ = false;
    bool settingsDirty_ = false;
    float resumeInputDelay_ = 0;
    float noticeTime_ = 0;
    std::string notice_;
    std::string fallbackNotice_;
    GameOverScreen gameOverScreen_;
    PauseMenu pauseMenu_;
    GameSettings settings_;
};
