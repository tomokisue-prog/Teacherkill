#include "GameScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "SaveSystem.h"
#include "CheckpointSave.h"
#include "ResourceManager.h"
#include <algorithm>

GameScene::GameScene(GameContext* context) : Scene(context) {}
bool GameScene::IsGameOver() const { return gameOver_; }
bool GameScene::IsPaused() const { return paused_; }
void GameScene::RequestCheckpointContinue() { loadCheckpointOnInit_ = true; }

void GameScene::Init()
{
    finished = false;
    nextScene = nullptr;
    gameOver_ = paused_ = settingsDirty_ = false;
    resumeInputDelay_ = noticeTime_ = 0;
    gameOverScreen_.Reset();
    if (!gameContext) return;

    gameContext->Init();
    SaveSystem::LoadScene(*gameContext, "scene.json");
    auto& controller = gameContext->GetCameraController();
    auto& camera = controller.GetPlayerCamera();
    camera.SetPlayerPosition(gameContext->GetPlayer().GetPosition());
    camera.Reset();
    controller.SetActiveCamera(CameraType::Player);
    settings_ = GameSettings{};
    settings_.Load();
    settings_.Apply(*gameContext);
    CheckpointData checkpoint;
    checkpointAvailable_ = CheckpointSave::Load(checkpoint);
    const bool continueRequested = loadCheckpointOnInit_;
    loadCheckpointOnInit_ = false;
    if (continueRequested) {
        if (checkpointAvailable_) gameContext->RestoreCheckpoint(checkpoint);
        else ShowNotice(u8"保存を読み込めなかったため、最初から開始します。", "Cannot load checkpoint. Starting a new game.");
    }
    AM().SetPaused(false);
    AM().PlayBgm(BgmId::Game);
    AM().PlayAmbience();
    DisableCursor();
}

void GameScene::BeginGameOver()
{
    gameOver_ = true;
    paused_ = false;
    noticeTime_ = 0;
    gameOverScreen_.Reset();
    CheckpointData checkpoint;
    checkpointAvailable_ = CheckpointSave::Load(checkpoint);
    gameOverScreen_.SetCheckpointAvailable(checkpointAvailable_);
    gameContext->GetInventory().CancelHealing();
    AM().StopBgm();
    AM().StopAmbience();
    AM().StopAllSe();
    AM().SetPaused(false);
    EnableCursor();
}

void GameScene::OpenPause()
{
    paused_ = true;
    pauseMenu_.Open();
    CheckpointData checkpoint;
    checkpointAvailable_ = CheckpointSave::Load(checkpoint);
    AM().SetPaused(true);
    EnableCursor();
}

bool GameScene::SaveSettings()
{
    if (!settingsDirty_) return true;
    if (!settings_.Save()) return false;
    settingsDirty_ = false;
    return true;
}

void GameScene::Resume()
{
    if (!SaveSettings()) ShowNotice(u8"設定を保存できませんでした。現在のプレイには反映されています。", "Settings applied, but could not be saved.");
    paused_ = false;
    AM().SetPaused(false);
    // Re-capturing the cursor can generate a mouse delta. Let one input boundary pass.
    resumeInputDelay_ = .1f;
    if (gameContext->GetCameraController().GetActiveType() == CameraType::Player) DisableCursor();
    else EnableCursor();
}

void GameScene::ShowNotice(const std::string& japanese, const std::string& english)
{
    notice_ = japanese;
    fallbackNotice_ = english;
    noticeTime_ = 3;
}

bool GameScene::SaveCheckpoint()
{
    if (!gameContext->CanSaveCheckpoint()) return false;
    const bool saved = CheckpointSave::Save(gameContext->CaptureCheckpoint());
    if (saved) checkpointAvailable_ = true;
    return saved;
}

bool GameScene::LoadCheckpoint()
{
    CheckpointData checkpoint;
    if (!CheckpointSave::Load(checkpoint)) {
        checkpointAvailable_ = false;
        gameOverScreen_.SetCheckpointAvailable(false);
        ShowNotice(u8"チェックポイントを読み込めませんでした。", "Cannot load checkpoint.");
        return false;
    }
    gameContext->RestoreCheckpoint(checkpoint);
    gameOver_ = false;
    checkpointAvailable_ = true;
    Resume();
    AM().PlayBgm(BgmId::Game);
    AM().PlayAmbience();
    ShowNotice(u8"チェックポイントから再開しました。", "Checkpoint loaded.");
    return true;
}

void GameScene::Update(float deltaTime)
{
    if (!gameContext) return;
    if (noticeTime_ > 0) noticeTime_ = (std::max)(0.0f, noticeTime_ - deltaTime);
    if (gameOver_) {
        switch (gameOverScreen_.Update(deltaTime)) {
        case GameOverAction::Retry: SetNextScene(SM().GetScene(SceneID::Game)); break;
        case GameOverAction::Title: SetNextScene(SM().GetScene(SceneID::Title)); break;
        case GameOverAction::Checkpoint: LoadCheckpoint(); break;
        case GameOverAction::None: break;
        }
        return;
    }
    if (!gameContext->GetPlayer().IsAlive()) { BeginGameOver(); return; }
    if (paused_) {
        switch (pauseMenu_.Update(deltaTime, settings_, checkpointAvailable_, gameContext->CanSaveCheckpoint())) {
        case PauseAction::Resume: Resume(); break;
        case PauseAction::Title: SetNextScene(SM().GetScene(SceneID::Title)); break;
        case PauseAction::SettingsChanged:
            settings_.Apply(*gameContext);
            settingsDirty_ = true;
            break;
        case PauseAction::SaveSettings:
            if (SaveSettings()) pauseMenu_.SetMessage(u8"設定を保存しました。", "Settings saved.");
            else pauseMenu_.SetMessage(u8"設定を保存できませんでした。", "Cannot save settings.");
            break;
        case PauseAction::SaveCheckpoint:
            if (SaveCheckpoint()) pauseMenu_.SetMessage(u8"チェックポイントを保存しました。", "Checkpoint saved.");
            else pauseMenu_.SetMessage(u8"保存できませんでした。", "Cannot save checkpoint.");
            break;
        case PauseAction::LoadCheckpoint:
            if (!LoadCheckpoint()) pauseMenu_.SetMessage(u8"保存データを読み込めませんでした。", "Cannot load checkpoint.");
            break;
        case PauseAction::None: break;
        }
        return; // World timers, healing, attacks and camera input stay frozen.
    }
    if (IsKeyPressed(KEY_ESCAPE) || (!IsWindowState(FLAG_WINDOW_HIDDEN) && !IsWindowFocused())) {
        OpenPause();
        return;
    }
    if (resumeInputDelay_ > 0) {
        resumeInputDelay_ = (std::max)(0.0f, resumeInputDelay_ - deltaTime);
        return;
    }
    gameContext->Update(deltaTime);
    if (!gameContext->GetPlayer().IsAlive()) { BeginGameOver(); return; }
    if (gameContext->ConsumeCheckpointRequest()) {
        if (SaveCheckpoint()) ShowNotice(u8"チェックポイントを自動保存しました。", "Checkpoint saved automatically.");
        else ShowNotice(u8"チェックポイントを保存できませんでした。", "Cannot save checkpoint.");
    }
    if (IsKeyPressed(KEY_B)) SetNextScene(SM().GetScene(SceneID::Title));
}

void GameScene::Render() const
{
    if (!gameContext) return;
    gameContext->Draw();
    if (gameOver_) gameOverScreen_.Draw();
    else if (paused_) pauseMenu_.Draw(settings_, checkpointAvailable_, gameContext->CanSaveCheckpoint());
    else DrawText("[Esc] PAUSE / SETTINGS", GetScreenWidth() - 275, 18, 18, {184, 196, 189, 220});
    if (noticeTime_ > 0 && !paused_) {
        const auto& font = JPFont();
        const float size = (std::max)(16.0f, 26.0f * GetScreenHeight() / 1080.0f);
        if (font.IsLoaded()) font.DrawText(notice_.c_str(), int((GetScreenWidth() - font.MeasureText(notice_.c_str(), size).x) / 2), int(GetScreenHeight() * .12f), size, {217, 231, 202, 255});
        else DrawText(fallbackNotice_.c_str(), (GetScreenWidth() - MeasureText(fallbackNotice_.c_str(), int(size))) / 2, int(GetScreenHeight() * .12f), int(size), RAYWHITE);
    }
}

void GameScene::End()
{
    SaveSettings();
    AM().StopBgm();
    AM().StopAmbience();
    AM().StopAllSe();
    AM().SetPaused(false);
    if (gameContext) gameContext->End();
}
