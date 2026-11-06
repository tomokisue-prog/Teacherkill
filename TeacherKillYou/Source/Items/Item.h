#pragma once

#include "raylib.h"
#include <cstddef>

// アイテムの種別定義
enum class ItemType
{
    None = 0,
    Medkit = 1,
    Battery = 2,
    Key = 3,
    KeyCard = 4,
    Ammo = 5,
    LostItem = 6,
    Gun = 7,
    AuthenticationKey = 8,
    Books = 9,
    Chair1 = 10,
    Chair2 = 11,
    Computer = 12,
    Locker = 13,
    Whiteboard = 14,
    TrashBin = 15,
    Table = 16,
    HorrorTable = 17,
    SlideDoorProp = 18,
    Count = 19,
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
    bool IsPickup() const;

    Vector3 GetRotation() const { return rotation_; }
    void SetRotation(Vector3 rotation) { rotation_ = rotation; }

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
    Vector3 rotation_{ 0.0f, 0.0f, 0.0f };
    bool collected_{ false };
    float yaw_ = 0.0f;
    float scale_ = 1.0f;
};