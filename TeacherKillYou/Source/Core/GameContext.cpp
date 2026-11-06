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

static void DrawCrosshair()
{
    const int x = GetScreenWidth() / 2;
    const int y = GetScreenHeight() / 2;

    const int gap = 5;
    const int length = 10;
    const int thickness = 2;

    DrawRectangle(x - thickness / 2, y - gap - length,
        thickness, length, WHITE);

    DrawRectangle(x - thickness / 2, y + gap,
        thickness, length, WHITE);

    DrawRectangle(x - gap - length, y - thickness / 2,
        length, thickness, WHITE);

    DrawRectangle(x + gap, y - thickness / 2,
        length, thickness, WHITE);

    DrawCircle(x, y, 2.0f, WHITE);
}
static void DrawHitMarker(bool isKill)
{
    const float x = GetScreenWidth() * 0.5f;
    const float y = GetScreenHeight() * 0.5f;

    const float inner = 7.0f;
    const float outer = 14.0f;
    const float thickness = 2.0f;

    const Color color = isKill ? RED : WHITE;

    DrawLineEx(
        { x - outer, y - outer },
        { x - inner, y - inner },
        thickness, color);

    DrawLineEx(
        { x + inner, y - inner },
        { x + outer, y - outer },
        thickness, color);

    DrawLineEx(
        { x - outer, y + outer },
        { x - inner, y + inner },
        thickness, color);

    DrawLineEx(
        { x + inner, y + inner },
        { x + outer, y + outer },
        thickness, color);
}

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
    inventory_.Init();  // Load item GLB models and spawn items
    bloodEffect_.Init();
    PlaceCharacters();
    horrorAudio_.Reset(player_.GetPosition());
    checkpointRequested_ = false;
    lighting_.Init(RM().GetModelShaders(), RM().GetModel(ResourceKeys::Model_Stage1));
    hitMarkerTimer_ = 0.0f;
    hitMarkerKill_ = false;
    securityTeleportUnlocked_ = false;
    pendingSecurityDialogue_ = false;
    showGunPin_ = false;
    gunPinPosition_ = { 0.0f, 0.0f, 0.0f };
    showAuthKeyPin_ = false;
    hasTriggeredAuthKeyPickup_ = false;

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
    AM().StopAllSe();
    player_.Reset();
    stage_.Reset();
    doors_.clear();
    DoorMove defaultDoor;
    doors_.push_back(defaultDoor);
    inventory_.Reset();
    bloodEffect_.Reset();

    gunTutorialUI_.Init();

    PlaceCharacters();
    horrorAudio_.Reset(player_.GetPosition());
    checkpointRequested_ = false;
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
    inventory_.PlaceOnStage(stage_);

    player_.SetPosition(stage_.FindGroundPosition(player_.GetPosition()));

    enemyManager_.Clear();

    paladinPosition_ = stage_.FindGroundPosition({ 0.0f, 0.0f, 3.0f });
    cameraController_.GetPlayerCamera().SetEyeHeight(player_.GetHeight() * WorldScale::EyeHeightRatio);
    cameraController_.GetPlayerCamera().SetPlayerPosition(player_.GetPosition());
    cameraController_.GetPlayerCamera().Update();
}

void GameContext::ProcessPlayerShot()
{
    if (!player_.HasGun()) return;
    if (!player_.ConsumeShot()) return;
    if (!player_.IsAlive()) return;

    if (cameraController_.GetActiveType() != CameraType::Player)
    {
        return;
    }

    constexpr float shotRange = 100.0f;
    constexpr int shotDamage = 25;

    const Camera3D camera =
        cameraController_.GetPlayerCamera().GetRaylibCamera();

    const Ray ray = {
        camera.position,
        Vector3Normalize(Vector3Subtract(camera.target, camera.position))
    };

    float closestDistance = shotRange;

    const RayCollision wallHit = stage_.Raycast(ray, shotRange);
    if (wallHit.hit)
    {
        closestDistance = wallHit.distance;
    }

    demo::EnemyId targetId = 0;
    bool foundTarget = false;
    Vector3 impactNormal = Vector3Negate(ray.direction);

    for (const auto& enemy : enemyManager_.GetEnemies())
    {
        if (!enemy.IsAlive()) continue;

        const BoundingBox bounds = enemy.GetBodyBounds();
        RayCollision hit = GetRayCollisionBox(ray, bounds);

        const bool inside =
            ray.position.x >= bounds.min.x &&
            ray.position.x <= bounds.max.x &&
            ray.position.y >= bounds.min.y &&
            ray.position.y <= bounds.max.y &&
            ray.position.z >= bounds.min.z &&
            ray.position.z <= bounds.max.z;

        if (inside)
        {
            hit.hit = true;
            hit.distance = 0.0f;
        }

        if (hit.hit &&
            hit.distance >= 0.0f &&
            hit.distance < closestDistance)
        {
            closestDistance = hit.distance;
            targetId = enemy.GetId();
            foundTarget = true;
            impactNormal = inside ? Vector3Negate(ray.direction) : hit.normal;
        }
    }

    muzzleFlashTimer_ = 0.06f;
    muzzleFlashFrame_ = GetRandomValue(0, 3);

    tracerStart_ = player_.GetMuzzlePosition(camera);
    tracerEnd_ = Vector3Add(
        ray.position,
        Vector3Scale(ray.direction, closestDistance));
    tracerAge_ = 0.0f;
    tracerActive_ = true;

    if (wallHit.hit && !foundTarget)
    {
        if (std::fabs(wallHit.normal.y) < 0.65f &&
            Vector3LengthSqr(wallHit.normal) > 0.0001f)
        {
            BulletMark mark;

            mark.position = wallHit.point;
            mark.normal = wallHit.normal;
            mark.remainingTime = bulletMarkLifetime_;

            if (bulletMarks_.size() >=
                static_cast<std::vector<BulletMark>::size_type>(maxBulletMarks_))
            {
                bulletMarks_.erase(bulletMarks_.begin());
            }

            bulletMarks_.push_back(mark);
        }
    }

    if (!foundTarget)
    {
        TraceLog(LOG_INFO, "SHOT: No enemy hit");
        return;
    }

    const demo::EnemyDamageResult result =
        enemyManager_.ApplyDamage(targetId, shotDamage);

    if (result.applied)
    {
        bloodEffect_.Spawn(tracerEnd_, impactNormal);
        hitMarkerTimer_ = 0.12f;
        hitMarkerKill_ = result.becameDead;

        TraceLog(LOG_INFO, "HIT: Enemy HP = %d", result.hp);
        if (result.becameDead)
        {
            TraceLog(LOG_INFO, "Enemy defeated");
        }
    }
}
void GameContext::UpdateShotEffects(float deltaTime)
{
    bloodEffect_.Update(deltaTime);
    muzzleFlashTimer_ = (std::max)(
        0.0f, muzzleFlashTimer_ - deltaTime);

    if (tracerActive_)
    {
        tracerAge_ += deltaTime;

        if (tracerAge_ >= tracerDuration_)
        {
            tracerActive_ = false;
        }
    }

    if (!player_.IsAlive())
    {
        muzzleFlashTimer_ = 0.0f;
    }

    for (BulletMark& mark : bulletMarks_)
    {
        mark.remainingTime -= deltaTime;
    }

    bulletMarks_.erase(
        std::remove_if(
            bulletMarks_.begin(),
            bulletMarks_.end(),
            [](const BulletMark& mark)
            {
                return mark.remainingTime <= 0.0f;
            }
        ),
        bulletMarks_.end()
    );
}
void GameContext::DrawShotEffects() const
{
    if (cameraController_.GetActiveType() != CameraType::Player)
    {
        return;
    }

    const Camera3D camera =
        cameraController_.GetPlayerCamera().GetRaylibCamera();

    const Color flashColor = { 255, 225, 140, 255 };
    const Color tracerColor = { 255, 220, 110, 255 };

    rlDrawRenderBatchActive();
    rlDisableDepthMask();

    BeginBlendMode(BLEND_ADDITIVE);

    if (tracerActive_ && tracerDuration_ > 0.0f)
    {
        const float progress = Clamp(
            tracerAge_ / tracerDuration_, 0.0f, 1.0f);

        const float head = Clamp(
            progress * 1.25f + 0.15f, 0.0f, 1.0f);

        const float tail = Clamp(
            progress * 1.25f - 0.10f, 0.0f, 1.0f);

        const Vector3 start =
            Vector3Lerp(tracerStart_, tracerEnd_, tail);

        const Vector3 end =
            Vector3Lerp(tracerStart_, tracerEnd_, head);

        DrawLine3D(start, end, tracerColor);
    }

    if (muzzleFlashTimer_ > 0.0f &&
        muzzleFlashTexture_.id != 0 &&
        player_.IsAlive())
    {
        const float frameWidth =
            muzzleFlashTexture_.width / 4.0f;

        const Rectangle source = {
            muzzleFlashFrame_ * frameWidth,
            0.0f,
            frameWidth,
            static_cast<float>(muzzleFlashTexture_.height)
        };

        const Vector3 muzzle =
            player_.GetMuzzlePosition(camera);

        DrawBillboardRec(
            camera,
            muzzleFlashTexture_,
            source,
            muzzle,
            { 0.12f, 0.12f },
            flashColor);
    }

    EndBlendMode();

    rlDrawRenderBatchActive();
    rlEnableDepthMask();
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
    }
    else
    {
        inventory_.CancelHealing();
    }

    ProcessPlayerShot();
    horrorAudio_.Update(deltaTime, player_, cameraController_.GetPlayerCamera(), enemyManager_, stage_, isPlayerCameraActive);
}

void GameContext::Draw() const
{
    const bool playerView =
        cameraController_.GetActiveType() == CameraType::Player;

    lighting_.BeginFrame(
        cameraController_.GetActiveRaylibCamera(),
        cameraController_.GetPlayerCamera().GetRaylibCamera(),
        stage_.GetPosition(),
        stage_.GetScale(),
        !playerView
    );

    BeginMode3D(cameraController_.GetActiveRaylibCamera());

    if (!playerView)
    {
        DrawGrid(20, 1.0f);
    }

    stage_.Draw();

    if (!playerView)
    {
        for (const auto& tp : teleportPoints_)
        {
            tp.Draw();
        }

        for (const auto& stp : securityTeleportPoints_)
        {
            stp.Draw();
        }

        goalPoint_.Draw();

        for (const auto& ep : eventDialoguePoints_)
        {
            ep.Draw();
        }
    }

    const PlayerCamera* activePlayerCam = nullptr;

    if (playerView)
    {
        activePlayerCam =
            &cameraController_.GetPlayerCamera();
    }

    // 全てのドアを描画
    for (const auto& d : doors_)
    {
        d.Draw();
    }

    player_.Draw(activePlayerCam);
    enemyManager_.Draw();
    DrawBulletMarks();
    DrawShotEffects();
    inventory_.DrawWorld();
    bloodEffect_.Draw(cameraController_.GetActiveRaylibCamera());

    securityPC_.Draw();

    EndMode3D();
    lighting_.EndFrame(playerView && player_.IsAlive());

    // ==================================
    // 2D HUD
    // ==================================
    if (playerView && player_.IsAlive())
    {
        player_.DrawDamageOverlay();

        DrawCrosshair();

        if (hitMarkerTimer_ > 0.0f)
        {
            DrawHitMarker(hitMarkerKill_);
        }

        player_.DrawHealthHUD();
        player_.DrawStaminaHUD();
        player_.DrawAmmoHUD();

        inventory_.DrawHUD();

        // プレイヤーに最も近いドアを探してインタラクションプロンプトを表示する
        const DoorMove* nearestDoor = nullptr;
        float minDoorDist = 999.0f;
        for (const auto& d : doors_)
        {
            if (d.IsPlayerInInteractionRange(player_))
            {
                float dist = Vector3Distance(player_.GetPosition(), d.GetPosition());
                if (dist < minDoorDist)
                {
                    minDoorDist = dist;
                    nearestDoor = &d;
                }
            }
        }

        if (nearestDoor)
        {
            std::string prompt = "";

            bool isClosedByCondition = (nearestDoor->GetLockType() != DoorLockType::None && nearestDoor->IsLocked(&inventory_));
            bool needsUnlock = false;

            if (nearestDoor->GetLockType() == DoorLockType::None)
            {
                needsUnlock = (nearestDoor->GetOpenConditionType() != DoorOpenConditionType::None && !nearestDoor->IsOpenConditionSatisfied());
            }
            else
            {
                needsUnlock = (nearestDoor->IsLocked(&inventory_) && nearestDoor->GetOpenConditionType() != DoorOpenConditionType::None && !nearestDoor->IsOpenConditionSatisfied());
            }

            if (needsUnlock)
            {
                if (nearestDoor->GetOpenConditionType() == DoorOpenConditionType::SecurityTeleportEntered)
                {
                    prompt = "Locked (Requires Security Teleport)";
                }
                else if (nearestDoor->GetOpenConditionType() == DoorOpenConditionType::SecurityPCActivated)
                {
                    prompt = "Locked (Requires Security PC)"; // 警備用PC起動が必要な場合のメッセージ
                }
                else
                {
                    prompt = "[M] Unlock"; // テスト用Mキーの扉
                }
            }
            else
            {
                prompt = nearestDoor->IsOpen() ? "[E] Close" : "[E] Open";
            }

            if (!prompt.empty())
            {
                const float fontSize = 36.0f;
                Vector2 textSize = JPFont().MeasureText(prompt, fontSize);
                const int x = static_cast<int>((GetScreenWidth() - textSize.x) / 2.0f);
                const int y = static_cast<int>((GetScreenHeight() / 2.0f) + 30.0f);

                JPFont().DrawText(prompt, x + 2, y + 2, fontSize, BLACK);
                JPFont().DrawText(prompt, x, y, fontSize, RAYWHITE);
            }
        }

        if (securityTeleportUnlocked_)
        {
            for (const auto& stp : securityTeleportPoints_)
            {
                if (stp.GetTeleportType() == SecurityTeleportType::Source &&
                    !stp.IsTriggered())
                {
                    Vector3 worldPos = stp.GetPosition();
                    worldPos.y += stp.GetRadius() * 0.5f;

                    const Camera3D& currentCam = cameraController_.GetPlayerCamera().GetRaylibCamera();
                    Vector2 screenPos = GetWorldToScreen(worldPos, currentCam);

                    Vector3 toPoint = Vector3Subtract(worldPos, currentCam.position);
                    float forwardDot = Vector3DotProduct(
                        Vector3Normalize(Vector3Subtract(currentCam.target, currentCam.position)),
                        Vector3Normalize(toPoint)
                    );

                    if (forwardDot > 0.0f &&
                        screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                        screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
                    {
                        float distance = Vector3Length(toPoint);
                        float baseSize = 40.0f;
                        float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                        float size = baseSize * scaleFactor;

                        float floatOffset = std::sin(GetTime() * 4.0f) * 4.0f * scaleFactor;
                        float centerY = screenPos.y + floatOffset;

                        Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                        Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                        Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                        DrawTriangle(topLeft, tip, topRight, GREEN);
                        DrawTriangleLines(topLeft, tip, topRight, BLUE);
                    }
                }
            }
        }

        // 銃への誘導ピンの描画
        if (showGunPin_)
        {
            // インベントリから実際の銃のワールド座標を取得する
            Vector3 worldPos = inventory_.GetGunPosition();
            worldPos.y += 0.5f; // 少し上方に浮かせる

            const Camera3D& currentCam = cameraController_.GetPlayerCamera().GetRaylibCamera();
            Vector2 screenPos = GetWorldToScreen(worldPos, currentCam);

			Vector3 toPoint = Vector3Subtract(worldPos, currentCam.position);
            float forwardDot = Vector3DotProduct(
                Vector3Normalize(Vector3Subtract(currentCam.target, currentCam.position)),
                Vector3Normalize(toPoint)
            );

            if (forwardDot > 0.0f &&
                screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
            {
                float distance = Vector3Length(toPoint);
                float baseSize = 40.0f;
                float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                float size = baseSize * scaleFactor;

                float floatOffset = std::sin(GetTime() * 4.0f) * 4.0f * scaleFactor;
                float centerY = screenPos.y + floatOffset;

                Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                DrawTriangle(topLeft, tip, topRight, YELLOW);
                DrawTriangleLines(topLeft, tip, topRight, ORANGE);
            }
        }

        // PCへの誘導ピンの描画
        if (showPCPin_)
        {
            Vector3 worldPos = securityPC_.GetPosition();
            worldPos.y += 0.8f; // 少し上方に浮かせる

            const Camera3D& currentCam = cameraController_.GetPlayerCamera().GetRaylibCamera();
            Vector2 screenPos = GetWorldToScreen(worldPos, currentCam);

            Vector3 toPoint = Vector3Subtract(worldPos, currentCam.position);
            float forwardDot = Vector3DotProduct(
                Vector3Normalize(Vector3Subtract(currentCam.target, currentCam.position)),
                Vector3Normalize(toPoint)
            );

            if (forwardDot > 0.0f &&
                screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
            {
                float distance = Vector3Length(toPoint);
                float baseSize = 40.0f;
                float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                float size = baseSize * scaleFactor;

                float floatOffset = std::sin(GetTime() * 4.0f) * 4.0f * scaleFactor;
                float centerY = screenPos.y + floatOffset;

                Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                // PCへの誘導は色を変えると分かりやすい（例：シアンや青緑など）
                DrawTriangle(topLeft, tip, topRight, SKYBLUE);
                DrawTriangleLines(topLeft, tip, topRight, BLUE);
            }
        }

        if (showAuthKeyPin_)
        {
            Vector3 worldPos = inventory_.GetAuthenticationKeyPosition();
            worldPos.y += 0.5f; // 少し上方に浮かせる

            const Camera3D& currentCam = cameraController_.GetPlayerCamera().GetRaylibCamera();
            Vector2 screenPos = GetWorldToScreen(worldPos, currentCam);

            Vector3 toPoint = Vector3Subtract(worldPos, currentCam.position);
            float forwardDot = Vector3DotProduct(
                Vector3Normalize(Vector3Subtract(currentCam.target, currentCam.position)),
                Vector3Normalize(toPoint)
            );

            if (forwardDot > 0.0f &&
                screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
            {
                float distance = Vector3Length(toPoint);
                float baseSize = 40.0f;
                float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                float size = baseSize * scaleFactor;

                float floatOffset = std::sin(GetTime() * 4.0f) * 4.0f * scaleFactor;
                float centerY = screenPos.y + floatOffset;

                Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                // 認証キーへの誘導は緑色などの分かりやすい色に設定
                DrawTriangle(topLeft, tip, topRight, GREEN);
                DrawTriangleLines(topLeft, tip, topRight, DARKGREEN);
            }
        }
    }

    gameFade_.Draw();
    EventMgr().Draw();

    if (player_.IsAlive() && IsMessageActive())
    {
        messageWindow_.Draw();
    }
    gunTutorialUI_.Draw();
}

void GameContext::End()
{
    AM().StopAllSe();
    enemyManager_.Clear();
    gunTutorialUI_.End();
    player_.End();
    inventory_.End();
    stage_.End();
    bloodEffect_.Shutdown();
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

void GameContext::DrawBulletMarks() const
{
    constexpr int segments = 16;
    constexpr float outerRadius = 0.075f;
    constexpr float innerRadius = 0.038f;

    for (const BulletMark& mark : bulletMarks_)
    {
        const float fade = Clamp(
            mark.remainingTime / bulletMarkLifetime_,
            0.0f, 1.0f);

        if (fade <= 0.0f)
            continue;

        if (Vector3LengthSqr(mark.normal) < 0.0001f)
            continue;

        const Vector3 normal = Vector3Normalize(mark.normal);

        const Vector3 reference = std::fabs(normal.y) > 0.9f
            ? Vector3{ 1.0f, 0.0f, 0.0f }
        : Vector3{ 0.0f, 1.0f, 0.0f };

        const Vector3 tangent = Vector3Normalize(
            Vector3CrossProduct(reference, normal));
        const Vector3 bitangent = Vector3CrossProduct(normal, tangent);

        auto drawDisc = [&](float radius, float offset, Color color)
            {
                const Vector3 center = Vector3Add(
                    mark.position, Vector3Scale(normal, offset));

                for (int i = 0; i < segments; ++i)
                {
                    const float angleA = 2.0f * PI * i / segments;
                    const float angleB = 2.0f * PI * (i + 1) / segments;

                    const Vector3 pointA = Vector3Add(
                        center,
                        Vector3Add(
                            Vector3Scale(tangent, std::cos(angleA) * radius),
                            Vector3Scale(bitangent, std::sin(angleA) * radius)));

                    const Vector3 pointB = Vector3Add(
                        center,
                        Vector3Add(
                            Vector3Scale(tangent, std::cos(angleB) * radius),
                            Vector3Scale(bitangent, std::sin(angleB) * radius)));

                    DrawTriangle3D(center, pointA, pointB, color);
                }
            };

        drawDisc(outerRadius, 0.008f, Fade(DARKGRAY, 0.65f * fade));
        drawDisc(innerRadius, 0.011f, Fade(BLACK, 0.95f * fade));
    }
}

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
    AM().StopAllSe();
    gameFade_.Reset();
    messageWindow_.Close();
    bloodEffect_.Reset();
    bulletMarks_.clear();
    muzzleFlashTimer_ = hitMarkerTimer_ = tracerAge_ = 0;
    hitMarkerKill_ = tracerActive_ = false;
    checkpointRequested_ = false;
    stage_.SetPosition(data.stagePosition);
    stage_.SetScale(data.stageScale);
    player_.RestoreCheckpoint(data.player);
    inventory_.RestoreCheckpoint(data.inventory);
    inventory_.PlaceOnStage(stage_);
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