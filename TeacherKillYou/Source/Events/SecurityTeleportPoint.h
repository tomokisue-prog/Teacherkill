#pragma once
#include "GameObject.h"
#include "raymath.h"
#include "Inventory.h"

enum class SecurityTeleportType
{
    Source,      // 移動元（ここに入ると警備室へテレポートする）
    Destination  // 移動先（警備室の中）
};

// 警備室テレポートポイントの有効化条件
enum class SecurityTeleportCondition
{
    None,               // 無効
    Normal,             // 通常（常時有効）
    LostItemCollected,  // LostItem回収後に有効
};

class SecurityTeleportPoint : public GameObject
{
public:
    SecurityTeleportPoint() : GameObject(GameObjectType::Stage, "SecurityTeleportPoint") {}
    ~SecurityTeleportPoint() override = default;

    void Init();
    void Reset();
    void Draw() const;

    float GetRadius() const { return radius_; }
    void SetRadius(float r) { radius_ = r; }

    int GetPointNumber() const { return pointNumber_; }
    void SetPointNumber(int num) { pointNumber_ = num; }

    SecurityTeleportType GetTeleportType() const { return type_; }
    void SetTeleportType(SecurityTeleportType type) { type_ = type; }

    SecurityTeleportCondition GetCondition() const { return condition_; }
    void SetCondition(SecurityTeleportCondition cond) { condition_ = cond; }

    bool IsTriggered() const { return triggered_; }
    void SetTriggered(bool triggered) { triggered_ = triggered; }

    // 条件を考慮したエリア判定
    bool CheckPlayerInArea(Vector3 playerPos, const Inventory& inventory) const
    {
        if (triggered_) return false; // 一度入ったら無効

        if (condition_ == SecurityTeleportCondition::None) return false;
        if (condition_ == SecurityTeleportCondition::LostItemCollected && !inventory.IsLostItemCollected())
        {
            return false;
        }

        float dist = Vector3Distance(position_, playerPos);
        return dist <= radius_;
    }

private:
    float radius_{ 2.0f };
    int pointNumber_{ 1 };
    SecurityTeleportType type_{ SecurityTeleportType::Source };
    SecurityTeleportCondition condition_{ SecurityTeleportCondition::Normal };
    bool triggered_{ false }; // 一度テレポートしたら真にする
};