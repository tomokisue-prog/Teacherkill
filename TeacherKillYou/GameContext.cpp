#include "HorrorLighting.h"
#include "GameContext.h"
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

static nlohmann::json Vector3ToJson(const Vector3& v) {
    return nlohmann::json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
}

static Vector3 JsonToVector3(const nlohmann::json& j) {
    return Vector3{
        j.value("x", 0.0f),
        j.value("y", 0.0f),
        j.value("z", 0.0f)
    };
}

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
}

void GameContext::Reset()
{
    AM().StopAllSe();
    player_.Reset();
    stage_.Reset();
    door_.Reset();
    PlaceCharacters();
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

    muzzleFlashTimer_ = 0.0f;
    tracerActive_ = false;
    tracerAge_ = 0.0f;
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
void GameContext::UpdateShotEffects(float deltaTime)
{
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
                                break;
                            }
                        }
                        break;
                    }
                }
            }
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

    if (!playerView)
    {
        for (const auto& tp : teleportPoints_)
        {
            tp.Draw();
        }
    }

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

    // Muzzle flash and bullet tracer must be drawn inside BeginMode3D.
    // Draw after the world geometry so these effects appear correctly.
    DrawShotEffects();

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

    gameFade_.Draw();
}

void GameContext::End()
{
    AM().StopAllSe();
    enemyManager_.Clear();
    player_.End();
    stage_.End();
    lighting_.Shutdown();

    if (muzzleFlashTexture_.id != 0)
    {
        UnloadTexture(muzzleFlashTexture_);
        muzzleFlashTexture_ = {};
    }

    muzzleFlashTimer_ = 0.0f;
    tracerActive_ = false;
}

void GameContext::SaveScene(const std::string& filepath)
{
    nlohmann::json root;

    // 1. Player
    root["player"]["position"] = Vector3ToJson(player_.GetPosition());
    root["player"]["rotation"] = Vector3ToJson(player_.GetRotation());
    root["player"]["scale"] = Vector3ToJson(player_.GetScale());

    // 2. Door
    root["door"]["position"] = Vector3ToJson(door_.GetPosition());
    root["door"]["rotation"] = Vector3ToJson(door_.GetRotation());
    root["door"]["scale"] = Vector3ToJson(door_.GetScale());
    root["door"]["slideSpeed"] = door_.GetSlideSpeed();
    root["door"]["openDistance"] = door_.GetOpenDistance();

    // 3. TeleportPoints
    nlohmann::json tpArray = nlohmann::json::array();
    for (const auto& tp : teleportPoints_)
    {
        nlohmann::json tpJson;
        tpJson["position"] = Vector3ToJson(tp.GetPosition());
        tpJson["rotation"] = Vector3ToJson(tp.GetRotation());
        tpJson["scale"] = Vector3ToJson(tp.GetScale());
        tpJson["pointNumber"] = tp.GetPointNumber();
        tpJson["type"] = static_cast<int>(tp.GetTeleportType());
        tpJson["radius"] = tp.GetRadius();
        tpArray.push_back(tpJson);
    }
    root["teleportPoints"] = tpArray;

    std::ofstream file(filepath);
    if (file.is_open()) {
        file << root.dump(4);
        file.close();
        TraceLog(LOG_INFO, "SCENE: Saved successfully to %s", filepath.c_str());
    }
    else {
        TraceLog(LOG_ERROR, "SCENE: Failed to open file for saving: %s", filepath.c_str());
    }
}

void GameContext::LoadScene(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        TraceLog(LOG_WARNING, "SCENE: Save file not found (%s). Skipping load.", filepath.c_str());
        return;
    }

    nlohmann::json root;
    try {
        file >> root;
        file.close();
    }
    catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "SCENE: Failed to parse JSON: %s", e.what());
        return;
    }

    // 1. Player
    if (root.contains("player")) {
        const auto& p = root["player"];
        if (p.contains("position")) player_.SetPosition(JsonToVector3(p["position"]));
        if (p.contains("rotation")) player_.SetRotation(JsonToVector3(p["rotation"]));
        if (p.contains("scale"))    player_.SetScale(JsonToVector3(p["scale"]));
    }

    // 2. Door
    if (root.contains("door")) {
        const auto& d = root["door"];
        if (d.contains("position"))     door_.SetPosition(JsonToVector3(d["position"]));
        if (d.contains("rotation"))     door_.SetRotation(JsonToVector3(d["rotation"]));
        if (d.contains("scale"))        door_.SetScale(JsonToVector3(d["scale"]));
        if (d.contains("slideSpeed"))   door_.SetSlideSpeed(d["slideSpeed"].get<float>());
        if (d.contains("openDistance")) door_.SetOpenDistance(d["openDistance"].get<float>());
    }

    // 3. TeleportPoints
    if (root.contains("teleportPoints") && root["teleportPoints"].is_array()) {
        teleportPoints_.clear();
        for (const auto& tpJson : root["teleportPoints"]) {
            TeleportPoint tp;
            if (tpJson.contains("position"))    tp.SetPosition(JsonToVector3(tpJson["position"]));
            if (tpJson.contains("rotation"))    tp.SetRotation(JsonToVector3(tpJson["rotation"]));
            if (tpJson.contains("scale"))       tp.SetScale(JsonToVector3(tpJson["scale"]));
            if (tpJson.contains("pointNumber")) tp.SetPointNumber(tpJson["pointNumber"].get<int>());
            if (tpJson.contains("type"))        tp.SetTeleportType(static_cast<TeleportType>(tpJson["type"].get<int>()));
            if (tpJson.contains("radius"))      tp.SetRadius(tpJson["radius"].get<float>());

            teleportPoints_.push_back(tp);
        }
    }

    player_.SetPosition(stage_.FindGroundPosition(player_.GetPosition()));
    TraceLog(LOG_INFO, "SCENE: Loaded successfully from %s", filepath.c_str());
}
