#include "HorrorLighting.h"
#include "GameContext.h"
#include "WorldScale.h"
#include "AudioManager.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "raylib.h"
#include "CameraController.h"
#include "raymath.h"
#include <fstream>
#include "json.hpp"
#include "rlgl.h"
#include <algorithm>
#include "SceneManager.h"
#include "DialogueManager.h"
#include <cmath>

namespace
{
    constexpr int MEDKIT_HEAL_AMOUNT = 50;
    constexpr int AMMO_PICKUP_AMOUNT = 16;
}

void GameContext::SetDisplaySettings(float brightness, float greenStrength)
{
    lighting_.SetDisplaySettings(brightness, greenStrength);
}

void GameContext::Init()
{
#ifndef NDEBUG
    debugGameplay_.ClearShots();
#endif
    gameFade_.Reset();
    messageWindow_.Close();
    doors_.clear();
    doors_.emplace_back();
    doors_.back().Init();
    lighting_.Reset();
    cameraController_.Init();
    player_.Init();
    stage_.Init();
    inventory_.Init(stage_);
    bloodEffect_.Init();
    lowHpBlood_.Init();
    enemyPlacements_ = {
        {stage_.FindGroundPosition({0.9f, 0.0f, 2.5f}), 0.0f, demo::EnemyConfig::ForType(demo::EnemyType::MonsterMale2)},
        {stage_.FindGroundPosition({-0.9f, 0.0f, 2.5f}), 0.0f, demo::EnemyConfig::ForType(demo::EnemyType::FemaleZombie)},
        {stage_.FindGroundPosition({2.7f, 0.0f, 2.5f}), 0.0f, demo::EnemyConfig::ForType(demo::EnemyType::MonsterMale2)}
    };
    PlaceCharacters();
    horrorAudio_.Reset(player_.GetPosition());
    checkpointRequested_ = false;
    goalRequested_ = false;
    lighting_.Init(RM().GetModelShaders(), RM().GetModel(ResourceKeys::Model_Stage1));
    hitMarkerTimer_ = 0.0f;
    hitMarkerKill_ = false;

    teleportPoints_.clear();
    TeleportPoint tp1;
    tp1.SetPosition({ 3.0f, 0.0f, 0.0f });
    tp1.SetPointNumber(1);
    tp1.SetTeleportType(TeleportType::Source);
    teleportPoints_.push_back(tp1);

    TeleportPoint tp2;
    tp2.SetPosition({ 8.0f, 0.0f, 0.0f });
    tp2.SetPointNumber(1);
    tp2.SetTeleportType(TeleportType::Destination);
    teleportPoints_.push_back(tp2);

    bulletMarks_.clear();

    if (muzzleFlashTexture_.id == 0)
    {
        muzzleFlashTexture_ =
            LoadTexture("Data/Image/Effects/muzzleFlash0.png");

        if (muzzleFlashTexture_.id != 0)
        {
            SetTextureFilter(
                muzzleFlashTexture_, TEXTURE_FILTER_BILINEAR);
        }
    }

    muzzleFlashTimer_ = 0.0f;
    tracerActive_ = false;
    tracerAge_ = 0.0f;

    goalPoint_.Init();
    goalPoint_.SetPosition({ 5.0f, 0.0f, 5.0f });

    DM().LoadFromJson("Data/Json/dialogue.json");
    ShowDialogue("door_locked");

    EventMgr().RegisterEvent("Tutorial_4F", std::make_shared<Tutorial4FFloorEvent>());
    EventMgr().StartEvent("Tutorial_4F");

    ShowDialogue("opening_4f");
}

void GameContext::Reset()
{
#ifndef NDEBUG
    debugGameplay_.ClearShots();
#endif
    AM().StopAllSe();
    player_.Reset();
    stage_.Reset();
    for (auto& door : doors_) door.Reset();
    inventory_.Reset();
    bloodEffect_.Reset();
    lowHpBlood_.Reset();
    PlaceCharacters();
    horrorAudio_.Reset(player_.GetPosition());
    checkpointRequested_ = false;
    goalRequested_ = false;
    lighting_.Init(RM().GetModelShaders(), RM().GetModel(ResourceKeys::Model_Stage1));
    hitMarkerTimer_ = 0.0f;
    hitMarkerKill_ = false;
    bulletMarks_.clear();
    teleportPoints_.clear();
    TeleportPoint tp1;
    tp1.SetPosition({ 3.0f, 0.0f, 0.0f });
    tp1.SetPointNumber(1);
    tp1.SetTeleportType(TeleportType::Source);
    teleportPoints_.push_back(tp1);

    TeleportPoint tp2;
    tp2.SetPosition({ 8.0f, 0.0f, 0.0f });
    tp2.SetPointNumber(1);
    tp2.SetTeleportType(TeleportType::Destination);
    teleportPoints_.push_back(tp2);

    muzzleFlashTimer_ = 0.0f;
    tracerActive_ = false;
    tracerAge_ = 0.0f;
    goalPoint_.Reset();
}

void GameContext::ShowDialogue(const std::string& dialogueId)
{
    const auto* entry = DM().GetDialogue(dialogueId);
    if (entry != nullptr)
    {
        messageWindow_.Open(entry->speaker, entry->text);
    }
}

void GameContext::PlaceCharacters()
{
    player_.SetPosition(stage_.FindGroundPosition(player_.GetPosition()));
    ApplyEnemyPlacements();
    paladinPosition_ = stage_.FindGroundPosition({ 0.0f, 0.0f, 3.0f });
    cameraController_.GetPlayerCamera().SetEyeHeight(player_.GetHeight() * WorldScale::EyeHeightRatio);
    cameraController_.GetPlayerCamera().SetPlayerPosition(player_.GetPosition());
    cameraController_.GetPlayerCamera().Update();
}

void GameContext::ApplyEnemyPlacements()
{
    enemyManager_.Clear();
    auto& spawnPoints = enemyManager_.GetSpawnPoints();
    spawnPoints.clear();

    for (const auto& placement : enemyPlacements_)
    {
        demo::EnemySpawnData spawnPoint;
        spawnPoint.SetPosition(placement.position);
        spawnPoint.yaw = placement.yaw;
        spawnPoint.config = placement.config;
        spawnPoint.condition = demo::EnemySpawnCondition::Normal;
        spawnPoint.despawnCondition = demo::EnemyDespawnCondition::None;
        spawnPoint.spawnedEnemyId = enemyManager_.Spawn(
            spawnPoint.GetPosition(), spawnPoint.config, spawnPoint.yaw);
        spawnPoint.spawned = true;
        spawnPoints.push_back(std::move(spawnPoint));
    }
}

void GameContext::Update(float deltaTime)
{
    if (!player_.IsAlive()) {
        inventory_.CancelHealing();
        return;
    }
    if (cameraController_.GetActiveType() == CameraType::System) {
        inventory_.CancelHealing();
        for (auto& door : doors_) door.Update(deltaTime, player_, &inventory_);
        cameraController_.Update();
        return;
    }
    if (IsMessageActive())
    {
        messageWindow_.Update(deltaTime);
        return;
    }

    EventMgr().Update(deltaTime);

    UpdateShotEffects(deltaTime);
    if (hitMarkerTimer_ > 0.0f)
    {
        hitMarkerTimer_ -= deltaTime;

        if (hitMarkerTimer_ < 0.0f)
        {
            hitMarkerTimer_ = 0.0f;
        }
    }

    lighting_.Update();
    bool isPlayerCameraActive = (cameraController_.GetActiveType() == CameraType::Player);

    if (isPlayerCameraActive)
    {
        Vector3 forward = cameraController_.GetPlayerCamera().GetForwardVector();
        Vector3 right = cameraController_.GetPlayerCamera().GetRightVector();

        Vector3 oldPlayerPos = player_.GetPosition();

        player_.Update(deltaTime, stage_, forward, right);
        if (!player_.IsAlive()) {
            inventory_.CancelHealing();
            return;
        }

        Vector3 playerCenter = {
            player_.GetPosition().x,
            player_.GetPosition().y + player_.GetHeight() * 0.5f,
            player_.GetPosition().z
        };

        for (const auto& door : doors_)
        {
            if (!CheckCollisionBoxSphere(door.GetBoundingBox(), playerCenter, player_.GetRadius())) continue;
            player_.SetPosition(oldPlayerPos);
            break;
        }

        if (!gameFade_.IsFading())
        {
            for (auto& sourcePt : teleportPoints_)
            {
                if (sourcePt.GetTeleportType() == TeleportType::Source)
                {
                    float dist = Vector3Distance(player_.GetPosition(), sourcePt.GetPosition());
                    if (dist <= sourcePt.GetRadius())
                    {
                        gameFade_.StartFade(2.5f);
                        break;
                    }
                }
            }
        }

        gameFade_.Update(deltaTime);

        if (gameFade_.IsMidpoint())
        {
            for (auto& sourcePt : teleportPoints_)
            {
                if (sourcePt.GetTeleportType() == TeleportType::Source)
                {
                    if (Vector3Distance(player_.GetPosition(), sourcePt.GetPosition()) <= sourcePt.GetRadius())
                    {
                        for (const auto& destPt : teleportPoints_)
                        {
                            if (destPt.GetTeleportType() == TeleportType::Destination &&
                                destPt.GetPointNumber() == sourcePt.GetPointNumber())
                            {
                                Vector3 targetPos = stage_.FindGroundPosition(destPt.GetPosition());
                                player_.SetPosition(targetPos);
                                checkpointRequested_ = true;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        float distToGoal = Vector3Distance(player_.GetPosition(), goalPoint_.GetPosition());
        if (distToGoal <= goalPoint_.GetRadius())
        {
            goalRequested_ = true;
            return;
        }
    }

    for (auto& door : doors_) door.Update(deltaTime, player_, &inventory_);

    BoundingBox doorBounds{};
    const BoundingBox* enemyDoorBlocker = nullptr;
    if (!doors_.empty())
    {
        doorBounds = doors_.front().GetBoundingBox();
        enemyDoorBlocker = &doorBounds;
    }
    enemyManager_.Update(deltaTime, player_, stage_, enemyDoorBlocker, &inventory_);
    enemyManager_.RemoveExpired();

    if (player_.ConsumeDamageImpact())
    {
        cameraController_.GetPlayerCamera().StartDamageImpact();
    }

    cameraController_.GetPlayerCamera().SetEyeHeight(player_.GetHeight() * WorldScale::EyeHeightRatio);
    cameraController_.GetPlayerCamera().SetPlayerPosition(
        player_.GetPosition()
    );

    cameraController_.Update();

    if (isPlayerCameraActive && player_.IsAlive())
    {
        const Camera3D camera =
            cameraController_.GetPlayerCamera().GetRaylibCamera();

        inventory_.Update(deltaTime, camera, &stage_);

        if (inventory_.GetPickedUpItem() == ItemType::Ammo)
        {
            player_.AddReserveAmmo(AMMO_PICKUP_AMOUNT);
        }

        if (inventory_.GetUsedItem() == ItemType::Medkit)
        {
            player_.Heal(MEDKIT_HEAL_AMOUNT);
        }
    }
    else
    {
        inventory_.CancelHealing();
    }

    lowHpBlood_.Update(deltaTime, player_.GetHp(), player_.GetMaxHp());

    ProcessPlayerShot();
    horrorAudio_.Update(deltaTime, player_, cameraController_.GetPlayerCamera(), enemyManager_, stage_, isPlayerCameraActive);
}

void GameContext::End()
{
#ifndef NDEBUG
    debugGameplay_.ClearShots();
#endif
    AM().StopAllSe();
    enemyManager_.Clear();
    player_.End();
    inventory_.End();
    stage_.End();
    bloodEffect_.Shutdown();
    lowHpBlood_.Shutdown();
    lighting_.Shutdown();

    if (muzzleFlashTexture_.id != 0)
    {
        UnloadTexture(muzzleFlashTexture_);
        muzzleFlashTexture_ = {};
    }

    muzzleFlashTimer_ = 0.0f;
    tracerActive_ = false;
    bulletMarks_.clear();
}

#ifndef NDEBUG
bool GameContext::DebugTeleportPlayer(Vector3 position, float yaw, float pitch) {
    if (!std::isfinite(yaw) || !std::isfinite(pitch) || !player_.DebugMoveTo(position)) return false;
    const bool hidden = IsCursorHidden();
    inventory_.CancelHealing(); gameFade_.Reset(); checkpointRequested_ = false;
    muzzleFlashTimer_ = 0; tracerActive_ = false; horrorAudio_.Reset(position);
    cameraController_.SetActiveCamera(CameraType::Player);
    auto& camera = cameraController_.GetPlayerCamera(); camera.SetPlayerPosition(position); camera.SetViewAngles(yaw, pitch);
    if (!hidden) EnableCursor();
    return true;
}
#endif