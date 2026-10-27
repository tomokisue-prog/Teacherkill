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
#ifndef NDEBUG
    debugGameplay_.ClearShots();
#endif
    // 新しいラウンドへ、前回の暗転・会話・ドア開閉・ライト状態を持ち越さない。
    gameFade_.Reset();
    messageWindow_.Close();
    door_.Init();
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
    door_.Reset();
    inventory_.Reset();
    bloodEffect_.Reset();
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
    // PlayerのTransformは維持し、足元の高さだけをステージの床へ合わせる。
    player_.SetPosition(stage_.FindGroundPosition(player_.GetPosition()));
    enemyManager_.Clear();
    enemyManager_.Spawn(stage_.FindGroundPosition({ 0.9f, 0.0f, 2.5f }), {});
    demo::EnemyConfig femaleConfig;
    femaleConfig.type = demo::EnemyType::FemaleZombie;
    // 旧マップの寸法を埋め込まず、現行校舎の共通基準から設定する。
    femaleConfig.bodyHeight = WorldScale::Zombie.height;
    femaleConfig.bodyRadius = WorldScale::Zombie.radius;
    enemyManager_.Spawn(stage_.FindGroundPosition({ -0.9f, 0.0f, 2.5f }), femaleConfig);
    paladinPosition_ = stage_.FindGroundPosition({ 0.0f, 0.0f, 3.0f });
    cameraController_.GetPlayerCamera().SetEyeHeight(player_.GetHeight() * WorldScale::EyeHeightRatio);
    cameraController_.GetPlayerCamera().SetPlayerPosition(player_.GetPosition());
    cameraController_.GetPlayerCamera().Update();
}

void GameContext::ProcessPlayerShot()
{
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
#ifndef NDEBUG
    int targetHpBefore = 0;
#endif
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
#ifndef NDEBUG
            targetHpBefore = enemy.GetHp();
#endif
            foundTarget = true;
            impactNormal = inside ? Vector3Negate(ray.direction) : hit.normal;
        }
    }

    // Trigger the visual effects for EVERY shot, even when it misses.
    // The raycast has already found the nearest wall or enemy distance.
    muzzleFlashTimer_ = 0.06f;
    muzzleFlashFrame_ = GetRandomValue(0, 3);

    // The tracer starts at the visible gun muzzle, but aims at the
    // point found by the crosshair's hitscan ray.
    tracerStart_ = player_.GetMuzzlePosition(camera);
    tracerEnd_ = Vector3Add(
        ray.position,
        Vector3Scale(ray.direction, closestDistance));
    tracerAge_ = 0.0f;
    tracerActive_ = true;

    // =====================================
    // Spawn a temporary mark on the wall.
    // No mark if an enemy was hit before the wall.
    // =====================================
    if (wallHit.hit && !foundTarget)
    {
        // Only vertical-ish surfaces
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

#ifndef NDEBUG
    DebugShot debugShot;
    debugShot.ray=ray;debugShot.end=tracerEnd_;
    debugShot.normal=foundTarget?impactNormal:wallHit.hit?wallHit.normal:Vector3{};
    debugShot.target=foundTarget?DebugShotTarget::Enemy:wallHit.hit?DebugShotTarget::Wall:DebugShotTarget::Miss;
    debugShot.enemyId=targetId;
#endif
    if (!foundTarget)
    {
#ifndef NDEBUG
        debugGameplay_.RecordShot(debugShot);
#endif
        TraceLog(LOG_INFO, "SHOT: No enemy hit");
        return;
    }

    const demo::EnemyDamageResult result =
        enemyManager_.ApplyDamage(targetId, shotDamage);
#ifndef NDEBUG
    debugShot.damage=result.applied?targetHpBefore-result.hp:0;
    debugShot.hpAfter=result.hp;debugShot.killed=result.becameDead;
    debugGameplay_.RecordShot(debugShot);
#endif

    if (result.applied)
    {
        // Spawn blood only when the shot hits a living enemy.
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
    // =====================================
    // Update the lifetime of bullet marks.
    // =====================================
    for (BulletMark& mark : bulletMarks_)
    {
        mark.remainingTime -= deltaTime;
    }

    // Remove expired marks
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

    // Keep depth testing, but don't let the effect write depth.
    rlDrawRenderBatchActive();
    rlDisableDepthMask();

    BeginBlendMode(BLEND_ADDITIVE);

    if (tracerActive_ && tracerDuration_ > 0.0f)
    {
        const float progress = Clamp(
            tracerAge_ / tracerDuration_, 0.0f, 1.0f);

        // Show a short moving segment rather than a full laser beam.
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
    if (!player_.IsAlive()) {
        inventory_.CancelHealing();
        return;
    }
    if(IsMessageActive())
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
        // HPが0になったフレームに、ゴール到達やテレポートを処理しない。
        if (!player_.IsAlive()) {
            inventory_.CancelHealing();
            return;
        }

        // ドアとの当たり判定チェック
        BoundingBox doorBox = door_.GetBoundingBox();

        // プレイヤーを高さ方向を考慮した球（あるいはカプセル）に見立てて判定
        Vector3 playerCenter = {
            player_.GetPosition().x,
            player_.GetPosition().y + player_.GetHeight() * 0.5f,
            player_.GetPosition().z
        };

        // Raylib の関数でボックスと球の衝突をチェック
        if (CheckCollisionBoxSphere(doorBox, playerCenter, player_.GetRadius()))
        {
            // 衝突している場合は移動前の位置に巻き戻す（すり抜け防止）
            player_.SetPosition(oldPlayerPos);
        }

        // 1. プレイヤーの移動処理後、テレポート判定を行う
        if (!gameFade_.IsFading())
        {
            for (auto& sourcePt : teleportPoints_)
            {
                if (sourcePt.GetTeleportType() == TeleportType::Source)
                {
                    float dist = Vector3Distance(player_.GetPosition(), sourcePt.GetPosition());
                    if (dist <= sourcePt.GetRadius())
                    {
                        // バイオ風のフェードアウトを開始
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

        float distToGoal = Vector3Distance(player_.GetPosition(), goalPoint_.GetPosition());
        if (distToGoal <= goalPoint_.GetRadius())
        {
            AM().PlaySe(SeId::Confirm); // 必要に応じて効果音
            SM().ChangeScene(SceneID::Goal);
            return; // シーン切り替え時は以降の更新をスキップ
        }
    }


    door_.Update(deltaTime, player_);

    // 2. ワールドオブジェクト（ステージ・敵）の更新
    const BoundingBox doorBounds = door_.GetBoundingBox();
    enemyManager_.Update(deltaTime, player_, stage_, &doorBounds);
    enemyManager_.RemoveExpired();

    // 3. ギズモ編集結果も含め、常に最新の Player 位置を PlayerCamera へ同期

    if (player_.ConsumeDamageImpact())
    {
        cameraController_.GetPlayerCamera().StartDamageImpact();
    }

    // 3. プレイヤー位置をカメラに同期
    cameraController_.GetPlayerCamera().SetEyeHeight(player_.GetHeight() * WorldScale::EyeHeightRatio);
    cameraController_.GetPlayerCamera().SetPlayerPosition(
        player_.GetPosition()
    );

    // 4. カメラ更新
    cameraController_.Update();

    // 5. Inventory: precise pickups, instant reserve ammo and hold-to-heal.
    if (isPlayerCameraActive && player_.IsAlive())
    {
        const Camera3D camera =
            cameraController_.GetPlayerCamera().GetRaylibCamera();

        // Passing the Stage prevents picking up through stage walls.
        inventory_.Update(deltaTime, camera, &stage_);

        // World ammo pickups never occupy an inventory slot.
        if (inventory_.GetPickedUpItem() == ItemType::Ammo)
        {
            player_.AddReserveAmmo(AMMO_PICKUP_AMOUNT);
        }

        // This event occurs only after holding E for 3 uninterrupted seconds.
        if (inventory_.GetUsedItem() == ItemType::Medkit)
        {
            player_.Heal(MEDKIT_HEAL_AMOUNT);
        }
    }
    else
    {
        // Camera switch or death interrupts any ongoing healing.
        inventory_.CancelHealing();
    }
    // 6. プレイヤー射撃処理
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

    // 編集用グリッド
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

        goalPoint_.Draw();
    }

    // プレイヤーカメラ
    const PlayerCamera* activePlayerCam = nullptr;

    if (playerView)
    {
        activePlayerCam =
            &cameraController_.GetPlayerCamera();
    }

    door_.Draw();

    player_.Draw(activePlayerCam);
    enemyManager_.Draw();
    DrawBulletMarks();
    DrawShotEffects();
    inventory_.DrawWorld();
    bloodEffect_.Draw(cameraController_.GetActiveRaylibCamera());
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

        // インベントリHUD表示
        inventory_.DrawHUD();
    }

    gameFade_.Draw();
    EventMgr().Draw();

    if (player_.IsAlive() && IsMessageActive())
    {
        messageWindow_.Draw();
    }

}


void GameContext::End()
{
#ifndef NDEBUG
    debugGameplay_.ClearShots();
#endif
    AM().StopAllSe();
    enemyManager_.Clear();
    player_.End();
    inventory_.End();  // Unload item GLB models
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


// ================================================
// Draw 2-second bullet holes on the stage surface.
// This is geometry only: no texture/model needed.
// ================================================
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

        // Build a local plane parallel to the impact surface.
        const Vector3 reference = std::fabs(normal.y) > 0.9f
            ? Vector3{ 1.0f, 0.0f, 0.0f }
        : Vector3{ 0.0f, 1.0f, 0.0f };

        const Vector3 tangent = Vector3Normalize(
            Vector3CrossProduct(reference, normal));
        const Vector3 bitangent = Vector3CrossProduct(normal, tangent);

        // Slightly offset the discs to prevent wall Z-fighting.
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

        // Outer gray impact edge, followed by a smaller black center.
        drawDisc(outerRadius, 0.008f, Fade(DARKGRAY, 0.65f * fade));
        drawDisc(innerRadius, 0.011f, Fade(BLACK, 0.95f * fade));
    }
}

CheckpointData GameContext::CaptureCheckpoint() const
{
    CheckpointData data;
    data.player = player_.CaptureCheckpoint();
    data.inventory = inventory_.CaptureCheckpoint();
    data.door = door_.CaptureCheckpoint();
    for (const auto& enemy : enemyManager_.GetEnemies())
        if (enemy.IsAlive()) data.enemies.push_back(enemy.CaptureCheckpoint());
    for (const auto& tp : teleportPoints_)
        data.teleports.push_back({{tp.GetPosition(), tp.GetRotation(), tp.GetScale()}, tp.GetPointNumber(), int(tp.GetTeleportType()), tp.GetRadius()});
    data.stagePosition = stage_.GetPosition();
    data.stageScale = stage_.GetScale();
    data.goal = {goalPoint_.GetPosition(), goalPoint_.GetRotation(), goalPoint_.GetScale()};
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
    stage_.SetPosition(data.stagePosition);
    stage_.SetScale(data.stageScale);
    player_.RestoreCheckpoint(data.player);
    inventory_.RestoreCheckpoint(data.inventory);
    inventory_.PlaceOnStage(stage_);
    door_.RestoreCheckpoint(data.door);
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
