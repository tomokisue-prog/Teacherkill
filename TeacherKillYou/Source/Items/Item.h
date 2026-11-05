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
    float GetYaw() const { return yaw_; }
    void SetYaw(float yaw) { yaw_ = yaw; }
    float GetScale() const { return scale_; }
    void SetScale(float scale) { scale_ = scale; }
    bool IsPickup() const { return type_ >= ItemType::Medkit && type_ <= ItemType::LostItem; }

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
    bool collected_{ false };
    float yaw_ = 0.0f;
    float scale_ = 1.0f;
};