#pragma once

#include "raylib.h"

// アイテムの種別定義
enum class ItemType
{
    None,
    Medkit,
    Battery,
    Key,
    KeyCard,
    Ammo,
    LostItem
};

class Item
{
public:
    Item(ItemType type, Vector3 position);

    // ゲッター / セッター
    ItemType GetType() const { return type_; }
    Vector3 GetPosition() const { return position_; }
    void SetPosition(Vector3 position) { position_ = position; }

    bool IsCollected() const { return collected_; }
    void Collect() { collected_ = true; }

    // 描画 & 判定
    void Draw(const Model* model, float scale) const;
    bool IsAimedAt(const Camera3D& camera, float pickupRange, float aimRadius, float& forwardDistance) const;

    // ユーティリティ
    static const char* GetName(ItemType type);

private:
    ItemType type_{ ItemType::None };
    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    bool collected_{ false };
};