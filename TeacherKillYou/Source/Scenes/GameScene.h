#pragma once
#include "Scene.h"
#include "GameOverScreen.h"
#include "GameSettings.h"
#include "PauseMenu.h"
#ifndef NDEBUG
#include "DebugMenu.h"
#endif
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
    void SetDebugToolsEnabled(bool enabled);
    bool IsDebugMenuOpen() const;
    void DrawDebugMenu();
    void RequestIntroFadeIn() { introFadeRequested_ = true; }

private:
    static constexpr double IntroFadeInSeconds = 0.65; // Adjust the game fade-in duration here.
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
    bool introFadeRequested_ = false;
    bool introFadeActive_ = false;
    bool introFadeClockStarted_ = false;
    double introFadeStart_ = 0.0;
    float introFadeAlpha_ = 0.0f;
    float resumeInputDelay_ = 0;
    float noticeTime_ = 0;
    std::string notice_;
    std::string fallbackNotice_;
    GameOverScreen gameOverScreen_;
    PauseMenu pauseMenu_;
    GameSettings settings_;
#ifndef NDEBUG
    bool debugToolsEnabled_ = false;
    DebugMenu debugMenu_;
#endif
};
