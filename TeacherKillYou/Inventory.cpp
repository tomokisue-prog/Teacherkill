
#include "Inventory.h"
#include "raymath.h"

#include <limits>

// ---------------------------------------
// Item names
// ---------------------------------------
const char* Inventory::GetItemName(ItemType item)
{
    switch (item)
    {
    case ItemType::Medkit:  return "MEDKIT";
    case ItemType::Battery: return "BATTERY";
    case ItemType::Key:     return "KEY";
    default:                return "EMPTY";
    }
}

// ---------------------------------------
// Reset inventory and test pickups
// ---------------------------------------
void Inventory::Reset()
{
    slots_.fill(ItemType::None);

    selectedSlot_ = 0;
    focusedPickup_ = -1;
    usedItem_ = ItemType::None;

    message_.clear();
    messageTimer_ = 0.0f;

    // Temporary test items near player spawn
    // Adjust positions for your stage.
    pickups_ = {
        { ItemType::Medkit,  { 0.0f, 0.35f, 1.8f }, false },
        { ItemType::Battery, { 1.2f, 0.35f, 2.0f }, false },
        { ItemType::Key,     {-1.2f, 0.35f, 2.0f }, false }
    };
}

// ---------------------------------------
// Add item to first available slot
// ---------------------------------------
bool Inventory::AddItem(ItemType item)
{
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

// ---------------------------------------
// Find a pickup near the camera aim
// ---------------------------------------
int Inventory::FindFocusedPickup(
    const Camera3D& camera) const
{
    Vector3 forward = Vector3Subtract(
        camera.target, camera.position);

    if (Vector3Length(forward) < 0.0001f)
        return -1;

    forward = Vector3Normalize(forward);

    constexpr float pickupRange = 2.5f;
    constexpr float aimRadius = 0.55f;

    float nearest = std::numeric_limits<float>::max();
    int result = -1;

    for (int i = 0; i < static_cast<int>(pickups_.size()); ++i)
    {
        const WorldPickup& pickup = pickups_[i];

        if (pickup.collected)
            continue;

        Vector3 toItem = Vector3Subtract(
            pickup.position, camera.position);

        float forwardDistance =
            Vector3DotProduct(toItem, forward);

        if (forwardDistance <= 0.0f ||
            forwardDistance > pickupRange)
            continue;

        Vector3 closestOffset = Vector3Subtract(
            toItem,
            Vector3Scale(forward, forwardDistance));

        float aimDistance = Vector3Length(closestOffset);

        if (aimDistance > aimRadius)
            continue;

        if (forwardDistance < nearest)
        {
            nearest = forwardDistance;
            result = i;
        }
    }

    return result;
}

// ---------------------------------------
// Update controls and interactions
// ---------------------------------------
void Inventory::Update(
    float deltaTime, const Camera3D& camera)
{
    // Reset one-frame use event
    usedItem_ = ItemType::None;

    if (messageTimer_ > 0.0f)
        messageTimer_ -= deltaTime;

    // Select slot
    if (IsKeyPressed(KEY_ONE))   selectedSlot_ = 0;
    if (IsKeyPressed(KEY_TWO))   selectedSlot_ = 1;
    if (IsKeyPressed(KEY_THREE)) selectedSlot_ = 2;
    if (IsKeyPressed(KEY_FOUR))  selectedSlot_ = 3;

    focusedPickup_ = FindFocusedPickup(camera);

    if (!IsKeyPressed(KEY_E))
        return;

    // Priority 1: Pick up an item
    if (focusedPickup_ >= 0)
    {
        WorldPickup& pickup = pickups_[focusedPickup_];

        if (AddItem(pickup.type))
        {
            message_ = std::string("Picked up ") +
                GetItemName(pickup.type);

            pickup.collected = true;
            focusedPickup_ = -1;
        }
        else
        {
            message_ = "Inventory Full!";
        }

        messageTimer_ = 2.0f;
        return;
    }

    // Priority 2: Use selected inventory item
    ItemType item = slots_[selectedSlot_];

    if (item == ItemType::None)
    {
        message_ = "Empty Slot";
    }
    else if (item == ItemType::Key)
    {
        // Keys should only be used with a locked door.
        message_ = "Find a locked door";
    }
    else
    {
        // Consumable item
        usedItem_ = item;
        slots_[selectedSlot_] = ItemType::None;

        message_ = std::string("Used ") +
            GetItemName(item);
    }

    messageTimer_ = 2.0f;
}

// ---------------------------------------
// Draw temporary world items
// ---------------------------------------
void Inventory::DrawWorld() const
{
    for (const WorldPickup& pickup : pickups_)
    {
        if (pickup.collected)
            continue;

        Color color = YELLOW;

        if (pickup.type == ItemType::Medkit)
            color = RED;
        else if (pickup.type == ItemType::Battery)
            color = SKYBLUE;
        else if (pickup.type == ItemType::Key)
            color = GOLD;

        DrawCube(
            pickup.position,
            0.45f, 0.45f, 0.45f,
            color);

        DrawCubeWires(
            pickup.position,
            0.45f, 0.45f, 0.45f,
            WHITE);
    }
}

// ---------------------------------------
// Draw 4 slots at bottom center
// ---------------------------------------
void Inventory::DrawHUD() const
{
    const int slotWidth = 80;
    const int slotHeight = 70;
    const int gap = 8;

    const int totalWidth =
        MAX_SLOTS * slotWidth + (MAX_SLOTS - 1) * gap;

    const int startX =
        (GetScreenWidth() - totalWidth) / 2;

    const int y = GetScreenHeight() - slotHeight - 25;

    for (int i = 0; i < MAX_SLOTS; ++i)
    {
        int x = startX + i * (slotWidth + gap);

        Rectangle rect{
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(slotWidth),
            static_cast<float>(slotHeight)
        };

        bool selected = (i == selectedSlot_);

        Color background = selected
            ? Color{ 50, 60, 45, 215 }
        : Color{ 20, 20, 25, 190 };

        DrawRectangleRec(rect, background);

        DrawRectangleLinesEx(
            rect,
            selected ? 3.0f : 1.0f,
            selected ? LIME : GRAY);

        // Slot number
        const char* number = TextFormat("%d", i + 1);

        DrawText(number, x + 7, y + 6, 16, LIGHTGRAY);

        // Item name
        const char* name = GetItemName(slots_[i]);

        const int fontSize = 14;
        int textWidth = MeasureText(name, fontSize);

        DrawText(
            name,
            x + (slotWidth - textWidth) / 2,
            y + 37,
            fontSize,
            slots_[i] == ItemType::None ? GRAY : WHITE);
    }

    // Show contextual E prompt
    std::string prompt;

    if (focusedPickup_ >= 0)
    {
        prompt = std::string("[E] Pick up ") +
            GetItemName(pickups_[focusedPickup_].type);
    }
    else if (slots_[selectedSlot_] != ItemType::None)
    {
        prompt = std::string("[E] Use ") +
            GetItemName(slots_[selectedSlot_]);
    }

    if (!prompt.empty())
    {
        const int fontSize = 20;

        DrawText(
            prompt.c_str(),
            (GetScreenWidth() -
                MeasureText(prompt.c_str(), fontSize)) / 2,
            y - 35,
            fontSize,
            WHITE);
    }

    // Temporary feedback
    if (messageTimer_ > 0.0f && !message_.empty())
    {
        DrawText(
            message_.c_str(),
            (GetScreenWidth() -
                MeasureText(message_.c_str(), 20)) / 2,
            y - 68,
            20,
            YELLOW);
    }
}
