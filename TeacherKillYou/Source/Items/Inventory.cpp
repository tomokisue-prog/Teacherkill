#include "Inventory.h"
#include "CheckpointData.h"
#include "Stage.h"
#include "WorldScale.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "raymath.h"

#include <algorithm>
#include <limits>

namespace
{
    const float PICKUP_RANGE = WorldScale::Player.height * 1.1f;
    constexpr float AIM_RADIUS = 0.30f;
    constexpr int AMMO_PICKUP_AMOUNT = 30;
}

void Inventory::Init()
{
    Reset();
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

    items_.emplace_back(ItemType::LostItem, Vector3{ 5.0f, 0.35f, 5.0f });
    items_.emplace_back(ItemType::KeyCard, Vector3{ 7.11f, 5.64f, 1.15f });
    items_.emplace_back(ItemType::Medkit, Vector3{ 0.0f, 0.35f, 1.8f });
    items_.emplace_back(ItemType::Ammo, Vector3{ 1.2f, 0.35f, 2.0f });
}

void Inventory::PlaceOnStage(const Stage& stage)
{
    for (auto& item : items_)
    {
        if (item.IsCollected()) continue;
        const Vector3 position = item.GetPosition();
        const Vector3 floor = stage.FindGroundPosition(position);
        if (position.y < floor.y + 0.08f)
        {
            item.SetPosition({ position.x, floor.y + 0.2f, position.z });
        }
    }
}

// 回復制御
void Inventory::CancelHealing()
{
    healing_ = false;
    healTimer_ = 0.0f;
}

float Inventory::GetHealingProgress() const
{
    const float progress = healTimer_ / HEAL_DURATION;
    return Clamp(progress, 0.0f, 1.0f);
}

// 内部ロジック & ヘルパー
bool Inventory::AddItem(ItemType item)
{
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
    lastCamera_ = camera;

    usedItem_ = ItemType::None;
    pickedUpItem_ = ItemType::None;
    messageTimer_ = (std::max)(0.0f, messageTimer_ - deltaTime);

    // スロット切り替え
    const int oldSlot = selectedSlot_;
    if (IsKeyPressed(KEY_ONE))   selectedSlot_ = 0;
    if (IsKeyPressed(KEY_TWO))   selectedSlot_ = 1;
    if (IsKeyPressed(KEY_THREE)) selectedSlot_ = 2;
    if (IsKeyPressed(KEY_FOUR))  selectedSlot_ = 3;

    if (selectedSlot_ != oldSlot)
        CancelHealing();

    focusedPickup_ = FindFocusedPickup(camera, stage);

    // 回復ホールド処理
    if (healing_)
    {
        if (!IsKeyDown(KEY_E) ||
            slots_[selectedSlot_] != ItemType::Medkit ||
            focusedPickup_ >= 0)
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

    if (!IsKeyPressed(KEY_E))
        return;

    // フィールド上のアイテムを拾う
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

    // スロット内アイテムの使用
    const ItemType selectedItem = slots_[selectedSlot_];
    if (selectedItem == ItemType::Medkit)
    {
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

const char* Inventory::GetItemModelKey(ItemType type) const
{
    switch (type)
    {
    case ItemType::KeyCard:  return ResourceKeys::Model_KeyCard;
    case ItemType::Medkit:   return ResourceKeys::Model_Medkit;
    case ItemType::Ammo:     return ResourceKeys::Model_Ammo;
    case ItemType::LostItem: return ResourceKeys::Model_LostItem;
    default:                 return nullptr;
    }
}

float Inventory::GetItemModelScale(ItemType type) const
{
    switch (type)
    {
    case ItemType::KeyCard:  return 0.15f; // キーカードのサイズ
    case ItemType::Medkit:   return 0.20f; // メドキットのサイズ
    case ItemType::Ammo:     return 0.15f; // 弾薬のサイズ
    case ItemType::LostItem: return 0.15f; // 忘れ物のサイズ
    default:                 return 1.0f;
    }
}

const Texture2D* Inventory::GetItemIcon(ItemType type) const
{
    switch (type)
    {
    case ItemType::KeyCard:  return RM().GetTexture(ResourceKeys::Icon_KeyCard);
    case ItemType::Medkit:   return RM().GetTexture(ResourceKeys::Icon_Medkit);
    case ItemType::LostItem: return RM().GetTexture(ResourceKeys::Icon_LostItem);
    default:                 return nullptr;
    }
}

void Inventory::DrawWorld() const
{
    for (const Item& item : items_)
    {
        const char* modelKey = GetItemModelKey(item.GetType());
        if (modelKey != nullptr)
        {
            Model model = RM().GetModel(modelKey);
            if (model.meshCount > 0 && model.meshes != nullptr)
            {
                item.Draw(&model, GetItemModelScale(item.GetType()));
            }
        }
    }
}

void Inventory::DrawHealingHUD() const
{
    if (!healing_)
        return;

    const float progress = GetHealingProgress();
    const float hudWidth = 600.0f;
    const float hudHeight = 144.0f;
    const float x = (GetScreenWidth() - hudWidth) * 0.5f;
    const float y = GetScreenHeight() - 195.0f;

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
    // 3Dワールド上の未回収 LostItem のFPS風三角形ピンを描画
    for (const auto& item : items_)
    {
        if (item.GetType() == ItemType::LostItem && !item.IsCollected())
        {
            Vector3 worldPos = item.GetPosition();
            worldPos.y += 0.8f; // 少し頭上にオフセット

            // 3D座標を2Dスクリーン座標に変換
            Vector2 screenPos = GetWorldToScreen(worldPos, lastCamera_);

            // カメラの背後にある場合は描画しない
            Vector3 toItem = Vector3Subtract(worldPos, lastCamera_.position);
            float forwardDot = Vector3DotProduct(
                Vector3Normalize(Vector3Subtract(lastCamera_.target, lastCamera_.position)),
                Vector3Normalize(toItem)
            );

            // 画面内に入っている場合のみ描画
            if (forwardDot > 0.0f &&
                screenPos.x >= 0 && screenPos.x <= GetScreenWidth() &&
                screenPos.y >= 0 && screenPos.y <= GetScreenHeight())
            {
                // プレイヤーからアイテムまでの距離を計算
                float distance = Vector3Length(toItem);

                // 近づくほど大きく、遠いと小さくなるようにスケールを計算
                float baseSize = 40.0f;
                float scaleFactor = Clamp(6.0f / (distance + 1.0f), 0.5f, 1.5f);
                float size = baseSize * scaleFactor;

                // ふわふわと上下に浮遊するアニメーション
                float floatOffset = std::sin(GetTime() * 4.0f) * 4.0f * scaleFactor;
                float centerY = screenPos.y + floatOffset;

                // 下向きの三角形ピンの頂点計算
                Vector2 tip = { screenPos.x, centerY + size * 0.5f };
                Vector2 topLeft = { screenPos.x - size * 0.45f, centerY - size * 0.5f };
                Vector2 topRight = { screenPos.x + size * 0.45f, centerY - size * 0.5f };

                // 内部を塗りつぶした黄色い三角形を描画
                DrawTriangle(topLeft, tip, topRight, YELLOW);

                // 視認性を上げるためのオレンジ色の枠線を描画
                DrawTriangleLines(topLeft, tip, topRight, ORANGE);
            }
        }
    }

    // インベントリスロットの描画
    const int slotWidth = 80;
    const int slotHeight = 70;
    const int gap = 8;
    const int totalWidth = MAX_SLOTS * slotWidth + (MAX_SLOTS - 1) * gap;
    const int startX = (GetScreenWidth() - totalWidth) / 2;
    const int y = GetScreenHeight() - slotHeight - 25;

    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        const int x = startX + i * (slotWidth + gap);
        const Rectangle rect{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(slotWidth), static_cast<float>(slotHeight) };
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
            const float maxWidth = 52.0f;
            const float maxHeight = 45.0f;
            const float sx = maxWidth / icon->width;
            const float sy = maxHeight / icon->height;
            const float scale = (std::min)(sx, sy);
            const float width = icon->width * scale;
            const float height = icon->height * scale;

            DrawTexturePro(*icon,
                { 0.0f, 0.0f, static_cast<float>(icon->width), static_cast<float>(icon->height) },
                { x + (slotWidth - width) / 2.0f,
                  y + 18.0f + (maxHeight - height) / 2.0f,
                  width, height },
                { 0.0f, 0.0f }, 0.0f, WHITE);
        }
        else
        {
            const char* name = Item::GetName(type);
            const int fontSize = 14;
            DrawText(name, x + (slotWidth - MeasureText(name, fontSize)) / 2, y + 37, fontSize, WHITE);
        }
    }

    // プロンプト / コンテキスト表示
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
        DrawText(prompt, (GetScreenWidth() - MeasureText(prompt, size)) / 2, y - 35, size, WHITE);
    }

    // システムメッセージ表示
    if (messageTimer_ > 0.0f && !message_.empty())
    {
        const int size = 20;
        DrawText(message_.c_str(), (GetScreenWidth() - MeasureText(message_.c_str(), size)) / 2, y - 68, size, YELLOW);
    }

    // 回復ゲージHUD描画
    DrawHealingHUD();
}

// チェックポイントの保存 / 復元
InventoryCheckpoint Inventory::CaptureCheckpoint() const
{
    InventoryCheckpoint data;
    data.slots = slots_;
    data.selectedSlot = selectedSlot_;
    for (const auto& item : items_)
        data.items.push_back({ item.GetType(), item.GetPosition(), item.IsCollected() });
    return data;
}

void Inventory::RestoreCheckpoint(const InventoryCheckpoint& data)
{
    Reset();
    slots_ = data.slots;
    selectedSlot_ = std::clamp(data.selectedSlot, 0, MAX_SLOTS - 1);
    items_.clear();
    for (const auto& saved : data.items)
    {
        items_.emplace_back(saved.type, saved.position);
        if (saved.collected) items_.back().Collect();
    }
}