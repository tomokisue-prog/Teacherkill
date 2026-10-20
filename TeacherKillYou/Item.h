#pragma once

#include "raylib.h"

// Keep the old Battery/Key values for compatibility with other game code.
enum class ItemType
{
    None,
    Medkit,
    Battery,
    Key,
    KeyCard,
    Ammo
};

class Item
{
public:
    Item(ItemType type, Vector3 position);

    ItemType GetType() const { return type_; }
    Vector3 GetPosition() const { return position_; }
    void SetPosition(Vector3 position)
    {
        position_ = position;
    }
    bool IsCollected() const { return collected_; }
    void Collect() { collected_ = true; }

    // Actual 3D model only: NO temporary colored cubes.
    void Draw(const Model* model, float scale) const;

    bool IsAimedAt(const Camera3D& camera,
        float pickupRange,
        float aimRadius,
        float& forwardDistance) const;

    static const char* GetName(ItemType type);



private:
    ItemType type_ = ItemType::None;
    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    bool collected_ = false;
};
