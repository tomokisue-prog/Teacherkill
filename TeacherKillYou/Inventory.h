
#pragma once

#include "raylib.h"
#include <array>
#include <string>
#include <vector>

// Types of items
enum class ItemType
{
    None,
    Medkit,
    Battery,
    Key
};

// Item placed in the 3D world
struct WorldPickup
{
    ItemType type = ItemType::None;
    Vector3 position{ 0, 0, 0 };
    bool collected = false;
};

class Inventory
{
public:
    void Reset();

    void Update(float deltaTime, const Camera3D& camera);

    // Call while drawing the 3D world
    void DrawWorld() const;

    // Call while drawing 2D HUD
    void DrawHUD() const;

    // Item used during the current frame
    ItemType GetUsedItem() const { return usedItem_; }

private:
    static constexpr int MAX_SLOTS = 4;

    std::array<ItemType, MAX_SLOTS> slots_{};
    std::vector<WorldPickup> pickups_;

    int selectedSlot_ = 0;
    int focusedPickup_ = -1;

    ItemType usedItem_ = ItemType::None;

    std::string message_;
    float messageTimer_ = 0.0f;

    bool AddItem(ItemType item);
    int FindFocusedPickup(const Camera3D& camera) const;

    static const char* GetItemName(ItemType item);
};
