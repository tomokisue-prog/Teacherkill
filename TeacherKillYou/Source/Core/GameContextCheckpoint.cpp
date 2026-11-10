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
    const bool oldLayout = sceneStartPosition_ && data.layoutRevision < CheckpointData::ReceivedFloorLayoutRevision;
    const bool updateFixtures = sceneStartPosition_ && !oldLayout &&
        data.layoutRevision < CheckpointData::CurrentLayoutRevision;
    guidancePins_ = oldLayout ? GuidanceProgress{} : data.guidance;
    auto savedPlayer = data.player;
    if (oldLayout) savedPlayer.position = *sceneStartPosition_;

    std::optional<TeleportPoint> updatedLanding;
    bool resetLandingView = false;
    if (updateFixtures) {
        const auto landing = std::find_if(teleportPoints_.begin(), teleportPoints_.end(), [](const TeleportPoint& point) {
            return point.GetPointNumber() == 2 && point.GetTeleportType() == TeleportType::Destination;
        });
        const auto previous = std::find_if(data.teleports.begin(), data.teleports.end(), [](const TeleportCheckpoint& point) {
            return point.number == 2 && point.type == int(TeleportType::Destination);
        });
        if (landing != teleportPoints_.end()) {
            updatedLanding = *landing;
            const auto isNear = [&](Vector3 position) {
                return Vector3Distance(savedPlayer.position, position) <= 2.2f;
            };
            if (isNear(landing->GetPosition()) || (previous != data.teleports.end() && isNear(previous->transform.position))) {
                savedPlayer.position = landing->GetPosition();
                resetLandingView = true;
            }
        }
    }

    player_.RestoreCheckpoint(savedPlayer);
    inventory_.RestoreCheckpoint(data.inventory, oldLayout || updateFixtures);
    player_.SetHasGun(inventory_.IsGunCollected());

    if (oldLayout) {
        // Keep the new floor transitions and restart from the authored spawn.
        enemyManager_.RespawnFromPlacements();
        for (auto& door : doors_) {
            if (door.GetOpenConditionType() == DoorOpenConditionType::SecurityTeleportEntered &&
                inventory_.IsGunCollected()) {
                door.SetOpenConditionSatisfied(true);
            }
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
            for (std::size_t i = 0; i < count; ++i) {
                auto saved = data.doors[i];
                if (updateFixtures) {
                    const auto authored = doors_[i].CaptureCheckpoint();
                    saved.transform = authored.transform;
                    saved.distance = authored.distance;
                }
                doors_[i].RestoreCheckpoint(saved);
            }
        } else if (!doors_.empty()) {
            auto saved = data.door;
            if (updateFixtures) {
                const auto authored = doors_.front().CaptureCheckpoint();
                saved.transform = authored.transform;
                saved.distance = authored.distance;
            }
            doors_.front().RestoreCheckpoint(saved);
        }
        if (updateFixtures) {
            // A resized door may cover the spot where the old game let the player save.
            Vector3 position = player_.GetPosition();
            const float radius = player_.GetRadius();
            for (const auto& door : doors_) {
                if (door.IsFullyOpen()) continue;
                const BoundingBox bounds = door.GetBoundingBox();
                const Vector3 center = Vector3Add(position, {0, player_.GetHeight() * .5f, 0});
                if (!CheckCollisionBoxSphere(bounds, center, radius)) continue;
                const float clearance = radius + .02f;
                if (bounds.max.x - bounds.min.x < bounds.max.z - bounds.min.z) {
                    position.x = position.x < (bounds.min.x + bounds.max.x) * .5f ?
                        bounds.min.x - clearance : bounds.max.x + clearance;
                } else {
                    position.z = position.z < (bounds.min.z + bounds.max.z) * .5f ?
                        bounds.min.z - clearance : bounds.max.z + clearance;
                }
            }
            player_.SetPosition(position);
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
            if (updatedLanding && saved.number == updatedLanding->GetPointNumber() &&
                saved.type == int(updatedLanding->GetTeleportType())) {
                teleportPoints_.push_back(*updatedLanding);
                continue;
            }
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
    if (resetLandingView) {
        camera.SetViewAngles(updatedLanding->GetRotation().y * DEG2RAD, 0.0f);
    } else {
        camera.SetViewAngles(oldLayout ? 0.0f : data.yaw, oldLayout ? 0.0f : data.pitch);
    }
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
