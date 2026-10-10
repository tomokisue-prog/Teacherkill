#include "GameContext.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "raylib.h"
#include "CameraController.h"

void GameContext::Init()
{
    cameraController_.Init();
    player_.Init();
    stage_.Init();
    PlaceCharacters();
    lighting_.Init(RM().GetModelShaders());
}

void GameContext::Reset()
{
    player_.Reset();
    stage_.Reset();
    door_.Reset();
    PlaceCharacters();
    lighting_.Init(RM().GetModelShaders());
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

void GameContext::Update(float deltaTime)
{
    lighting_.Update();
    bool isPlayerCameraActive = (cameraController_.GetActiveType() == CameraType::Player);

    // 1. プレイヤーの移動入力更新（プレイヤーカメラ使用時のみ移動入力受付）
    if (isPlayerCameraActive)
    {
        Vector3 forward = cameraController_.GetPlayerCamera().GetForwardVector();
        Vector3 right = cameraController_.GetPlayerCamera().GetRightVector();
        player_.Update(deltaTime, stage_, forward, right);
    }

    door_.Update(deltaTime, player_);

    // 2. ワールドオブジェクト（ステージ・敵）の更新
    enemyManager_.Update(deltaTime, player_, stage_);
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
}

void GameContext::Draw() const
{
    const bool playerView = cameraController_.GetActiveType() == CameraType::Player;
    lighting_.BeginFrame(cameraController_.GetActiveRaylibCamera(),
        cameraController_.GetPlayerCamera().GetRaylibCamera());
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
    DrawModel(paladinModel, paladinPosition_, 1.0f, WHITE);

    EndMode3D();
    lighting_.EndFrame(playerView);
}

void GameContext::End()
{
    enemyManager_.Clear();
    player_.End();
    stage_.End();
    lighting_.Shutdown();
}