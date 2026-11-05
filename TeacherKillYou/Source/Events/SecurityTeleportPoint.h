#pragma once
#include "GameObject.h"
#include "raymath.h"

enum class SecurityTeleportType
{
    Source,      // 移動元（ここに入ると警備室へテレポートする）
    Destination  // 移動先（警備室の中。ここに入っても何もしない＝一方通行）
};

class SecurityTeleportPoint : public GameObject
{
public:
    SecurityTeleportPoint() : GameObject(GameObjectType::Stage, "SecurityTeleportPoint") {}
    ~SecurityTeleportPoint() override = default;

    void Init();
    void Reset();
    void Draw() const;

    // ゲッター・セッター
    float GetRadius() const { return radius_; }
    void SetRadius(float r) { radius_ = r; }

    int GetPointNumber() const { return pointNumber_; }
    void SetPointNumber(int num) { pointNumber_ = num; }

    SecurityTeleportType GetTeleportType() const { return type_; }
    void SetTeleportType(SecurityTeleportType type) { type_ = type; }

private:
    float radius_{ 2.0f };                      // 判定範囲の半径
    int pointNumber_{ 1 };                      // ペアを識別する番号
    SecurityTeleportType type_{ SecurityTeleportType::Source };
};