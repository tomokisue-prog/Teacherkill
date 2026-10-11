#include "HorrorLighting.h"
#include "GameContext.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "raylib.h"
#include "CameraController.h"
#include "raymath.h"

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

void GameContext::Init()
{
    cameraController_.Init();
    player_.Init();
    stage_.Init();
    PlaceCharacters();
    lighting_.Init(RM().GetModelShaders(), RM().GetModel(ResourceKeys::Model_Stage1));
    hitMarkerTimer_ = 0.0f;
    hitMarkerKill_ = false;
}

void GameContext::Reset()
{
    player_.Reset();
    stage_.Reset();
    door_.Reset();
    PlaceCharacters();
    lighting_.Init(RM().GetModelShaders(), RM().GetModel(ResourceKeys::Model_Stage1));
    hitMarkerTimer_ = 0.0f;
    hitMarkerKill_ = false;
}

void GameContext::PlaceCharacters()
{
    // PlayerのTransformは維持し、足元の高さだけをステージの床へ合わせる。
    player_.SetPosition(stage_.FindGroundPosition(player_.GetPosition()));
    enemyManager_.Clear();
    enemyManager_.Spawn(stage_.FindGroundPosition({ 0.9f, 0.0f, 2.5f }), {});
    paladinPosition_ = stage_.FindGroundPosition({ 0.0f, 0.0f, 3.0f });
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
        }
    }

    if (!foundTarget)
    {
        TraceLog(LOG_INFO, "SHOT: no enemy hit");
        return;
    }

    const demo::EnemyDamageResult result =
        enemyManager_.ApplyDamage(targetId, shotDamage);

    if (result.applied)
    {
        hitMarkerTimer_ = 0.12f;
        hitMarkerKill_ = result.becameDead;

        TraceLog(LOG_INFO, "HIT: enemy HP = %d", result.hp);

        if (result.becameDead)
        {
            TraceLog(LOG_INFO, "Enemy defeated");
        }
    }
}

void GameContext::Update(float deltaTime)
{
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
    }

    door_.Update(deltaTime, player_);

    // 2. ワールドオブジェクト（ステージ・敵）の更新
    const BoundingBox doorBounds = door_.GetBoundingBox();
    enemyManager_.Update(deltaTime, player_, stage_, &doorBounds);
    enemyManager_.RemoveExpired();

    // パラディンモデルのアニメーション更新
    int animCount = 0;
    ModelAnimation* anims = ResourceManager::GetInstance().GetModelAnimations(ResourceKeys::Model_Paladin, &animCount);
    if (anims != nullptr && animCount > 0)
    {
        animFrame_++;
        if (animFrame_ >= anims[animIndex_].frameCount) {
            animFrame_ = 0;
        }
        RM().ApplyModelAnimation(ResourceKeys::Model_Paladin, animFrame_, animIndex_);
    }

    // 3. ギズモ編集結果も含め、常に最新の Player 位置を PlayerCamera へ同期
    cameraController_.GetPlayerCamera().SetPlayerPosition(player_.GetPosition());

    // 4. カメラコントローラーの更新
    cameraController_.Update();
    ProcessPlayerShot();
}

void GameContext::Draw() const
{
    const bool playerView = cameraController_.GetActiveType() == CameraType::Player;
    lighting_.BeginFrame(cameraController_.GetActiveRaylibCamera(),
        cameraController_.GetPlayerCamera().GetRaylibCamera(), stage_.GetPosition(), stage_.GetScale());
    BeginMode3D(cameraController_.GetActiveRaylibCamera());

    // 編集用グリッドはシステムカメラでだけ表示する。
    if (!playerView) DrawGrid(20, 1.0f);
    stage_.Draw();

    // Player Camera がアクティブな時だけカメラポインタを渡し、一人称用に追従させる
    const PlayerCamera* activePlayerCam = nullptr;
    if (cameraController_.GetActiveType() == CameraType::Player)
    {
        activePlayerCam = &cameraController_.GetPlayerCamera();
    }

    door_.Draw();

    player_.Draw(activePlayerCam);
    enemyManager_.Draw();

    Model& paladinModel = ResourceManager::GetInstance().GetModelRef(ResourceKeys::Model_Paladin);
    DrawHorrorModel(paladinModel, paladinPosition_, 1.0f, WHITE);

    EndMode3D();
    lighting_.EndFrame(playerView);

    if (playerView)
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
    }
}

void GameContext::End()
{
    enemyManager_.Clear();
    player_.End();
    stage_.End();
    lighting_.Shutdown();
}