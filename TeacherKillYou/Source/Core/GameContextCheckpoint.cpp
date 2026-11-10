#include "GameContext.h"
#include "AudioManager.h"
#include "WorldScale.h"
#include <algorithm>

CheckpointData GameContext::CaptureCheckpoint() const
{
    CheckpointData data;
    data.layoutRevision = CheckpointData::CurrentLayoutRevision;
    data.player = player_.CaptureCheckpoint();
    data.inventory = inventory_.CaptureCheckpoint();
    data.guidance = guidancePins_;
    // 複数ドアのうち最初のドア、またはチェックポイント用の処理（必要に応じて対応）
    for (const auto& door : doors_) data.doors.push_back(door.CaptureCheckpoint());
    if (!data.doors.empty()) data.door = data.doors.front();
    const auto& placements = enemyManager_.GetSpawnPoints();
    for (const auto& enemy : enemyManager_.GetEnemies()) {
        if (!enemy.IsAlive()) continue;
        auto saved = enemy.CaptureCheckpoint();
        const auto placement = std::find_if(placements.begin(), placements.end(), [&](const demo::EnemySpawnData& entry) {
            return entry.spawnedEnemyId == enemy.GetId();
        });
        if (placement != placements.end()) saved.spawnPointIndex = int(placement - placements.begin());
        data.enemies.push_back(saved);
    }
    for (const auto& tp : teleportPoints_)
        data.teleports.push_back({ {tp.GetPosition(), tp.GetRotation(), tp.GetScale()}, tp.GetPointNumber(), int(tp.GetTeleportType()), tp.GetRadius() });
    data.stagePosition = stage_.GetPosition();
    data.stageScale = stage_.GetScale();
    data.goal = { goalPoint_.GetPosition(), goalPoint_.GetRotation(), goalPoint_.GetScale() };
    data.goalRadius = goalPoint_.GetRadius();
    const auto& camera = cameraController_.GetPlayerCamera();
    data.yaw = camera.GetYaw();
    data.pitch = camera.GetPitch();
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
    elevatorTrip_.reset();
    messageWindow_.Close();
    bloodEffect_.Reset();
    bulletMarks_.clear();
    muzzleFlashTimer_ = hitMarkerTimer_ = tracerAge_ = 0;
    hitMarkerKill_ = tracerActive_ = false;
    checkpointRequested_ = false;
    goalRequested_ = false;
    pendingSecurityDialogue_ = false;
    const bool oldLayout = sceneStartPosition_ && data.layoutRevision < CheckpointData::CurrentLayoutRevision;
    guidancePins_ = oldLayout ? GuidanceProgress{} : data.guidance;
    auto savedPlayer = data.player;
    if (oldLayout) savedPlayer.position = *sceneStartPosition_;
    player_.RestoreCheckpoint(savedPlayer);
    inventory_.RestoreCheckpoint(data.inventory, oldLayout);
    player_.SetHasGun(inventory_.IsGunCollected());

    if (oldLayout) {
        // Keep the new floor transitions and restart from the authored spawn.
        enemyManager_.RespawnFromPlacements();
        for (auto& door : doors_) {
            if (door.GetOpenConditionType() != DoorOpenConditionType::Crowbar) continue;
            auto state = door.CaptureCheckpoint();
            state.open = false;
            state.progress = 0.0f;
            state.conditionSatisfied = false;
            door.RestoreCheckpoint(state);
        }
    } else {
        stage_.SetPosition(data.stagePosition);
        stage_.SetScale(data.stageScale);
        if (!data.doors.empty()) {
            const auto count = (std::min)(doors_.size(), data.doors.size());
            for (std::size_t i = 0; i < count; ++i) doors_[i].RestoreCheckpoint(data.doors[i]);
        } else if (!doors_.empty()) {
            doors_.front().RestoreCheckpoint(data.door);
        }
        enemyManager_.Clear();
        enemyManager_.RestoreSpawnStatus(inventory_);
        auto& placements = enemyManager_.GetSpawnPoints();
        for (const auto& saved : data.enemies) {
            const auto id = enemyManager_.Spawn(saved.position, saved.config, saved.yaw);
            enemyManager_.Find(id)->RestoreCheckpoint(saved);
            if (saved.spawnPointIndex >= 0 && size_t(saved.spawnPointIndex) < placements.size()) {
                auto& placement = placements[saved.spawnPointIndex];
                placement.spawned = true;
                placement.spawnedEnemyId = id;
            }
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
    }
    securityPC_.SetActivated(std::any_of(doors_.begin(), doors_.end(), [](const DoorMove& door) {
        return door.GetOpenConditionType() == DoorOpenConditionType::SecurityPCActivated &&
            door.IsOpenConditionSatisfied();
    }));
    const bool securityReached = std::any_of(doors_.begin(), doors_.end(), [](const DoorMove& door) {
        return door.GetOpenConditionType() == DoorOpenConditionType::SecurityTeleportEntered &&
            door.IsOpenConditionSatisfied();
    });
    for (auto& point : securityTeleportPoints_) {
        if (point.GetTeleportType() == SecurityTeleportType::Source) point.SetTriggered(securityReached);
    }
    // Older checkpoints may have saved the light switched off.
    lighting_.Reset();
    auto& camera = cameraController_.GetPlayerCamera();
    camera.SetEyeHeight(player_.GetHeight() * WorldScale::EyeHeightRatio);
    camera.SetPlayerPosition(player_.GetPosition());
    camera.SetViewAngles(oldLayout ? 0.0f : data.yaw, oldLayout ? 0.0f : data.pitch);
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
