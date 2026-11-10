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
}

void GameContext::SetDisplaySettings(float brightness, float greenStrength)
{
    lighting_.SetDisplaySettings(brightness, greenStrength);
}

void GameContext::Init()
{
    guidancePins_ = {};
#ifndef NDEBUG
    debugGameplay_.ClearShots();
#endif
    gameFade_.Reset();
    elevatorTrip_.reset();
    messageWindow_.Close();
    sceneStartPosition_.reset();
    gunTutorialUI_.Init();
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

    enemyManager_.Clear();
    enemyManager_.GetSpawnPoints().clear();

    enemyPlacements_ = {
        {stage_.FindGroundPosition({0.9f, 0.0f, 2.5f}), 0.0f, demo::EnemyConfig::ForType(demo::EnemyType::MonsterMale2)},
        {stage_.FindGroundPosition({-0.9f, 0.0f, 2.5f}), 0.0f, demo::EnemyConfig::ForType(demo::EnemyType::FemaleZombie)},
        {stage_.FindGroundPosition({2.7f, 0.0f, 2.5f}), 0.0f, demo::EnemyConfig::ForType(demo::EnemyType::MonsterMale2)}
    };
    PlaceCharacters();
    horrorAudio_.Reset(player_.GetPosition());
    checkpointRequested_ = false;
    goalRequested_ = false;
    lighting_.Init(RM().GetModelShaders());
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
    guidancePins_ = {};
    gameFade_.Reset();
    elevatorTrip_.reset();
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
    gunTutorialUI_.Reset();
    securityPC_.Reset();
    PlaceCharacters();
    horrorAudio_.Reset(player_.GetPosition());
    checkpointRequested_ = false;
    goalRequested_ = false;
    lighting_.Init(RM().GetModelShaders());
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
    pendingSecurityDialogue_ = false;
    wasSystemCamera_ = false;
}

void GameContext::ShowDialogue(const std::string& dialogueId)
{
    const auto* entry = DM().GetDialogue(dialogueId);
    if (entry != nullptr)
    {
        messageWindow_.Open(entry->speaker, entry->text);
    }
    else if (dialogueId == "security_room_arrival")
    {
        messageWindow_.Open("", u8"ここは…警備室か？");
    }
    else if (dialogueId == "security_pc_missing_key")
    {
        messageWindow_.Open(u8"警備室PC", u8"エラー：認証キーが挿入されていません。\n認証キーを探してセットしてください。");
    }
    else if (dialogueId == "security_pc_with_key")
    {
        messageWindow_.Open("", u8"認証キーを手に入れた。警備室のPCに戻って操作しよう。");
    }
    else if (dialogueId == "security_pc_door_open")
    {
        messageWindow_.Open(u8"警備室PC", u8"認証完了。セキュリティドアのロックを解除しました。");
    }
    else if (dialogueId == "3rd_floor_second")
    {
        messageWindow_.Open("", u8"鍵がかかっている…非常階段の鍵を探さなければ。");
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
    auto& spawnPoints = enemyManager_.GetSpawnPoints();

    if (spawnPoints.empty())
    {
        for (const auto& placement : enemyPlacements_)
        {
            demo::EnemySpawnData spawnPoint;
            spawnPoint.SetPosition(placement.position);
            spawnPoint.yaw = placement.yaw;
            spawnPoint.config = placement.config;
            spawnPoint.condition = demo::EnemySpawnCondition::Normal;
            spawnPoint.despawnCondition = demo::EnemyDespawnCondition::None;
            spawnPoints.push_back(std::move(spawnPoint));
        }
    }

    enemyManager_.RespawnFromPlacements(&inventory_);
}

const DoorMove* GameContext::GetNearbyDoor() const
{
    const DoorMove* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (const auto& door : doors_) {
        if (!door.IsPlayerInInteractionRange(player_) ||
            (door.GetOpenConditionType() == DoorOpenConditionType::Crowbar && door.IsOpenConditionSatisfied())) continue;
        const float distance = Vector3DistanceSqr(player_.GetPosition(), door.GetPosition());
        if (!nearest || distance < nearestDistance) {
            nearest = &door;
            nearestDistance = distance;
        }
    }
    return nearest;
}

bool GameContext::UpdateDialoguePoints()
{
    const auto* inspectedDoor = IsKeyPressed(KEY_E) ? GetNearbyDoor() : nullptr;
    if (inspectedDoor && inspectedDoor->GetOpenConditionType() == DoorOpenConditionType::Crowbar &&
        inspectedDoor->IsLocked(&inventory_)) {
        guidancePins_.shutterInspected = true;
    }
    for (auto& point : eventDialoguePoints_) {
        if (!point.CheckPlayerInArea(player_.GetPosition(), inventory_, &doors_, inspectedDoor)) continue;
        ShowDialogue(point.GetDialogueId());
        if (!IsMessageActive()) continue;
        const auto condition = point.GetCondition();
        if (condition != EventDialogueCondition::LockedDoor) point.SetTriggered(true);
        if (point.GetDialogueId() == "3rd_floor_second") guidancePins_.serviceKey = true;
        return true;
    }
    return false;
}

const TeleportPoint* GameContext::GetNearbyElevator() const
{
    for (const auto& point : teleportPoints_) {
        if (point.GetTeleportType() == TeleportType::Source &&
            Vector3Distance(player_.GetPosition(), point.GetPosition()) <= point.GetRadius())
            return &point;
    }
    return nullptr;
}

void GameContext::Update(float deltaTime)
{
    if (!player_.IsAlive()) {
        inventory_.CancelHealing();
        return;
    }

    if (gunTutorialUI_.IsOpen())
    {
        gunTutorialUI_.Update();
        return;
    }

    if (cameraController_.GetActiveType() == CameraType::System) {
        inventory_.CancelHealing();
        for (auto& door : doors_) door.Update(deltaTime, player_, &inventory_);
        cameraController_.Update();
        wasSystemCamera_ = true;
        return;
    }

    if (wasSystemCamera_) {
        wasSystemCamera_ = false;
        inventory_.ApplyPlacements();
        ApplyEnemyPlacements();
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

        // 通常テレポートポイントのフェード開始判定
        if (!gameFade_.IsFading())
        {
            const auto* elevator = GetNearbyElevator();
            if (elevator && IsKeyPressed(KEY_E)) {
                elevatorTrip_ = elevator->GetPointNumber();
                gameFade_.StartFade(2.5f);
            } else {
                for (const auto& source : securityTeleportPoints_) {
                    if (source.GetTeleportType() != SecurityTeleportType::Source ||
                        !source.CheckPlayerInArea(player_.GetPosition(), inventory_)) continue;
                    gameFade_.StartFade(2.5f);
                    break;
                }
            }
        }

        gameFade_.Update(deltaTime);

        // フェード暗転頂点でのテレポート実行
        if (gameFade_.IsMidpoint())
        {
            if (elevatorTrip_) {
                for (const auto& dest : teleportPoints_) {
                    if (dest.GetTeleportType() != TeleportType::Destination ||
                        dest.GetPointNumber() != *elevatorTrip_) continue;
                    player_.SetPosition(stage_.FindGroundPosition(dest.GetPosition()));
                    cameraController_.GetPlayerCamera().SetViewAngles(dest.GetRotation().y * DEG2RAD, 0.0f);
                    inventory_.CancelHealing();
                    checkpointRequested_ = true;
                    break;
                }
                elevatorTrip_.reset();
            } else {
                for (auto& sourceStp : securityTeleportPoints_)
                {
                    if (sourceStp.GetTeleportType() == SecurityTeleportType::Source &&
                        sourceStp.CheckPlayerInArea(player_.GetPosition(), inventory_))
                    {
                        sourceStp.SetTriggered(true);

                        for (const auto& destStp : securityTeleportPoints_)
                        {
                            if (destStp.GetTeleportType() == SecurityTeleportType::Destination &&
                                destStp.GetPointNumber() == sourceStp.GetPointNumber())
                            {
                                Vector3 targetPos = stage_.FindGroundPosition(destStp.GetPosition());
                                player_.SetPosition(targetPos);
                                checkpointRequested_ = true;
                                break;
                            }
                        }

                        enemyManager_.DespawnBySecurityTeleport();

                        for (auto& d : doors_)
                        {
                            if (d.GetOpenConditionType() == DoorOpenConditionType::SecurityTeleportEntered)
                            {
                                d.SetOpenConditionSatisfied(true);
                            }
                        }

                        pendingSecurityDialogue_ = true;
                        break;
                    }
                }
            }
        }

        // 警備室到着セリフを表示し、銃オブジェクトへの誘導ピンを点灯
        if (!gameFade_.IsFading() && pendingSecurityDialogue_)
        {
            pendingSecurityDialogue_ = false;
            ShowDialogue("security_room_arrival");
        }

        float distToGoal = Vector3Distance(player_.GetPosition(), goalPoint_.GetPosition());
        if (distToGoal <= goalPoint_.GetRadius() &&
            std::all_of(doors_.begin(), doors_.end(), [](const DoorMove& door) {
                return door.GetOpenConditionType() != DoorOpenConditionType::Crowbar ||
                    (door.IsOpenConditionSatisfied() && door.IsFullyOpen());
            }))
        {
            goalRequested_ = true;
            return;
        }

        if (IsMessageActive() || (!gameFade_.IsFading() && UpdateDialoguePoints())) return;

        // 警備室PC（SecurityPC）のインタラクト操作
        if (!gameFade_.IsFading() && securityPC_.CanInteract(player_.GetPosition()))
        {
            if (IsKeyPressed(KEY_E))
            {
                if (securityPC_.IsActivated())
                {
                    ShowDialogue("security_pc_door_open");
                    AM().PlaySe(SeId::Confirm);
                }
                else if (!inventory_.IsAuthenticationKeyCollected())
                {
                    guidancePins_.authKey = true;
                    ShowDialogue("security_pc_missing_key");
                    AM().PlaySe(SeId::Error);
                }
                else
                {
                    securityPC_.SetActivated(true);
                    ShowDialogue("security_pc_door_open");
                    AM().PlaySe(SeId::Confirm);

                    for (auto& d : doors_)
                    {
                        if (d.GetOpenConditionType() == DoorOpenConditionType::SecurityPCActivated)
                        {
                            d.SetOpenConditionSatisfied(true);
                        }
                    }
                }
            }
        }
        if (IsMessageActive()) return;
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

        const bool mustInspectShutter = !guidancePins_.shutterInspected &&
            std::any_of(doors_.begin(), doors_.end(), [](const DoorMove& door) {
                return door.GetOpenConditionType() == DoorOpenConditionType::Crowbar;
            });
        inventory_.Update(deltaTime, camera, &stage_, mustInspectShutter ? ItemType::Crowbar : ItemType::None);

        const auto pickedUp = inventory_.GetPickedUpItem();
        if (pickedUp != ItemType::None) enemyManager_.SpawnReadyEnemies(&inventory_);
        if (pickedUp == ItemType::Gun) {
            player_.SetHasGun(true);
            gunTutorialUI_.Open();
        } else if (pickedUp == ItemType::AuthenticationKey) {
            ShowDialogue("security_pc_with_key");
        } else if (pickedUp == ItemType::Ammo) {
            player_.AddReserveAmmo(Inventory::AmmoPickupAmount);
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
    gunTutorialUI_.End();

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
