#include "Inventory.h"
#include "ItemCatalog.h"
#include "AudioManager.h"
#include "CheckpointData.h"
#include "Stage.h"
#include "WorldScale.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "raymath.h"

#include <algorithm>
#include <limits>
#include <cmath>

namespace
{
    const float PICKUP_RANGE = WorldScale::Player.height * 1.1f;
    constexpr float AIM_RADIUS = 0.30f;
}

void Inventory::Init(const Stage& stage)
{
    End();
    placements_ = {
        {ItemType::LostItem, {5.0f, 0.35f, 5.0f}},
        {ItemType::KeyCard, {7.11f, 5.64f, 1.15f}},
        {ItemType::Medkit, {0.0f, 0.35f, 1.8f}},
        {ItemType::Ammo, {1.2f, 0.35f, 2.0f}},
        {ItemType::Gun, {2.0f, 0.35f, 2.0f}},
        {ItemType::AuthenticationKey, {3.0f, 0.35f, -2.0f}}
    };
    for (auto& item : placements_) {
        const auto position = item.GetPosition();
        const auto floor = stage.FindGroundPosition(position);
        if (position.y < floor.y + .08f)
            item.SetPosition({ position.x, floor.y + .2f, position.z });
    }
    Reset();
    const char* path = "Data/Image/UI/Inventory.png";
    if (FileExists(path))
    {
        slotBorder_ = LoadTexture(path);
        if (slotBorder_.id != 0) SetTextureFilter(slotBorder_, TEXTURE_FILTER_BILINEAR);
    }
    else TraceLog(LOG_WARNING, "INVENTORY: Slot image not found: %s", path);
}

void Inventory::End()
{
    if (slotBorder_.id != 0) UnloadTexture(slotBorder_);
    slotBorder_ = {};
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

    ApplyPlacements();
}

void Inventory::ApplyPlacements()
{
    items_ = placements_;
    focusedPickup_ = -1;
    usedItem_ = pickedUpItem_ = ItemType::None;
}

void Inventory::CancelHealing()
{
    if (healing_) AM().StopSe(SeId::Healing);
    healing_ = false;
    healTimer_ = 0.0f;
}

float Inventory::GetHealingProgress() const
{
    const float progress = healTimer_ / HEAL_DURATION;
    return Clamp(progress, 0.0f, 1.0f);
}

bool Inventory::AddItem(ItemType item)
{
    if (item < ItemType::Medkit || item > ItemType::LostItem || item == ItemType::Ammo)
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

int Inventory::FindFocusedPickup(const Camera3D& camera, Stage* stage, ItemType blockedPickup) const
{
    float nearest = std::numeric_limits<float>::max();
    int result = -1;

    for (int i = 0; i < static_cast<int>(items_.size()); ++i)
    {
        const Item& item = items_[i];
        if (item.GetType() == blockedPickup) continue;
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

void Inventory::Update(float deltaTime, const Camera3D& camera, Stage* stage, ItemType blockedPickup)
{
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

    focusedPickup_ = FindFocusedPickup(camera, stage, blockedPickup);

    // 回復ホールド処理
    if (healing_ && focusedPickup_ < 0)
    {
        if (!IsKeyDown(KEY_E) || slots_[selectedSlot_] != ItemType::Medkit)
        {
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

    // フィールド上のアイテムを拾う
    if (focusedPickup_ >= 0)
    {
        if (healing_) CancelHealing();
        if (!IsKeyPressed(KEY_F))
            return;

        Item& item = items_[focusedPickup_];
        const ItemType type = item.GetType();

        if (type == ItemType::Ammo)
        {
            item.Collect();
            pickedUpItem_ = type;
            AM().PlaySe(SeId::Pickup);
            message_ = TextFormat("+%d AMMO", AmmoPickupAmount);
        }
        else if (type == ItemType::LostItem || type == ItemType::Gun || type == ItemType::AuthenticationKey || type == ItemType::EmergencyStairsKey || type == ItemType::Crowbar)
        {
            item.Collect();
            pickedUpItem_ = type;
            AM().PlaySe(SeId::Pickup);

            if (type == ItemType::Gun) message_ = "Picked up GUN";
            else if (type == ItemType::AuthenticationKey) message_ = "Picked up AUTHENTICATION KEY";
            else if (type == ItemType::EmergencyStairsKey) message_ = "Picked up SERVICE ELEVATOR KEY";
            else if (type == ItemType::Crowbar) message_ = "Picked up CROWBAR";
            else message_ = "Picked up LOST ITEM";
        }
        else if (AddItem(type))
        {
            item.Collect();
            pickedUpItem_ = type;
            AM().PlaySe(SeId::Pickup);
            message_ = std::string("Picked up ") + Item::GetName(type);
        }
        else
        {
            AM().PlaySe(SeId::Error);
            message_ = "Inventory Full!";
        }

        messageTimer_ = 2.0f;
        focusedPickup_ = -1;
        return;
    }

    if (!IsKeyPressed(KEY_E))
        return;

    // スロット内アイテムの使用
    const ItemType selectedItem = slots_[selectedSlot_];
    if (selectedItem == ItemType::Medkit)
    {
        healing_ = true;
        AM().PlaySe(SeId::Healing);
        healTimer_ = 0.0f;
        messageTimer_ = 0.0f;
        return;
    }

    if (selectedItem == ItemType::KeyCard || selectedItem == ItemType::Key) return;
    if (selectedItem == ItemType::None)
    {
        AM().PlaySe(SeId::Error);
        message_ = "Empty Slot";
    }
    else
    {
        usedItem_ = selectedItem;
        slots_[selectedSlot_] = ItemType::None;
        message_ = std::string("Used ") + Item::GetName(selectedItem);
    }
    messageTimer_ = 1.5f;
}

const Texture2D* Inventory::GetItemIcon(ItemType type) const
{
    switch (type)
    {
    case ItemType::KeyCard:  return RM().GetTexture(ResourceKeys::Icon_KeyCard);
    case ItemType::Medkit:   return RM().GetTexture(ResourceKeys::Icon_Medkit);
    default:                 return nullptr;
    }
}

void Inventory::DrawWorld(bool editing) const
{
    for (const auto& item : editing ? placements_ : items_)
        item.Draw();
}

void Inventory::DrawHealingHUD() const
{
    if (!healing_)
        return;

    const float progress = GetHealingProgress();
    const float uiScale = Clamp((std::min)(GetScreenWidth() / 1280.0f,
        GetScreenHeight() / 720.0f), 0.5f, 1.5f);
    const float hudWidth = (std::min)(600.0f * uiScale, GetScreenWidth() - 20.0f * uiScale);
    const float hudHeight = 144.0f * uiScale;
    const float x = (GetScreenWidth() - hudWidth) * 0.5f;
    const float slotHeight = 88.0f * uiScale * 586.0f / 606.0f;
    const float inventoryTop = GetScreenHeight() - slotHeight - 20.0f * uiScale;
    const float y = (std::max)(0.0f, inventoryTop - hudHeight - 12.0f * uiScale);

    const Texture2D* healingBarPtr = RM().GetTexture(ResourceKeys::Icon_HealingBar);
    if (healingBarPtr != nullptr && healingBarPtr->id != 0)
    {
        const Texture2D& healingBar = *healingBarPtr;
        const float tw = static_cast<float>(healingBar.width);
        const float th = static_cast<float>(healingBar.height);
        const float iconPart = 0.12f;
        const float iconSourceWidth = tw * iconPart;
        const float iconDrawWidth = hudWidth * iconPart;
        const float barSourceWidth = tw - iconSourceWidth;
        const float barDrawWidth = hudWidth - iconDrawWidth;

        DrawTexturePro(healingBar,
            { 0.0f, 0.0f, iconSourceWidth, th },
            { x, y, iconDrawWidth, hudHeight },
            { 0.0f, 0.0f }, 0.0f, WHITE);

        DrawTexturePro(healingBar,
            { iconSourceWidth, 0.0f, barSourceWidth, th },
            { x + iconDrawWidth, y, barDrawWidth, hudHeight },
            { 0.0f, 0.0f }, 0.0f, Color{ 85, 85, 85, 135 });

        if (progress > 0.0f)
        {
            DrawTexturePro(healingBar,
                { iconSourceWidth, 0.0f, barSourceWidth * progress, th },
                { x + iconDrawWidth, y, barDrawWidth * progress, hudHeight },
                { 0.0f, 0.0f }, 0.0f, WHITE);
        }
    }
    else
    {
        DrawRectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(hudWidth), 20, DARKGRAY);
        DrawRectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(hudWidth * progress), 20, GREEN);
    }
}

void Inventory::DrawHUD() const
{
    const float uiScale = Clamp((std::min)(GetScreenWidth() / 1280.0f,
        GetScreenHeight() / 720.0f), 0.5f, 1.5f);
    const int slotWidth = static_cast<int>(88.0f * uiScale);
    const int slotHeight = static_cast<int>(slotWidth * 586.0f / 606.0f);
    const int gap = static_cast<int>(6.0f * uiScale);
    const int totalWidth = MAX_SLOTS * slotWidth + (MAX_SLOTS - 1) * gap;
    const int startX = (GetScreenWidth() - totalWidth) / 2;
    const int y = GetScreenHeight() - slotHeight - static_cast<int>(20.0f * uiScale);

    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        const int x = startX + i * (slotWidth + gap);
        const Rectangle rect{ static_cast<float>(x), static_cast<float>(y),
                              static_cast<float>(slotWidth), static_cast<float>(slotHeight) };
        const bool selected = (i == selectedSlot_);
        const float centerX = x + slotWidth * 0.478f;
        const float centerY = y + slotHeight * 0.590f;

        if (slotBorder_.id != 0)
        {
            const Rectangle source{ 0, 0, static_cast<float>(slotBorder_.width),
                                          static_cast<float>(slotBorder_.height) };
            const Color tint = selected ? WHITE : Color{ 205, 205, 205, 230 };
            DrawTexturePro(slotBorder_, source, rect, { 0, 0 }, 0.0f, tint);
            if (selected)
                DrawCircleLines(static_cast<int>(centerX), static_cast<int>(centerY),
                    slotWidth * 0.235f, Color{ 225, 195, 150, 230 });
        }
        else
        {
            DrawRectangleRec(rect, selected ? Color{ 50, 60, 45, 215 } : Color{ 20, 20, 25, 190 });
            DrawRectangleLinesEx(rect, selected ? 3.0f : 1.0f, selected ? LIME : GRAY);
        }
        const ItemType type = slots_[i];
        if (type == ItemType::None) continue;

        const Texture2D* icon = GetItemIcon(type);
        if (icon != nullptr && icon->id != 0 && icon->width > 0 && icon->height > 0)
        {
            const float maxWidth = 42.0f * uiScale;
            const float maxHeight = 40.0f * uiScale;
            const float scale = (std::min)(maxWidth / icon->width, maxHeight / icon->height);
            const float width = icon->width * scale;
            const float height = icon->height * scale;
            DrawTexturePro(*icon,
                { 0.0f, 0.0f, static_cast<float>(icon->width), static_cast<float>(icon->height) },
                { centerX - width * 0.5f, centerY - height * 0.5f, width, height },
                { 0.0f, 0.0f }, 0.0f, WHITE);
        }
        else
        {
            const char* name = Item::GetName(type);
            const int fontSize = (std::max)(9, static_cast<int>(11.0f * uiScale));
            DrawText(name, static_cast<int>(centerX) - MeasureText(name, fontSize) / 2,
                static_cast<int>(centerY) - fontSize / 2, fontSize, WHITE);
        }
    }

    if (focusedPickup_ >= 0)
    {
        const ItemType targetType = items_[focusedPickup_].GetType();
        const auto& font = JPFont();
        const bool useJapanese = font.IsLoaded();
        const auto& item = GetItemDefinition(targetType);
        std::string prompt = useJapanese ? std::string("[F] ") + item.displayName + u8"\u3092\u62fe\u3046"
                                         : std::string("[F] PICK UP ") + item.name;
        if (targetType == ItemType::Ammo)
            prompt += " (+30)";

        const int size = 20;
        const int width = useJapanese ? int(font.MeasureText(prompt, size).x) : MeasureText(prompt.c_str(), size);
        const int px = (GetScreenWidth() - width) / 2;
        const int py = GetScreenHeight() / 2 + 76;
        DrawRectangle(px - 10, py - 6, width + 20, 32, Color{ 0, 0, 0, 160 });
        if (useJapanese) font.DrawText(prompt, px, py, size, WHITE);
        else DrawText(prompt.c_str(), px, py, size, WHITE);
    }
    else if (!healing_ && slots_[selectedSlot_] != ItemType::None &&
        slots_[selectedSlot_] != ItemType::KeyCard && slots_[selectedSlot_] != ItemType::Key)
    {
        const ItemType selectedItem = slots_[selectedSlot_];
        const char* prompt = nullptr;
        std::string otherPrompt;

        if (selectedItem == ItemType::Medkit)
            prompt = "HOLD [E] TO HEAL (3s)";
        else
        {
            otherPrompt = std::string("[E] USE ") + Item::GetName(selectedItem);
            prompt = otherPrompt.c_str();
        }

        const int size = 18;
        DrawText(prompt, (GetScreenWidth() - MeasureText(prompt, size)) / 2, y - 35, size, WHITE);
    }

    if (messageTimer_ > 0.0f && !message_.empty())
    {
        const int size = 20;
        DrawText(message_.c_str(), (GetScreenWidth() - MeasureText(message_.c_str(), size)) / 2, y - 68, size, YELLOW);
    }

    DrawHealingHUD();
}

InventoryCheckpoint Inventory::CaptureCheckpoint() const
{
    InventoryCheckpoint data;
    data.slots = slots_;
    data.selectedSlot = selectedSlot_;
    for (const auto& item : items_)
        data.items.push_back({ item.GetType(), item.GetPosition(), item.IsCollected(), item.GetYaw(), item.GetScale() });
    return data;
}

void Inventory::RestoreCheckpoint(const InventoryCheckpoint& data, bool useScenePlacements)
{
    Reset();
    slots_ = data.slots;
    selectedSlot_ = std::clamp(data.selectedSlot, 0, MAX_SLOTS - 1);
    if (useScenePlacements) {
        // Old coordinates refer to the previous floor layout.
        std::vector<bool> matched(data.items.size());
        for (auto& item : items_) {
            if (!item.IsPickup()) continue;
            std::size_t nearest = data.items.size();
            float distance = std::numeric_limits<float>::max();
            for (std::size_t i = 0; i < data.items.size(); ++i) {
                const auto& saved = data.items[i];
                if (matched[i] || item.GetType() != saved.type) continue;
                const float candidate = Vector3DistanceSqr(item.GetPosition(), saved.position);
                if (candidate < distance) {
                    nearest = i;
                    distance = candidate;
                }
            }
            if (nearest != data.items.size()) {
                matched[nearest] = true;
                if (data.items[nearest].collected) item.Collect();
            }
        }
        return;
    }
    // Scene props keep their authored positions and full rotations after a reload.
    items_.erase(std::remove_if(items_.begin(), items_.end(), [](const Item& item) {
        return item.IsPickup();
    }), items_.end());
    const bool hasSceneProps = !items_.empty();
    for (const auto& saved : data.items)
    {
        Item item(saved.type, saved.position);
        if (hasSceneProps && !item.IsPickup()) continue;
        item.SetYaw(saved.yaw);
        item.SetScale(saved.scale);
        if (saved.collected) item.Collect();
        items_.push_back(item);
    }
}

bool Inventory::HasCrowbar() const
{
    return IsWorldItemCollected(ItemType::Crowbar);
}

bool Inventory::IsLostItemCollected() const
{
    return IsWorldItemCollected(ItemType::LostItem);
}

bool Inventory::IsGunCollected() const
{
    return IsWorldItemCollected(ItemType::Gun);
}

bool Inventory::IsAuthenticationKeyCollected() const
{
    return IsWorldItemCollected(ItemType::AuthenticationKey);
}

bool Inventory::IsWorldItemCollected(ItemType type) const
{
    return std::any_of(items_.begin(), items_.end(), [type](const Item& item) {
        return item.GetType() == type && item.IsCollected();
    });
}

const Item* Inventory::FindUncollectedItem(ItemType type) const
{
    const auto item = std::find_if(items_.begin(), items_.end(), [type](const Item& item) {
        return item.GetType() == type && !item.IsCollected();
    });
    return item != items_.end() ? &*item : nullptr;
}
