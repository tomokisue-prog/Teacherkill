#include "Player.h"
#include "Stage.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "PlayerCamera.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>

void Player::Init() { Reset(); }

void Player::Reset()
{
    position_ = { 0.0f, 4.0f, 0.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
    velocity_ = { 0.0f, 0.0f, 0.0f };
    isGrounded_ = false;
}

void Player::Update(float deltaTime, const Stage& stage, Vector3 forward, Vector3 right)
{
    Vector3 moveDir = GetInputMoveDirection(forward, right);
    if (Vector3Length(moveDir) > 0.0f)
    {
        Vector3 moveAmount = Vector3Scale(Vector3Normalize(moveDir), moveSpeed_ * deltaTime);
        MoveWithCollision(moveAmount, stage);
    }

    UpdateVerticalPhysics(deltaTime, stage);
}

Vector3 Player::GetInputMoveDirection(Vector3 forward, Vector3 right) const
{
    Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
    if (IsKeyDown(KEY_W)) moveDir = Vector3Add(moveDir, forward);
    if (IsKeyDown(KEY_S)) moveDir = Vector3Subtract(moveDir, forward);
    if (IsKeyDown(KEY_A)) moveDir = Vector3Add(moveDir, right);
    if (IsKeyDown(KEY_D)) moveDir = Vector3Subtract(moveDir, right);
    return moveDir;
}

void Player::MoveWithCollision(Vector3 moveAmount, const Stage& stage)
{
    Vector3 rayPos = Vector3Add(position_, { 0.0f, 0.8f, 0.0f });

    if (moveAmount.x != 0.0f)
    {
        Vector3 dir = { moveAmount.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f };
        RayCollision hit = stage.Raycast({ rayPos, dir });
        if (!hit.hit || hit.distance > playerRadius_) position_.x += moveAmount.x;
    }

    if (moveAmount.z != 0.0f)
    {
        Vector3 dir = { 0.0f, 0.0f, moveAmount.z > 0.0f ? 1.0f : -1.0f };
        RayCollision hit = stage.Raycast({ rayPos, dir });
        if (!hit.hit || hit.distance > playerRadius_) position_.z += moveAmount.z;
    }
}

void Player::UpdateVerticalPhysics(float deltaTime, const Stage& stage)
{
    if (IsKeyDown(KEY_SPACE) && isGrounded_)
    {
        velocity_.y = jumpForce_;
        isGrounded_ = false;
    }

    if (!isGrounded_) velocity_.y += gravity_ * deltaTime;
    const float previousY = position_.y;
    position_.y += velocity_.y * deltaTime;
    // 移動前から落下区間を検査し、天井面の誤判定と床のすり抜けを防ぐ。
    constexpr float probeHeight = 0.25f;
    const Ray downRay = { { position_.x, previousY + probeHeight, position_.z }, { 0, -1, 0 } };
    const float probeDistance = probeHeight + (std::max)(0.0f, previousY - position_.y) + 0.01f;
    const RayCollision groundHit = stage.RaycastGround(downRay, probeDistance);
    if (velocity_.y <= 0.0f && groundHit.hit && position_.y <= groundHit.point.y + 0.01f)
    {
        position_.y = groundHit.point.y;
        velocity_.y = 0.0f;
        isGrounded_ = true;
        return;
    }
    isGrounded_ = false;
}

void Player::Draw(const PlayerCamera* camera) const
{
    Model handGun = ResourceManager::GetInstance().GetModel(ResourceKeys::Model_HandGunView);

    if (camera != nullptr)
    {
        // 1. 一人称視点 (FPS) 時の描画計算
        // カメラの位置・正面方向・上下角度(Pitch)に手を完全に追従させる
        
        Vector3 camPos = camera->GetPosition();
        Vector3 forward = camera->GetForwardVector();
        Vector3 right = camera->GetRightVector();
        Vector3 up = camera->GetUpVector();

        // カメラ視点からの手元オフセット位置
        Vector3 pos = Vector3Add(camPos, Vector3Scale(forward, 0.22f));
        pos = Vector3Add(pos, Vector3Scale(right, 0.01f));
        pos = Vector3Subtract(pos, Vector3Scale(up, 0.25f));

        float yaw = atan2f(forward.x, forward.z) * RAD2DEG;

        // FPSカメラの上下・左右に連動させて描画
        DrawModelEx(
            handGun, pos, { 0.0f, 1.0f, 0.0f }, yaw,
            { viewModelScale_, viewModelScale_, viewModelScale_ }, WHITE
        );
    }
    else
    {
        // 2. システムカメラ（デバッグ）時の描画計算
        // ギズモ操作用の Transform (position_, rotation_, scale_) に直接従う
        rlPushMatrix();
        {
            rlTranslatef(position_.x, position_.y, position_.z);
            rlRotatef(rotation_.z, 0.0f, 0.0f, 1.0f);
            rlRotatef(rotation_.x, 1.0f, 0.0f, 0.0f);
            rlRotatef(rotation_.y, 0.0f, 1.0f, 0.0f);

            rlScalef(
                scale_.x * viewModelScale_,
                scale_.y * viewModelScale_,
                scale_.z * viewModelScale_
            );

            for (int i = 0; i < handGun.meshCount; i++)
            {
                DrawMesh(handGun.meshes[i], handGun.materials[handGun.meshMaterial[i]], MatrixIdentity());
            }
        }
        rlPopMatrix();
    }
}

void Player::End() {}