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
    void End(); // Release the inventory slot texture.

    // --- 更新 & 描画 ---
    void Update(float deltaTime, const Camera3D& camera, Stage* stage = nullptr);
    void DrawWorld(bool editing = false) const;
    void DrawHUD() const;

    // --- 回復アクション制御 ---
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

private:
    static constexpr int MAX_SLOTS = 4;
    static constexpr float HEAL_DURATION = 3.0f;

    // インベントリスロット & ワールド設置アイテム
    Texture2D slotBorder_{};
    std::array<ItemType, MAX_SLOTS> slots_{};
    std::vector<Item> items_;
    std::vector<Item> placements_;
    CameraController cameraController_;

    Camera3D lastCamera_{};

    // 選択・フォーカス状態
    int selectedSlot_{ 0 };
    int focusedPickup_{ -1 };
    ItemType usedItem_{ ItemType::None };
    ItemType pickedUpItem_{ ItemType::None };

    // 回復タイマー状態
    bool healing_{ false };
    float healTimer_{ 0.0f };

    // HUD用通知メッセージ
    std::string message_;
    float messageTimer_{ 0.0f };

    // --- 内部ヘルパー関数 ---
    bool AddItem(ItemType item);
    int FindFocusedPickup(const Camera3D& camera, Stage* stage) const;

    // リソース連携用アクセサ
    const Texture2D* GetItemIcon(ItemType type) const;

    void DrawHealingHUD() const;
};