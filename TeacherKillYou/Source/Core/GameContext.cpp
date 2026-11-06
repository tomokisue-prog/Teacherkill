#include "HorrorLighting.h"
#include "GameContext.h"
#include "WorldScale.h"
#include "AudioManager.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "raylib.h"
#include "CameraController.h"
#include "raymath.h"
#include <algorithm>
#include "SceneManager.h"
#include "DialogueManager.h"
#include "../Events/EventManager.h"
#include "../Events/Tutorial4FFloorEvent.h"
#include <cmath>

namespace
{
    constexpr int MEDKIT_HEAL_AMOUNT = 50;
    constexpr int AMMO_PICKUP_AMOUNT = 30;
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
    // 新しいラウンドへ、前回の暗転・会話・ドア開閉・ライト状態を持ち越さない。
    gameFade_.Reset();
    messageWindow_.Close();
    doors_.clear();
    DoorMove defaultDoor;
    doors_.push_back(defaultDoor);
    lighting_.Reset();
    cameraController_.Init();
    player_.Init();
    stage_.Init();
    inventory_.Init(stage_);
    bloodEffect_.Init();
    lowHpBlood_.Init();
    enemyPlacements_.clear();

    PlaceCharacters();
    horrorAudio_.Reset(player_.GetPosition());
    checkpointRequested_ = false;
    goalRequested_ = false;
    lighting_.Init(RM().GetModelShaders(), RM().GetModel(ResourceKeys::Model_Stage1));
    hitMarkerTimer_ = 0.0f;
    hitMarkerKill_ = false;
    securityTeleportUnlocked_ = false;
    pendingSecurityDialogue_ = false;
    showGunPin_ = false;
    gunPinPosition_ = { 0.0f, 0.0f, 0.0f };
    showAuthKeyPin_ = false;
    hasTriggeredAuthKeyPickup_ = false;
    showSecurityDoorPin_ = false;

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
    securityPC_.Init();

    gunTutorialUI_.Init();

    // ゾンビ遭遇エリアのイベントポイントを初期化・配置
    eventDialoguePoints_.clear();
    EventDialoguePoint zombieEvent;
    zombieEvent.SetPosition({ 5.0f, 0.0f, -5.0f }); // ゾンビ手前の通路の座標などに調整
    zombieEvent.SetRadius(3.0f);                    // 範囲
    zombieEvent.SetDialogueId("zombie_encounter");  // JSONで設定したID
    eventDialoguePoints_.push_back(zombieEvent);
}

void GameContext::Reset()
{
#ifndef NDEBUG
    debugGameplay_.ClearShots();
#endif
    AM().StopAllSe();
    player_.Reset();
    stage_.Reset();
    doors_.clear();
    DoorMove defaultDoor;
    doors_.push_back(defaultDoor);
    inventory_.Reset();
    bloodEffect_.Reset();
    gunTutorialUI_.Init();
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
    securityTeleportUnlocked_ = false;
    pendingSecurityDialogue_ = false;
    showAuthKeyPin_ = false;
    hasTriggeredAuthKeyPickup_ = false;
    showSecurityDoorPin_ = false;
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
    inventory_.ApplyPlacements();

    player_.SetPosition(stage_.FindGroundPosition(player_.GetPosition()));

    // PlayerのTransformは維持し、足元の高さだけをステージの床へ合わせる。
    ApplyEnemyPlacements();
    paladinPosition_ = stage_.FindGroundPosition({ 0.0f, 0.0f, 3.0f });
    cameraController_.GetPlayerCamera().SetEyeHeight(player_.GetHeight() * WorldScale::EyeHeightRatio);
    cameraController_.GetPlayerCamera().SetPlayerPosition(player_.GetPosition());
    cameraController_.GetPlayerCamera().Update();
}

void GameContext::ApplyEnemyPlacements()
{
    enemyManager_.Clear();

    // enemyPlacements_ を使う場合は spawnPoints に条件付きで追加する
    auto& spawnPoints = enemyManager_.GetSpawnPoints();
    spawnPoints.clear();

    for (const auto& placement : enemyPlacements_)
    {
        // デフォルトでは None や Normal などの設定に合わせてスポーンポイントに追加
        enemyManager_.AddSpawnPoint(placement.position, placement.config, demo::EnemySpawnCondition::Normal, placement.yaw);
    }
}

void GameContext::Update(float deltaTime)
{
    if (gunTutorialUI_.IsOpen())
    {
        gunTutorialUI_.Update();
        return;
    }

    if (!player_.IsAlive()) {
        inventory_.CancelHealing();
        return;
    }
    if (cameraController_.GetActiveType() == CameraType::System) {
        inventory_.CancelHealing();
        for (auto& d : doors_)
        {
            d.Update(deltaTime, player_, &inventory_);
        }
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

    // 1. プレイヤーの移動入力更新（プレイヤーカメラ使用時のみ移動入力受付）
    if (isPlayerCameraActive)
    {
        Vector3 forward = cameraController_.GetPlayerCamera().GetForwardVector();
        Vector3 right = cameraController_.GetPlayerCamera().GetRightVector();

        // プレイヤーの移動前の位置を保持しておく
        Vector3 oldPlayerPos = player_.GetPosition();

        // プレイヤー更新
        player_.Update(deltaTime, stage_, forward, right);
        if (!player_.IsAlive()) {
            inventory_.CancelHealing();
            return;
        }

        // 全てのドアとの当たり判定チェック（すり抜け防止）
        for (auto& d : doors_)
        {
            BoundingBox doorBox = d.GetBoundingBox();
            Vector3 playerCenter = {
                player_.GetPosition().x,
                player_.GetPosition().y + player_.GetHeight() * 0.5f,
                player_.GetPosition().z
            };

            if (CheckCollisionBoxSphere(doorBox, playerCenter, player_.GetRadius()))
            {
                player_.SetPosition(oldPlayerPos);
            }
        }

        // テレポート判定
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

        // フェードの更新
        gameFade_.Update(deltaTime);

        // 画面が完全に暗転した瞬間にテレポート先へ移動
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
                                AM().PlaySe(SeId::Teleport);
                                checkpointRequested_ = true;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        // 警備室一方通行テレポート判定
        if (!gameFade_.IsFading() && securityTeleportUnlocked_)
        {
            for (auto& sourcePt : securityTeleportPoints_)
            {
                if (sourcePt.GetTeleportType() == SecurityTeleportType::Source)
                {
                    if (!sourcePt.IsTriggered())
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
        }

        // 画面が完全に暗転した瞬間にテレポート先へ移動
        if (gameFade_.IsMidpoint() && securityTeleportUnlocked_)
        {
            for (auto& sourcePt : securityTeleportPoints_)
            {
                if (sourcePt.GetTeleportType() == SecurityTeleportType::Source && !sourcePt.IsTriggered())
                {
                    float dist = Vector3Distance(player_.GetPosition(), sourcePt.GetPosition());
                    if (dist <= sourcePt.GetRadius())
                    {
                        for (const auto& destPt : securityTeleportPoints_)
                        {
                            if (destPt.GetTeleportType() == SecurityTeleportType::Destination &&
                                destPt.GetPointNumber() == sourcePt.GetPointNumber())
                            {
                                Vector3 targetPos = stage_.FindGroundPosition(destPt.GetPosition());
                                player_.SetPosition(targetPos);
                                AM().PlaySe(SeId::Teleport);
                                checkpointRequested_ = true;

                                sourcePt.SetTriggered(true);
                                enemyManager_.DespawnBySecurityTeleport();

                                // 警備室テレポートに入ったら条件付きの全ドアを開放状態にする
                                for (auto& d : doors_)
                                {
                                    if (d.GetOpenConditionType() == DoorOpenConditionType::SecurityTeleportEntered)
                                    {
                                        d.SetOpenConditionSatisfied(true);
                                    }
                                }

                                // テレポートしたので、フェード完了後のセリフ待ちフラグを有効にする
                                pendingSecurityDialogue_ = true;

                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        if (pendingSecurityDialogue_ && !gameFade_.IsFading())
        {
            ShowDialogue("security_room_arrival");
            pendingSecurityDialogue_ = false; // 一度出したらフラグをオフにする

            showGunPin_ = true;
        }

        float distToGoal = Vector3Distance(player_.GetPosition(), goalPoint_.GetPosition());
        if (distToGoal <= goalPoint_.GetRadius())
        {
            AM().PlaySe(SeId::Confirm);
            SM().ChangeScene(SceneID::Goal);
            return;
        }
    }

    // 全てのドアを更新
    for (auto& d : doors_)
    {
        d.Update(deltaTime, player_, &inventory_);
    }

    // 2. ワールドオブジェクト（ステージ・敵）の更新
    // 複数のドアがあるため、プレイヤーに最も近いドアのバウンディングボックスを敵の障害物として渡す
    BoundingBox nearestDoorBox{};
    bool hasValidDoor = false;
    float minDist = 999.0f;

    for (const auto& d : doors_)
    {
        float dist = Vector3Distance(player_.GetPosition(), d.GetPosition());
        if (dist < minDist)
        {
            minDist = dist;
            nearestDoorBox = d.GetBoundingBox();
            hasValidDoor = true;
        }
    }

    const BoundingBox* doorBoxPtr = hasValidDoor ? &nearestDoorBox : nullptr;
    enemyManager_.Update(deltaTime, player_, stage_, doorBoxPtr, &inventory_);
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

        bool wasHasGun = player_.HasGun();
        player_.SetHasGun(inventory_.IsGunCollected());

        if (!wasHasGun && player_.HasGun())
        {
            gunTutorialUI_.Open();
            showGunPin_ = false;
            showPCPin_ = true;
        }

        if (inventory_.GetPickedUpItem() == ItemType::Ammo)
        {
            player_.AddReserveAmmo(AMMO_PICKUP_AMOUNT);
        }

        if (inventory_.GetUsedItem() == ItemType::Medkit)
        {
            player_.Heal(MEDKIT_HEAL_AMOUNT);
        }

        if (!hasTriggeredLostItemDialogue_ && inventory_.IsLostItemCollected())
        {
            hasTriggeredLostItemDialogue_ = true;
            ShowDialogue("second_line_of_dialogue");
        }

        if (inventory_.IsLostItemCollected() && securityPC_.CanInteract(player_.GetPosition()))
        {
            if (IsKeyPressed(KEY_E))
            {
                if (!securityPC_.IsActivated())
                {
                    // 認証キーを持っているかどうかで分岐する
                    if (inventory_.IsAuthenticationKeyCollected())
                    {
                        // キーを持った状態でPCを起動した場合のセリフに変更
                        ShowDialogue("security_pc_door_open");
                        securityPC_.SetActivated(true); // PC起動済み（扉のロック解除など）にする
                        showPCPin_ = false;            // PCピンを消す
                        showSecurityDoorPin_ = true;

                        // 必要に応じて、ここで階段のドアのロックを解除する処理を追加できます
                        for (auto& d : doors_)
                        {
                            if (d.GetOpenConditionType() == DoorOpenConditionType::SecurityPCActivated)
                            {
                                d.SetOpenConditionSatisfied(true);
                            }
                        }
                    }
                    else
                    {
                        ShowDialogue("security_pc_missing_key");
                        showPCPin_ = false;
                        showAuthKeyPin_ = true;        // キーがなければ再度キーへ誘導
                    }
                }
            }
        }
        if (!hasTriggeredAuthKeyPickup_ && inventory_.IsAuthenticationKeyCollected())
        {
            hasTriggeredAuthKeyPickup_ = true;
            showAuthKeyPin_ = false;
            showPCPin_ = true; // 認証キーを取ったら、再び警備用PCへのピンを指す
        }

        if (!IsMessageActive())
        {
            for (auto& eventPoint : eventDialoguePoints_)
            {
                if (eventPoint.CheckPlayerInArea(player_.GetPosition(), inventory_))
                {
                    eventPoint.SetTriggered(true);
                    ShowDialogue(eventPoint.GetDialogueId());

                    securityTeleportUnlocked_ = true;
                    break;
                }
            }
        }

        // 扉への誘導ピンが表示中、対象のドアが開いたらピンを消す
        if (showSecurityDoorPin_)
        {
            for (const auto& d : doors_)
            {
                if (d.GetOpenConditionType() == DoorOpenConditionType::SecurityPCActivated)
                {
                    if (d.IsOpen() || Vector3Distance(player_.GetPosition(), d.GetPosition()) < 2.0f)
                    {
                        showSecurityDoorPin_ = false;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        inventory_.CancelHealing();
    }
    // Synchronize the on-screen blood with the current HP (damage / medkit).
    lowHpBlood_.Update(deltaTime, player_.GetHp(), player_.GetMaxHp());

    // 6. プレイヤー射撃処理
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
    gunTutorialUI_.End();
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
    const bool hidden=IsCursorHidden();
    inventory_.CancelHealing();gameFade_.Reset();checkpointRequested_=false;
    muzzleFlashTimer_=0;tracerActive_=false;horrorAudio_.Reset(position);
    cameraController_.SetActiveCamera(CameraType::Player);
    auto& camera=cameraController_.GetPlayerCamera();camera.SetPlayerPosition(position);camera.SetViewAngles(yaw,pitch);
    if(!hidden) EnableCursor();
    return true;
}
#endif
