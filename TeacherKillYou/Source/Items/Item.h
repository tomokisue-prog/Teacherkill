#pragma once

#include "raylib.h"
#include <cstddef>

// アイテムの種別定義
enum class ItemType
{
    None,
    Medkit,
    Battery,
    Key,
    KeyCard,
    Ammo,
    LostItem,
    Gun,
    AuthenticationKey,
    Books,
    Chair1,
    Chair2,
    Computer,
    Locker,
    Whiteboard,
    TrashBin,
    Table,
    HorrorTable,
    SlideDoorProp,
    Count
};

inline constexpr std::size_t MaxItemPlacements = 1024;

class Item
{
public:
    Item(ItemType type, Vector3 position);

    // ゲッター / セッター
    ItemType GetType() const { return type_; }
    void SetType(ItemType type) { type_ = type; }
    Vector3 GetPosition() const { return position_; }
    void SetPosition(Vector3 position) { position_ = position; }

    Vector3 GetRotation() const { return rotation_; }
    void SetRotation(Vector3 rotation) {
        rotation_ = rotation;
        yaw_ = rotation.y * DEG2RAD;
    }

    float GetYaw() const { return rotation_.y * DEG2RAD; }
    void SetYaw(float yaw) {
        yaw_ = yaw;
        rotation_.y = yaw * RAD2DEG;
    }

    float GetScale() const { return scale_; }
    void SetScale(float scale) { scale_ = scale; }

    bool IsPickup() const {
        switch (type_) {
        case ItemType::Medkit:
        case ItemType::Battery:
        case ItemType::Key:
        case ItemType::KeyCard:
        case ItemType::Ammo:
        case ItemType::LostItem:
        case ItemType::Gun:
        case ItemType::AuthenticationKey:
            return true;
        default:
            return false;
        }
    }

    bool IsCollected() const { return collected_; }
    void Collect() { collected_ = true; }

    // 描画 & 判定
    void Draw() const;
    RayCollision RaycastPlacement(Ray ray) const;
    bool IsAimedAt(const Camera3D& camera, float pickupRange, float aimRadius, float& forwardDistance) const;

    // ユーティリティ
    static const char* GetName(ItemType type);

private:
    ItemType type_{ ItemType::None };
    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f }; // 度数法 (X, Y, Z)
    bool collected_{ false };
    float yaw_ = 0.0f;
    float scale_ = 1.0f;
};