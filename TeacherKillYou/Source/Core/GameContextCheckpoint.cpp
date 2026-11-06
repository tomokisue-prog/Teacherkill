#include "GameContext.h"
#include "AudioManager.h"
#include "WorldScale.h"

CheckpointData GameContext::CaptureCheckpoint() const
{
    CheckpointData data;
    data.player = player_.CaptureCheckpoint();
    data.inventory = inventory_.CaptureCheckpoint();
    // 複数ドアのうち最初のドア、またはチェックポイント用の処理（必要に応じて対応）
    if (!doors_.empty())
    {
        data.door = doors_[0].CaptureCheckpoint();
    }
    for (const auto& enemy : enemyManager_.GetEnemies())
        if (enemy.IsAlive()) data.enemies.push_back(enemy.CaptureCheckpoint());
    for (const auto& tp : teleportPoints_)
        data.teleports.push_back({ {tp.GetPosition(), tp.GetRotation(), tp.GetScale()}, tp.GetPointNumber(), int(tp.GetTeleportType()), tp.GetRadius() });
    data.stagePosition = stage_.GetPosition();
    data.stageScale = stage_.GetScale();
    data.goal = { goalPoint_.GetPosition(), goalPoint_.GetRotation(), goalPoint_.GetScale() };
    data.goalRadius = goalPoint_.GetRadius();
    const auto& camera = cameraController_.GetPlayerCamera();
    data.yaw = camera.GetYaw();
    data.pitch = camera.GetPitch();
    data.flashlight = lighting_.IsFlashlightOn();
    return data;
}

void GameContext::RestoreCheckpoint(const CheckpointData& data)
{
#ifndef NDEBUG
    debugGameplay_.ClearShots();
#endif
    // Loaded models and HUD textures stay alive. Transient attacks and visual effects do not.
    AM().StopAllSe();
    gameFade_.Reset();
    messageWindow_.Close();
    bloodEffect_.Reset();
    bulletMarks_.clear();
    muzzleFlashTimer_ = hitMarkerTimer_ = tracerAge_ = 0;
    hitMarkerKill_ = tracerActive_ = false;
    checkpointRequested_ = false;
    goalRequested_ = false;
    stage_.SetPosition(data.stagePosition);
    stage_.SetScale(data.stageScale);
    player_.RestoreCheckpoint(data.player);
    inventory_.RestoreCheckpoint(data.inventory);
    inventory_.ApplyPlacements();
    if (!doors_.empty())
    {
        doors_[0].RestoreCheckpoint(data.door);
    }
    enemyManager_.Clear();
    for (const auto& saved : data.enemies) {
        const auto id = enemyManager_.Spawn(saved.position, saved.config, saved.yaw);
        enemyManager_.Find(id)->RestoreCheckpoint(saved);
    }
    teleportPoints_.clear();
    for (const auto& saved : data.teleports) {
        TeleportPoint tp;
        tp.SetPosition(saved.transform.position);
        tp.SetRotation(saved.transform.rotation);
        tp.SetScale(saved.transform.scale);
        tp.SetPointNumber(saved.number);
        tp.SetTeleportType(static_cast<TeleportType>(saved.type));
        tp.SetRadius(saved.radius);
        teleportPoints_.push_back(tp);
    }
    goalPoint_.SetPosition(data.goal.position);
    goalPoint_.SetRotation(data.goal.rotation);
    goalPoint_.SetScale(data.goal.scale);
    goalPoint_.SetRadius(data.goalRadius);
    lighting_.SetFlashlightOn(data.flashlight);
    auto& camera = cameraController_.GetPlayerCamera();
    camera.SetEyeHeight(player_.GetHeight() * WorldScale::EyeHeightRatio);
    camera.SetPlayerPosition(player_.GetPosition());
    camera.SetViewAngles(data.yaw, data.pitch);
    cameraController_.SetActiveCamera(CameraType::Player);
    horrorAudio_.Reset(player_.GetPosition());
}

bool GameContext::CanSaveCheckpoint() const
{
    return player_.IsAlive() && !gameFade_.IsFading() && !IsMessageActive()
        && cameraController_.GetActiveType() == CameraType::Player;
}

bool GameContext::ConsumeCheckpointRequest()
{
    const bool requested = checkpointRequested_ && CanSaveCheckpoint();
    if (requested) checkpointRequested_ = false;
    return requested;
}
