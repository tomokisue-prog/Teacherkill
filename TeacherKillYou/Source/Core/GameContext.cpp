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
    lighting_.Init(RM().GetModelShaders(), RM().GetModel(stage_.GetStageKey()));
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
    gunTutorialUI_.Reset();
    securityPC_.Reset();
    PlaceCharacters();
    horrorAudio_.Reset(player_.GetPosition());
    checkpointRequested_ = false;
    goalRequested_ = false;
    lighting_.Init(RM().GetModelShaders(), RM().GetModel(stage_.GetStageKey()));
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
    showGunPin_ = false;
    showPCPin_ = false;
    showAuthKeyPin_ = false;
    showSecurityDoorPin_ = false;
    showEmergencyStairsDoorPin_ = false;
    showEmergencyKeyPin_ = false;
    hasTriggeredEmergencyDoorDialogue_ = false;
    securityTeleportUnlocked_ = false;
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

    enemyManager_.Clear();

    for (auto& sp : spawnPoints)
    {
        sp.spawned = false;
        sp.spawnedEnemyId = 0;

        bool shouldSpawn = false;
        if (sp.condition == demo::EnemySpawnCondition::Normal)
        {
            shouldSpawn = true;
        }
        else if (sp.condition == demo::EnemySpawnCondition::LostItemCollected)
        {
            if (inventory_.IsLostItemCollected())
            {
                shouldSpawn = true;
            }
        }

        if (shouldSpawn)
        {
            sp.spawnedEnemyId = enemyManager_.Spawn(
                sp.GetPosition(), sp.config, sp.yaw);
            sp.spawned = true;
        }
    }
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

            for (auto& sourceStp : securityTeleportPoints_)
            {
                if (sourceStp.GetTeleportType() == SecurityTeleportType::Source &&
                    sourceStp.CheckPlayerInArea(player_.GetPosition(), inventory_))
                {
                    gameFade_.StartFade(2.5f);
                    break;
                }
            }
        }

        gameFade_.Update(deltaTime);

        // フェード暗転頂点でのテレポート実行
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

            for (auto& sourceStp : securityTeleportPoints_)
            {
                if (sourceStp.GetTeleportType() == SecurityTeleportType::Source &&
                    sourceStp.CheckPlayerInArea(player_.GetPosition(), inventory_))
                {
                    sourceStp.SetTriggered(true);
                    securityTeleportUnlocked_ = false;

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

        // 警備室到着セリフを表示し、銃オブジェクトへの誘導ピンを点灯
        if (!gameFade_.IsFading() && pendingSecurityDialogue_)
        {
            pendingSecurityDialogue_ = false;
            ShowDialogue("security_room_arrival");
            if (!inventory_.IsGunCollected())
            {
                showGunPin_ = true;
            }
        }

        float distToGoal = Vector3Distance(player_.GetPosition(), goalPoint_.GetPosition());
        if (distToGoal <= goalPoint_.GetRadius())
        {
            goalRequested_ = true;
            return;
        }

        // イベントセリフポイントの判定
        for (auto& ep : eventDialoguePoints_)
        {
            if (ep.CheckPlayerInArea(player_.GetPosition(), inventory_, &doors_))
            {
                ep.SetTriggered(true);

                const auto* dialogue = DM().GetDialogue(ep.GetDialogueId());
                if (dialogue != nullptr)
                {
                    messageWindow_.Open(dialogue->speaker, dialogue->text);
                }
                else if (ep.GetDialogueId() == "stairs_blocked_3f")
                {
                    messageWindow_.Open("", u8"階段はふさがれていてこれ以上下には行けないようだ、おそらく非常階段なら下に行けそうだ");
                }

                if (ep.GetCondition() == EventDialogueCondition::LostItemCollected)
                {
                    securityTeleportUnlocked_ = true;
                }

                // ★ SecurityPCDoorUnlocked のセリフポイントに入ったら、非常階段ドアへ誘導ピンを点灯！
                if (ep.GetCondition() == EventDialogueCondition::SecurityPCDoorUnlocked)
                {
                    showSecurityDoorPin_ = false; // セキュリティドアのピンを消灯
                    showEmergencyStairsDoorPin_ = true; // 非常階段ドアへの誘導ピンを点灯
                }
            }
        }

        // ★ 修正：SecurityPCDoorUnlockedに触れて非常階段ドアへのピン（showEmergencyStairsDoorPin_）が出ている状態で、
        // 鍵未所持のまま非常階段ドアを [E] キーで開けようとした時のみセリフを出してキーへ誘導する！
        if (showEmergencyStairsDoorPin_ && !inventory_.HasEmergencyStairsKey())
        {
            for (const auto& d : doors_)
            {
                if (d.GetOpenConditionType() == DoorOpenConditionType::EmergencyStairsKey)
                {
                    if (d.IsPlayerInInteractionRange(player_) && IsKeyPressed(KEY_E))
                    {
                        ShowDialogue("3rd_floor_second");
                        showEmergencyStairsDoorPin_ = false; // ドアへの誘導ピンを消灯
                        showEmergencyKeyPin_ = true;         // 非常階段の鍵へ誘導するピンを点灯！
                        break;
                    }
                }
            }
        }

        // 非常階段ドアが開いた場合は誘導ピンを消灯
        if (showEmergencyStairsDoorPin_)
        {
            for (const auto& d : doors_)
            {
                if (d.GetOpenConditionType() == DoorOpenConditionType::EmergencyStairsKey && d.IsOpen())
                {
                    showEmergencyStairsDoorPin_ = false;
                    break;
                }
            }
        }

        // SecurityPCActivatedドアが開いた場合は誘導ピンを消灯
        if (showSecurityDoorPin_)
        {
            for (const auto& d : doors_)
            {
                if (d.GetOpenConditionType() == DoorOpenConditionType::SecurityPCActivated && d.IsOpen())
                {
                    showSecurityDoorPin_ = false;
                    break;
                }
            }
        }

        // 警備室PC（SecurityPC）のインタラクト操作
        if (!IsMessageActive() && !gunTutorialUI_.IsOpen() && securityPC_.CanInteract(player_.GetPosition()))
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
                    ShowDialogue("security_pc_missing_key");
                    showPCPin_ = false;
                    showAuthKeyPin_ = true;
                    AM().PlaySe(SeId::Error);
                }
                else
                {
                    securityPC_.SetActivated(true);
                    showPCPin_ = false;
                    showSecurityDoorPin_ = true;
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

        // 銃（GUN）を拾った瞬間
        if (inventory_.GetPickedUpItem() == ItemType::Gun)
        {
            player_.SetHasGun(true);
            showGunPin_ = false;
            gunTutorialUI_.Open();

            if (!securityPC_.IsActivated())
            {
                showPCPin_ = true;
            }
        }

        // 認証キーを拾った瞬間
        if (inventory_.GetPickedUpItem() == ItemType::AuthenticationKey)
        {
            showAuthKeyPin_ = false;
            ShowDialogue("security_pc_with_key");

            if (!securityPC_.IsActivated())
            {
                showPCPin_ = true;
            }
        }

        // 非常階段のキー（EmergencyStairsKey）を拾った瞬間：キーピン消灯＆ドアへ誘導ピンを再点灯！
        if (inventory_.GetPickedUpItem() == ItemType::EmergencyStairsKey)
        {
            showEmergencyKeyPin_ = false;        // キーへのピンを消灯
            showEmergencyStairsDoorPin_ = true;  // ドアへ戻る誘導ピンを点灯
        }

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
