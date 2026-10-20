#include "Inventory.h"

#include <algorithm>
#include <limits>


namespace
{
    constexpr const char* KEY_CARD_MODEL = "Data/Image/Items/Key_Card.glb";
    constexpr const char* MEDKIT_MODEL = "Data/Image/Items/Med_Kit.glb";
    constexpr const char* AMMO_MODEL = "Data/Image/Items/Ammo.glb";
}

void Inventory::LoadItemModel(ItemModel& destination,
    const char* glbPath,
    float targetSize)
{
    if (!FileExists(glbPath))
    {
        TraceLog(LOG_WARNING, "ITEM MODEL NOT FOUND: %s", glbPath);
        TraceLog(LOG_INFO, "Current directory: %s", GetWorkingDirectory());
        return;
    }

    destination.model = LoadModel(glbPath);
    destination.loaded = destination.model.meshCount > 0 &&
        destination.model.meshes != nullptr;

    if (!destination.loaded)
    {
        TraceLog(LOG_WARNING, "ITEM MODEL FAILED TO LOAD: %s", glbPath);
        UnloadModel(destination.model);
        destination.model = {};
        return;
    }

    // Scale each imported mesh to approximately the same world size.
    const BoundingBox bounds = GetModelBoundingBox(destination.model);
    const float width = bounds.max.x - bounds.min.x;
    const float height = bounds.max.y - bounds.min.y;
    const float depth = bounds.max.z - bounds.min.z;
    const float largestDimension = (std::max)(width, (std::max)(height, depth));

    destination.scale = largestDimension > 0.0001f
        ? targetSize / largestDimension
        : 1.0f;

    TraceLog(LOG_INFO, "Loaded item model: %s (scale %f)",
        glbPath, destination.scale);
}

void Inventory::UnloadItemModel(ItemModel& destination)
{
    if (destination.loaded)
        UnloadModel(destination.model);

    destination = {};
}

void Inventory::Init()
{
    End();

    LoadItemModel(keyCardModel_, KEY_CARD_MODEL, 0.45f);
    LoadItemModel(medkitModel_, MEDKIT_MODEL, 0.50f);
    LoadItemModel(ammoModel_, AMMO_MODEL, 0.40f);

    Reset();
}

void Inventory::End()
{
    UnloadItemModel(keyCardModel_);
    UnloadItemModel(medkitModel_);
    UnloadItemModel(ammoModel_);
}

void Inventory::Reset()
{
    slots_.fill(ItemType::None);
    selectedSlot_ = 0;
    focusedPickup_ = -1;
    usedItem_ = ItemType::None;
    message_.clear();
    messageTimer_ = 0.0f;

    items_.clear();

    // World-space positions. Adjust these to match your school map.
    items_.emplace_back(ItemType::KeyCard, Vector3{ 7.11f, 5.64f, 1.15f });
    items_.emplace_back(ItemType::Medkit, Vector3{ 0.0f, 0.35f, 1.8f });
    items_.emplace_back(ItemType::Ammo, Vector3{ 1.2f, 0.35f, 2.0f });
}

bool Inventory::AddItem(ItemType item)
{
    if (item == ItemType::None)
        return false;

    for (ItemType& slot : slots_)
    {
        if (slot == ItemType::None)
        {
            slot = item;
            return true;
        }
    }

    return false;
}

int Inventory::FindFocusedPickup(const Camera3D& camera) const
{
    constexpr float pickupRange = 2.5f;
    constexpr float aimRadius = 0.55f;

    float nearest = std::numeric_limits<float>::max();
    int result = -1;

    for (int i = 0; i < static_cast<int>(items_.size()); ++i)
    {
        float forwardDistance = 0.0f;
        if (!items_[i].IsAimedAt(camera, pickupRange,
            aimRadius, forwardDistance))
            continue;

        if (forwardDistance < nearest)
        {
            nearest = forwardDistance;
            result = i;
        }
    }

    return result;
}

void Inventory::Update(float deltaTime, const Camera3D& camera)
{
    // The GameContext must read this event in the same frame.
    usedItem_ = ItemType::None;

    if (messageTimer_ > 0.0f)
        messageTimer_ -= deltaTime;

    if (IsKeyPressed(KEY_ONE))   selectedSlot_ = 0;
    if (IsKeyPressed(KEY_TWO))   selectedSlot_ = 1;
    if (IsKeyPressed(KEY_THREE)) selectedSlot_ = 2;
    if (IsKeyPressed(KEY_FOUR))  selectedSlot_ = 3;

    focusedPickup_ = FindFocusedPickup(camera);

    if (!IsKeyPressed(KEY_E))
        return;

    // Priority 1: Pick up the item being aimed at.
    if (focusedPickup_ >= 0)
    {
        Item& item = items_[focusedPickup_];
        if (AddItem(item.GetType()))
        {
            message_ = std::string("Picked up ") + Item::GetName(item.GetType());
            item.Collect();
            focusedPickup_ = -1;
        }
        else
        {
            message_ = "Inventory Full!";
        }
        messageTimer_ = 2.0f;
        return;
    }

    // Priority 2: Use the selected inventory item.
    const ItemType selectedItem = slots_[selectedSlot_];

    if (selectedItem == ItemType::None)
    {
        message_ = "Empty Slot";
    }
    else if (selectedItem == ItemType::KeyCard || selectedItem == ItemType::Key)
    {
        message_ = "Find a locked door";
    }
    else
    {
        // Consumable event only; HP/ammo changes need GameContext integration.
        usedItem_ = selectedItem;
        slots_[selectedSlot_] = ItemType::None;
        message_ = std::string("Used ") + Item::GetName(selectedItem);
    }

    messageTimer_ = 2.0f;
}

const Inventory::ItemModel* Inventory::GetItemModel(ItemType type) const
{
    switch (type)
    {
    case ItemType::KeyCard: return &keyCardModel_;
    case ItemType::Medkit:  return &medkitModel_;
    case ItemType::Ammo:    return &ammoModel_;
    default:                return nullptr;
    }
}

void Inventory::DrawWorld() const
{
    for (const Item& item : items_)
    {
        const ItemModel* asset = GetItemModel(item.GetType());
        if (asset != nullptr && asset->loaded)
            item.Draw(&asset->model, asset->scale);
    }
}

void Inventory::DrawHUD() const
{
    const int slotWidth = 80;
    const int slotHeight = 70;
    const int gap = 8;
    const int totalWidth = MAX_SLOTS * slotWidth + (MAX_SLOTS - 1) * gap;
    const int startX = (GetScreenWidth() - totalWidth) / 2;
    const int y = GetScreenHeight() - slotHeight - 25;

    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        const int x = startX + i * (slotWidth + gap);
        const Rectangle rect{
            static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(slotWidth), static_cast<float>(slotHeight)
        };
        const bool selected = (i == selectedSlot_);
        const Color bg = selected
            ? Color{ 50, 60, 45, 215 }
        : Color{ 20, 20, 25, 190 };

        DrawRectangleRec(rect, bg);
        DrawRectangleLinesEx(rect, selected ? 3.0f : 1.0f,
            selected ? LIME : GRAY);
        DrawText(TextFormat("%d", i + 1), x + 7, y + 6, 16, LIGHTGRAY);

        const char* name = Item::GetName(slots_[i]);
        const int fontSize = 14;
        const int width = MeasureText(name, fontSize);
        DrawText(name, x + (slotWidth - width) / 2, y + 37,
            fontSize, slots_[i] == ItemType::None ? GRAY : WHITE);
    }

    std::string prompt;
    if (focusedPickup_ >= 0)
    {
        prompt = std::string("[E] Pick up ") +
            Item::GetName(items_[focusedPickup_].GetType());
    }
    else if (slots_[selectedSlot_] != ItemType::None)
    {
        prompt = std::string("[E] Use ") +
            Item::GetName(slots_[selectedSlot_]);
    }

    if (!prompt.empty())
    {
        const int fontSize = 20;
        DrawText(prompt.c_str(),
            (GetScreenWidth() - MeasureText(prompt.c_str(), fontSize)) / 2,
            y - 35, fontSize, WHITE);
    }

    if (messageTimer_ > 0.0f && !message_.empty())
    {
        DrawText(message_.c_str(),
            (GetScreenWidth() - MeasureText(message_.c_str(), 20)) / 2,
            y - 68, 20, YELLOW);
    }
}
