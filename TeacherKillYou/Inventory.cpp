#include "Inventory.h"
#include "Stage.h"
#include "raymath.h"

#include <algorithm>
#include <limits>

namespace
{
    constexpr const char* KEY_CARD_MODEL = "Data/Image/Items/Key_Card.glb";
    constexpr const char* MEDKIT_MODEL = "Data/Image/Items/Med_Kit.glb";
    constexpr const char* AMMO_MODEL = "Data/Image/Items/Ammo.glb";

    constexpr const char* KEY_CARD_ICON = "Data/Image/Items/Key_Card.png";
    constexpr const char* MEDKIT_ICON = "Data/Image/Items/Med_Kit.png";
    constexpr const char* HEALING_BAR = "Data/Image/Items/Healing_bar.png";

    constexpr float PICKUP_RANGE = 2.0f;
    constexpr float AIM_RADIUS = 0.30f;
    constexpr int AMMO_PICKUP_AMOUNT = 30;
}

void Inventory::LoadItemModel(ItemModel& destination, const char* path, float targetSize)
{
    if (!FileExists(path))
    {
        TraceLog(LOG_WARNING, "ITEM MODEL NOT FOUND: %s", path);
        return;
    }

    destination.model = LoadModel(path);
    destination.loaded = destination.model.meshCount > 0 &&
        destination.model.meshes != nullptr;

    if (!destination.loaded)
    {
        TraceLog(LOG_WARNING, "ITEM MODEL FAILED TO LOAD: %s", path);
        UnloadModel(destination.model);
        destination.model = {};
        return;
    }

    const BoundingBox bounds = GetModelBoundingBox(destination.model);
    const float w = bounds.max.x - bounds.min.x;
    const float h = bounds.max.y - bounds.min.y;
    const float d = bounds.max.z - bounds.min.z;
    const float largest = (std::max)(w, (std::max)(h, d));
    destination.scale = largest > 0.0001f ? targetSize / largest : 1.0f;
}

void Inventory::UnloadItemModel(ItemModel& destination)
{
    if (destination.loaded)
        UnloadModel(destination.model);
    destination = {};
}

void Inventory::LoadItemIcon(Texture2D& destination, const char* path)
{
    if (!FileExists(path))
    {
        TraceLog(LOG_WARNING, "ITEM PNG NOT FOUND: %s", path);
        return;
    }

    destination = LoadTexture(path);
    if (destination.id == 0)
    {
        TraceLog(LOG_WARNING, "ITEM PNG FAILED TO LOAD: %s", path);
        return;
    }
    SetTextureFilter(destination, TEXTURE_FILTER_BILINEAR);
}

void Inventory::UnloadItemIcon(Texture2D& destination)
{
    if (destination.id != 0)
        UnloadTexture(destination);
    destination = {};
}

void Inventory::Init()
{
    End();

    LoadItemModel(keyCardModel_, KEY_CARD_MODEL, 0.45f);
    LoadItemModel(medkitModel_, MEDKIT_MODEL, 0.50f);
    LoadItemModel(ammoModel_, AMMO_MODEL, 0.40f);

    LoadItemIcon(keyCardIcon_, KEY_CARD_ICON);
    LoadItemIcon(medkitIcon_, MEDKIT_ICON);
    LoadItemIcon(healingBar_, HEALING_BAR);

    Reset();
}

void Inventory::End()
{
    CancelHealing();
    UnloadItemModel(keyCardModel_);
    UnloadItemModel(medkitModel_);
    UnloadItemModel(ammoModel_);
    UnloadItemIcon(keyCardIcon_);
    UnloadItemIcon(medkitIcon_);
    UnloadItemIcon(healingBar_);
}

void Inventory::CancelHealing()
{
    healing_ = false;
    healTimer_ = 0.0f;
}


float Inventory::GetHealingProgress() const
{
    float progress = healTimer_ / HEAL_DURATION;

    if (progress < 0.0f)
        progress = 0.0f;

    if (progress > 1.0f)
        progress = 1.0f;

    return progress;
}

void Inventory::Reset()
{
    slots_.fill(ItemType::None);
    selectedSlot_ = 0;
    focusedPickup_ = -1;
    usedItem_ = ItemType::None;
    pickedUpItem_ = ItemType::None;
    message_.clear();
    messageTimer_ = 0.0f;
    CancelHealing();

    items_.clear();
    // Keep your existing world positions; ImGui can move them as before.
    items_.emplace_back(ItemType::KeyCard, Vector3{ 7.11f, 5.64f, 1.15f });
    items_.emplace_back(ItemType::Medkit, Vector3{ 0.0f, 0.35f, 1.8f });
    items_.emplace_back(ItemType::Ammo, Vector3{ 1.2f, 0.35f, 2.0f });
}

bool Inventory::AddItem(ItemType item)
{
    // Ammo is never stored in inventory; it is handled as a pickup event.
    if (item == ItemType::None || item == ItemType::Ammo)
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

int Inventory::FindFocusedPickup(const Camera3D& camera, Stage* stage) const
{
    float nearest = std::numeric_limits<float>::max();
    int result = -1;

    for (int i = 0; i < static_cast<int>(items_.size()); ++i)
    {
        const Item& item = items_[i];
        float forwardDistance = 0.0f;

        if (!item.IsAimedAt(camera, PICKUP_RANGE, AIM_RADIUS, forwardDistance))
            continue;

        if (stage != nullptr)
        {
            const Vector3 toItem = Vector3Subtract(item.GetPosition(), camera.position);
            const float distance = Vector3Length(toItem);
            if (distance > 0.0001f)
            {
                const Ray ray = { camera.position, Vector3Scale(toItem, 1.0f / distance) };
                const RayCollision hit = stage->Raycast(ray, distance);

                // Block items behind stage walls. Ignore contact near the item origin.
                if (hit.hit && hit.distance < distance - 0.08f)
                    continue;
            }
        }

        if (forwardDistance < nearest)
        {
            nearest = forwardDistance;
            result = i;
        }
    }
    return result;
}

void Inventory::Update(float deltaTime, const Camera3D& camera, Stage* stage)
{
    // One-frame events consumed immediately by GameContext::Update().
    usedItem_ = ItemType::None;
    pickedUpItem_ = ItemType::None;
    messageTimer_ = (std::max)(0.0f, messageTimer_ - deltaTime);

    const int oldSlot = selectedSlot_;
    if (IsKeyPressed(KEY_ONE))   selectedSlot_ = 0;
    if (IsKeyPressed(KEY_TWO))   selectedSlot_ = 1;
    if (IsKeyPressed(KEY_THREE)) selectedSlot_ = 2;
    if (IsKeyPressed(KEY_FOUR))  selectedSlot_ = 3;

    if (selectedSlot_ != oldSlot)
        CancelHealing();

    focusedPickup_ = FindFocusedPickup(camera, stage);

    // Ongoing 3-second hold-to-heal action.
    if (healing_)
    {
        if (!IsKeyDown(KEY_E) ||
            slots_[selectedSlot_] != ItemType::Medkit ||
            focusedPickup_ >= 0)
        {
            // Release E / change slots / aim at another world item => 0%.
            CancelHealing();
            return;
        }

        healTimer_ += (std::max)(0.0f, deltaTime);
        if (healTimer_ >= HEAL_DURATION)
        {
            slots_[selectedSlot_] = ItemType::None;
            usedItem_ = ItemType::Medkit;
            CancelHealing();
            message_ = "HEALING COMPLETE (+50 HP)";
            messageTimer_ = 1.5f;
        }
        return;
    }

    // A fresh key press starts a pickup or medkit action.
    if (!IsKeyPressed(KEY_E))
        return;

    // Priority: picking up world items.
    if (focusedPickup_ >= 0)
    {
        Item& item = items_[focusedPickup_];
        const ItemType type = item.GetType();

        if (type == ItemType::Ammo)
        {
            item.Collect();
            pickedUpItem_ = ItemType::Ammo;
            message_ = TextFormat("+%d AMMO", AMMO_PICKUP_AMOUNT);
        }
        else if (AddItem(type))
        {
            item.Collect();
            pickedUpItem_ = type;
            message_ = std::string("Picked up ") + Item::GetName(type);
        }
        else
        {
            message_ = "Inventory Full!";
        }

        messageTimer_ = 2.0f;
        focusedPickup_ = -1;
        return;
    }

    const ItemType selectedItem = slots_[selectedSlot_];
    if (selectedItem == ItemType::Medkit)
    {
        // Don't consume until E has been held for three whole seconds.
        healing_ = true;
        healTimer_ = 0.0f;
        messageTimer_ = 0.0f;
        return;
    }

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
        usedItem_ = selectedItem;
        slots_[selectedSlot_] = ItemType::None;
        message_ = std::string("Used ") + Item::GetName(selectedItem);
    }
    messageTimer_ = 1.5f;
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

const Texture2D* Inventory::GetItemIcon(ItemType type) const
{
    switch (type)
    {
    case ItemType::KeyCard: return &keyCardIcon_;
    case ItemType::Medkit:  return &medkitIcon_;
    default:                return nullptr;
    }
}

void Inventory::DrawWorld() const
{
    for (const Item& item : items_)
    {
        const ItemModel* model = GetItemModel(item.GetType());
        if (model != nullptr && model->loaded)
            item.Draw(&model->model, model->scale);
    }
}

void Inventory::DrawHealingHUD() const
{
    if (!healing_)
        return;

    const float progress = GetHealingProgress();
    const float hudWidth = 460.0f;   // CHANGE: healing bar width
    const float hudHeight = 54.0f;   // CHANGE: healing bar height
    const float x = (GetScreenWidth() - hudWidth) * 0.5f;
    const float y = GetScreenHeight() - 195.0f; // CHANGE: vertical position

    if (healingBar_.id != 0)
    {
        // Single PNG: the first 12% contains the permanent cross icon.
        const float tw = static_cast<float>(healingBar_.width);
        const float th = static_cast<float>(healingBar_.height);
        const float iconPart = 0.12f;
        const float iconSourceWidth = tw * iconPart;
        const float iconDrawWidth = hudWidth * iconPart;
        const float barSourceWidth = tw - iconSourceWidth;
        const float barDrawWidth = hudWidth - iconDrawWidth;

        // Always show the medical cross.
        DrawTexturePro(healingBar_,
            { 0.0f, 0.0f, iconSourceWidth, th },
            { x, y, iconDrawWidth, hudHeight },
            { 0.0f, 0.0f }, 0.0f, WHITE);

        // Dark/unfilled progress track.
        DrawTexturePro(healingBar_,
            { iconSourceWidth, 0.0f, barSourceWidth, th },
            { x + iconDrawWidth, y, barDrawWidth, hudHeight },
            { 0.0f, 0.0f }, 0.0f, Color{ 85, 85, 85, 135 });

        // Bright green section grows from left to right over 3 seconds.
        if (progress > 0.0f)
        {
            DrawTexturePro(healingBar_,
                { iconSourceWidth, 0.0f, barSourceWidth * progress, th },
                { x + iconDrawWidth, y, barDrawWidth * progress, hudHeight },
                { 0.0f, 0.0f }, 0.0f, WHITE);
        }
    }
    else
    {
        // Fallback if healing_bar.png cannot be found.
        DrawRectangle(static_cast<int>(x), static_cast<int>(y),
            static_cast<int>(hudWidth), 20, DARKGRAY);
        DrawRectangle(static_cast<int>(x), static_cast<int>(y),
            static_cast<int>(hudWidth * progress), 20, GREEN);
    }

    const char* timer = TextFormat("HEALING   %.1f / 3.0s", (std::min)(healTimer_, HEAL_DURATION));
    DrawText(timer,
        static_cast<int>((GetScreenWidth() - MeasureText(timer, 18)) / 2.0f),
        static_cast<int>(y - 24.0f), 18, WHITE);
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
        const Rectangle rect{ (float)x, (float)y, (float)slotWidth, (float)slotHeight };
        const bool selected = (i == selectedSlot_);
        const Color bg = selected ? Color{ 50, 60, 45, 215 } : Color{ 20, 20, 25, 190 };

        DrawRectangleRec(rect, bg);
        DrawRectangleLinesEx(rect, selected ? 3.0f : 1.0f, selected ? LIME : GRAY);
        DrawText(TextFormat("%d", i + 1), x + 7, y + 6, 16, LIGHTGRAY);

        const ItemType type = slots_[i];
        if (type == ItemType::None)
            continue;

        const Texture2D* icon = GetItemIcon(type);
        if (icon != nullptr && icon->id != 0 && icon->width > 0 && icon->height > 0)
        {
            // Preserve PNG aspect ratio; do not stretch the medkit/keycard.
            const float maxWidth = 52.0f; // CHANGE: inventory icon width
            const float maxHeight = 45.0f; // CHANGE: inventory icon height
            const float sx = maxWidth / icon->width;
            const float sy = maxHeight / icon->height;
            const float scale = (std::min)(sx, sy);
            const float width = icon->width * scale;
            const float height = icon->height * scale;

            DrawTexturePro(*icon,
                { 0.0f, 0.0f, (float)icon->width, (float)icon->height },
                { x + (slotWidth - width) / 2.0f,
                  y + 18.0f + (maxHeight - height) / 2.0f,
                  width, height },
                { 0.0f, 0.0f }, 0.0f, WHITE);
        }
        else
        {
            // Fallback for other items / missing PNG files.
            const char* name = Item::GetName(type);
            const int fontSize = 14;
            DrawText(name, x + (slotWidth - MeasureText(name, fontSize)) / 2,
                y + 37, fontSize, WHITE);
        }
    }

    // Context interaction at the crosshair; no interaction at long range.
    if (focusedPickup_ >= 0)
    {
        const ItemType targetType = items_[focusedPickup_].GetType();
        std::string prompt = std::string("[E] PICK UP ") + Item::GetName(targetType);
        if (targetType == ItemType::Ammo)
            prompt += " (+30)";

        const int size = 20;
        const int width = MeasureText(prompt.c_str(), size);
        const int px = (GetScreenWidth() - width) / 2;
        const int py = GetScreenHeight() / 2 + 45;
        DrawRectangle(px - 10, py - 6, width + 20, 32, Color{ 0, 0, 0, 160 });
        DrawText(prompt.c_str(), px, py, size, WHITE);
    }
    else if (!healing_ && slots_[selectedSlot_] != ItemType::None)
    {
        const ItemType selectedItem = slots_[selectedSlot_];
        const char* prompt = nullptr;
        std::string otherPrompt;

        if (selectedItem == ItemType::Medkit)
            prompt = "HOLD [E] TO HEAL (3s)";
        else if (selectedItem == ItemType::KeyCard || selectedItem == ItemType::Key)
            prompt = "Find a locked door";
        else
        {
            otherPrompt = std::string("[E] USE ") + Item::GetName(selectedItem);
            prompt = otherPrompt.c_str();
        }

        const int size = 18;
        DrawText(prompt, (GetScreenWidth() - MeasureText(prompt, size)) / 2,
            y - 35, size, WHITE);
    }

    if (messageTimer_ > 0.0f && !message_.empty())
    {
        const int size = 20;
        DrawText(message_.c_str(),
            (GetScreenWidth() - MeasureText(message_.c_str(), size)) / 2,
            y - 68, size, YELLOW);
    }

    DrawHealingHUD();
}
