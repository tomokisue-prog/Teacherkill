#pragma once
#include "raylib.h"
#include <raymath.h>
#include "GameObject.h"

class SecurityPC : public GameObject
{
public:
    SecurityPC();
    ~SecurityPC() override = default;

    void Init();
    void Reset();
    void Update(float deltaTime); 
    void Draw() const;

    Vector3 GetPosition() const { return position_; }

    // プレイヤーとの距離がインタラクト範囲内か判定
    bool CanInteract(const Vector3& playerPos) const {
        return Vector3Distance(position_, playerPos) <= interactRadius_;
    }

    // PCが起動済みかどうか
    bool IsActivated() const { return isActivated_; }
    void SetActivated(bool activated) { isActivated_ = activated; }

private:

    float interactRadius_{ 2.0f }; // 調べられる距離
    bool isActivated_{ false };      // 扉のロック解除済みフラグなど
};