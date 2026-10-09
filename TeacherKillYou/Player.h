#pragma once
#include "GameObject.h"

class Stage;
class PlayerCamera; // 前方宣言

class Player : public GameObject
{
public:
    Player() : GameObject(GameObjectType::Player, "Player") {}
    ~Player() override = default;

    void Init();
    void Reset();
    void Update(float deltaTime, const Stage& stage, Vector3 forward = { 0,0,1 }, Vector3 right = { 1,0,0 });

    // カメラ情報を受け取る Draw 関数
    void Draw(const PlayerCamera* camera = nullptr) const;
    void End();

    float GetRadius() const { return playerRadius_; }
    float GetHeight() const { return playerHeight_; }
    void SetColliderSize(float radius, float height) { playerRadius_ = radius; playerHeight_ = height; }

private:
    Vector3 GetInputMoveDirection(Vector3 forward, Vector3 right) const;
    void MoveWithCollision(Vector3 moveAmount, const Stage& stage);
    void UpdateVerticalPhysics(float deltaTime, const Stage& stage);

private:
    Vector3 velocity_{ 0.0f, 0.0f, 0.0f };
    float moveSpeed_{ 5.0f };

    float playerRadius_{ 0.4f };
    float playerHeight_{ 1.8f };

    float gravity_{ -9.81f };
    float jumpForce_{ 3.0f };

    bool isGrounded_{ false };
    float viewModelScale_{ 0.01f }; // 手のモデルのスケール調整用
};