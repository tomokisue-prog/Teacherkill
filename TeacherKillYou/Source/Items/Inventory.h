#pragma once

#include "raylib.h"
#include "Item.h"
#include "CameraController.h"
#include <array>
#include <string>
#include <vector>

class Stage;
struct InventoryCheckpoint;

class Inventory
{
public:
    Inventory() = default;
    ~Inventory() = default;

    // --- 初期化 / 破棄 / リセット ---
    void Init(const Stage& stage);
    void Reset();
    void ApplyPlacements();
    void End();

    // --- 更新 & 描画 ---
    void Update(float deltaTime, const Camera3D& camera, Stage* stage = nullptr);
    void DrawWorld(bool editing = false) const;
    void DrawHUD() const;

    // --- 回復アクション関連 ---
    void CancelHealing();
    bool IsHealing() const { return healing_; }
    float GetHealingProgress() const;

    // --- フレームごとのイベント参照 ---
    ItemType GetUsedItem() const { return usedItem_; }
    ItemType GetPickedUpItem() const { return pickedUpItem_; }

    // --- チェックポイント（セーブ / ロード） ---
    InventoryCheckpoint CaptureCheckpoint() const;
    void RestoreCheckpoint(const InventoryCheckpoint& data);

    // --- ワールド内アイテム参照（DebugUI / ギズモ編集用） ---
    std::vector<Item>& GetWorldItems() { return items_; }
    const std::vector<Item>& GetWorldItems() const { return items_; }
    std::vector<Item>& GetPlacements() { return placements_; }
    const std::vector<Item>& GetPlacements() const { return placements_; }

    bool IsLostItemCollected() const;
    bool IsGunCollected() const;
    bool IsAuthenticationKeyCollected() const;

    // 非常階段キー関連
    bool IsEmergencyStairsKeyCollected() const
    {
        for (const auto& item : items_)
        {
            if (item.GetType() == ItemType::EmergencyStairsKey && item.IsCollected())
            {
                return true;
            }
        }
        return false;
    }

    bool HasEmergencyStairsKey() const
    {
        for (const auto& slot : slots_)
        {
            if (slot == ItemType::EmergencyStairsKey) return true;
        }
        return IsEmergencyStairsKeyCollected();
    }

    Vector3 GetGunPosition() const;
    Vector3 GetAuthenticationKeyPosition() const;

    // 非常階段のキーの位置取得
    Vector3 GetEmergencyStairsKeyPosition() const
    {
        for (const auto& item : items_)
        {
            if (item.GetType() == ItemType::EmergencyStairsKey)
            {
                return item.GetPosition();
            }
        }
        return { 0.0f, 0.0f, 0.0f };
    }

private:
    static constexpr int MAX_SLOTS = 4;
    static constexpr float HEAL_DURATION = 3.0f;

    Texture2D slotBorder_{};
    std::array<ItemType, MAX_SLOTS> slots_{};
    std::vector<Item> items_;
    std::vector<Item> placements_;
    CameraController cameraController_;

    Camera3D lastCamera_{};

    int selectedSlot_{ 0 };
    int focusedPickup_{ -1 };
    ItemType usedItem_{ ItemType::None };
    ItemType pickedUpItem_{ ItemType::None };

    bool healing_{ false };
    float healTimer_{ 0.0f };

    std::string message_;
    float messageTimer_{ 0.0f };

    bool AddItem(ItemType item);
    int FindFocusedPickup(const Camera3D& camera, Stage* stage) const;

    const Texture2D* GetItemIcon(ItemType type) const;

    void DrawHealingHUD() const;
};