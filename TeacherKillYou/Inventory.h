#pragma once

#include "raylib.h"
#include "Item.h"

#include <array>
#include <string>
#include <vector>

class Stage;

class Inventory
{
public:
    void Init();
    void Reset();
    void End();

    void Update(float deltaTime, const Camera3D& camera, Stage* stage = nullptr);
    void DrawWorld() const;
    void DrawHUD() const;

    // Events are valid only on the frame Update() generates them.
    ItemType GetUsedItem() const { return usedItem_; }
    ItemType GetPickedUpItem() const { return pickedUpItem_; }

    void CancelHealing();
    bool IsHealing() const { return healing_; }
    float GetHealingProgress() const;

    // DebugUI / ImGui: continue adjusting item positions directly.
    std::vector<Item>& GetWorldItems() { return items_; }
    const std::vector<Item>& GetWorldItems() const { return items_; }

private:
    static constexpr int MAX_SLOTS = 4;
    static constexpr float HEAL_DURATION = 3.0f;

    struct ItemModel
    {
        Model model{};
        bool loaded = false;
        float scale = 1.0f;
    };

    // GLB world objects.
    ItemModel keyCardModel_{};
    ItemModel medkitModel_{};
    ItemModel ammoModel_{};

    // PNG inventory icons and the ONE healing HUD PNG.
    Texture2D keyCardIcon_{};
    Texture2D medkitIcon_{};
    Texture2D healingBar_{};

    std::array<ItemType, MAX_SLOTS> slots_{};
    std::vector<Item> items_;

    int selectedSlot_ = 0;
    int focusedPickup_ = -1;
    ItemType usedItem_ = ItemType::None;
    ItemType pickedUpItem_ = ItemType::None;

    bool healing_ = false;
    float healTimer_ = 0.0f;

    std::string message_;
    float messageTimer_ = 0.0f;

    bool AddItem(ItemType item);
    int FindFocusedPickup(const Camera3D& camera, Stage* stage) const;
    const ItemModel* GetItemModel(ItemType type) const;
    const Texture2D* GetItemIcon(ItemType type) const;
    void DrawHealingHUD() const;

    static void LoadItemModel(ItemModel& destination, const char* path, float targetSize);
    static void UnloadItemModel(ItemModel& destination);
    static void LoadItemIcon(Texture2D& destination, const char* path);
    static void UnloadItemIcon(Texture2D& destination);
};
