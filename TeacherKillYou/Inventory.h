
#pragma once

#include "raylib.h"
#include "Item.h"

#include <array>
#include <string>
#include <vector>

class Inventory
{
public:
    void Init();
    void Reset();
    void End();

    void Update(float deltaTime, const Camera3D& camera);
    void DrawWorld() const;
    void DrawHUD() const;

    ItemType GetUsedItem() const
    {
        return usedItem_;
    }

    // =====================================
    // DebugUI - World Item Access
    // =====================================
    std::vector<Item>& GetWorldItems()
    {
        return items_;
    }

    const std::vector<Item>& GetWorldItems() const
    {
        return items_;
    }

private:
    static constexpr int MAX_SLOTS = 4;

    struct ItemModel
    {
        Model model{};
        bool loaded = false;
        float scale = 1.0f;
    };

    ItemModel keyCardModel_{};
    ItemModel medkitModel_{};
    ItemModel ammoModel_{};

    std::array<ItemType, MAX_SLOTS> slots_{};
    std::vector<Item> items_;

    int selectedSlot_ = 0;
    int focusedPickup_ = -1;
    ItemType usedItem_ = ItemType::None;

    std::string message_;
    float messageTimer_ = 0.0f;

    bool AddItem(ItemType item);
    int FindFocusedPickup(const Camera3D& camera) const;

    const ItemModel* GetItemModel(ItemType type) const;

    static void LoadItemModel(
        ItemModel& destination,
        const char* glbPath,
        float targetSize);

    static void UnloadItemModel(ItemModel& destination);
};
