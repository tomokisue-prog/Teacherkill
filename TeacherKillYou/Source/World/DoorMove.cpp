#include "HorrorLighting.h"
#include "DoorMove.h"
#include "CheckpointData.h"
#include "AudioManager.h"
#include "Player.h"
#include "Inventory.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "raymath.h"
#include "rlgl.h"

void DoorMove::Init()
{
    Reset();
}

void DoorMove::Reset()
{
    position_ = { 0.0f, 0.0f, 0.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
    closedPosition_ = position_;
    isOpen_ = false;
    slideProgress_ = 0.0f;
    lockType_ = DoorLockType::None;
    openConditionType_ = DoorOpenConditionType::None;
    openConditionSatisfied_ = false;
}

void DoorMove::Open() {
    if (isOpen_) return;
    isOpen_ = true;
    AM().PlaySe(SeId::Door);
}
void DoorMove::Close() {
    if (!isOpen_) return;
    isOpen_ = false;
    AM().PlaySe(SeId::Door);
}
void DoorMove::Toggle() {
    if (isOpen_) Close();
    else Open();
}

// 開ける条件を満たしているか
bool DoorMove::IsOpenConditionSatisfied() const
{
    if (openConditionType_ == DoorOpenConditionType::None)
    {
        return true;
    }
    if (openConditionType_ == DoorOpenConditionType::TestMKey ||
        openConditionType_ == DoorOpenConditionType::SecurityTeleportEntered ||
        openConditionType_ == DoorOpenConditionType::SecurityPCActivated) // ★SecurityPCActivatedに対応
    {
        return openConditionSatisfied_;
    }
    return true;
}

// 閉じる条件（LockCondition）自体がトリガーされたか（例：忘れ物を拾ったか）
bool DoorMove::HasClosedByCondition(const Inventory* inventory) const
{
    if (lockType_ == DoorLockType::None) return false;

    if (lockType_ == DoorLockType::LostItemCollected && inventory != nullptr)
    {
        for (const auto& item : inventory->GetWorldItems())
        {
            if (item.GetType() == ItemType::LostItem && item.IsCollected())
            {
                return true;
            }
        }
    }
    return false;
}

// 実際に現在ロックされていて開けられない状態か
bool DoorMove::IsLocked(const Inventory* inventory) const
{
    if (openConditionSatisfied_)
    {
        return false;
    }

    if (lockType_ != DoorLockType::None)
    {
        if (!HasClosedByCondition(inventory))
        {
            return false;
        }
        return !IsOpenConditionSatisfied();
    }

    if (lockType_ == DoorLockType::None && openConditionType_ != DoorOpenConditionType::None)
    {
        return !IsOpenConditionSatisfied();
    }

    return false;
}

bool DoorMove::IsPlayerInInteractionRange(const Player& player) const
{
    // 常に現在のスライド位置を反映した正確なボックスを取得する
    float radY = rotation_.y * DEG2RAD;
    float currentSlide = openDistance_ * slideProgress_; // スケールを掛けない正しい距離
    Vector3 localOffset = { currentSlide, 0.0f, 0.0f };
    Vector3 worldOffset = {
        localOffset.x * cosf(radY) - localOffset.z * sinf(radY),
        localOffset.y,
        localOffset.x * sinf(radY) + localOffset.z * cosf(radY)
    };
    Vector3 currentPos = Vector3Add(position_, worldOffset);

    float width = 1.2f * scale_.x;
    float height = 2.0f * scale_.y;
    float depth = 0.2f * scale_.z;

    if (fabsf(sinf(radY)) > 0.707f)
    {
        std::swap(width, depth);
    }

    Vector3 halfSize = { width * 0.5f, height * 0.5f, depth * 0.5f };
    BoundingBox box = { Vector3Subtract(currentPos, halfSize), Vector3Add(currentPos, halfSize) };

    // interactRadius_ 分だけボックスを全方向に広げてインタラクション可能エリアを作る
    Vector3 min = { box.min.x - interactRadius_, box.min.y - interactRadius_, box.min.z - interactRadius_ };
    Vector3 max = { box.max.x + interactRadius_, box.max.y + interactRadius_, box.max.z + interactRadius_ };
    BoundingBox expandedBox = { min, max };

    Vector3 playerPos = player.GetPosition();
    Vector3 playerCenter = { playerPos.x, playerPos.y + player.GetHeight() * 0.5f, playerPos.z };

    return (playerCenter.x >= expandedBox.min.x && playerCenter.x <= expandedBox.max.x &&
        playerCenter.y >= expandedBox.min.y && playerCenter.y <= expandedBox.max.y &&
        playerCenter.z >= expandedBox.min.z && playerCenter.z <= expandedBox.max.z);
}

bool DoorMove::CanInteract(const Player& player, const Inventory* inventory) const
{
    if (!IsPlayerInInteractionRange(player)) return false;
    return !IsLocked(inventory);
}

void DoorMove::Update(float deltaTime, const Player& player, const Inventory* inventory)
{
    bool closedByCondition = HasClosedByCondition(inventory);
    bool openSatisfied = IsOpenConditionSatisfied();

    if (lockType_ != DoorLockType::None && closedByCondition && !openSatisfied && isOpen_)
    {
        Close();
    }

    if (IsPlayerInInteractionRange(player))
    {
        bool needsMKey = false;
        if (lockType_ == DoorLockType::None)
        {
            needsMKey = (openConditionType_ == DoorOpenConditionType::TestMKey && !openSatisfied);
        }
        else
        {
            needsMKey = (closedByCondition && openConditionType_ == DoorOpenConditionType::TestMKey && !openSatisfied);
        }

        if (needsMKey && IsKeyPressed(KEY_M))
        {
            openConditionSatisfied_ = true;
            AM().PlaySe(SeId::Confirm);
        }

        if (IsKeyPressed(KEY_E))
        {
            if (CanInteract(player, inventory))
            {
                Toggle();
            }
        }
    }

    if (isOpen_)
    {
        slideProgress_ += slideSpeed_ * deltaTime;
        if (slideProgress_ > 1.0f) slideProgress_ = 1.0f;
    }
    else
    {
        slideProgress_ -= slideSpeed_ * deltaTime;
        if (slideProgress_ < 0.0f) slideProgress_ = 0.0f;
    }
}

void DoorMove::Draw() const
{
    Model doorModel = ResourceManager::GetInstance().GetModel(ResourceKeys::Model_Door);

    float radY = rotation_.y * DEG2RAD;
    float currentSlide = openDistance_ * slideProgress_; // スケールを掛けない

    Vector3 localOffset = { currentSlide, 0.0f, 0.0f };
    Vector3 worldOffset = {
        localOffset.x * cosf(radY) - localOffset.z * sinf(radY),
        localOffset.y,
        localOffset.x * sinf(radY) + localOffset.z * cosf(radY)
    };

    Vector3 currentPos = Vector3Add(position_, worldOffset);

    rlPushMatrix();
    {
        rlTranslatef(currentPos.x, currentPos.y, currentPos.z);
        rlRotatef(rotation_.z, 0.0f, 0.0f, 1.0f);
        rlRotatef(rotation_.x, 1.0f, 0.0f, 0.0f);
        rlRotatef(rotation_.y, 0.0f, 1.0f, 0.0f);
        rlScalef(scale_.x, scale_.y, scale_.z);

        for (int i = 0; i < doorModel.meshCount; i++)
        {
            DrawHorrorMesh(doorModel.meshes[i], doorModel.materials[doorModel.meshMaterial[i]], MatrixIdentity());
        }
    }
    rlPopMatrix();
}

BoundingBox DoorMove::GetBoundingBox() const
{
    // ドアが完全に開ききった（95%以上）ときは通り抜けられるように判定を消す
    if (slideProgress_ >= 0.95f)
    {
        return { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    }

    float radY = rotation_.y * DEG2RAD;
    float currentSlide = openDistance_ * slideProgress_; // スケールを掛けない

    Vector3 localOffset = { currentSlide, 0.0f, 0.0f };
    Vector3 worldOffset = {
        localOffset.x * cosf(radY) - localOffset.z * sinf(radY),
        localOffset.y,
        localOffset.x * sinf(radY) + localOffset.z * cosf(radY)
    };

    Vector3 currentPos = Vector3Add(position_, worldOffset);

    float width = 1.2f * scale_.x;
    float height = 2.0f * scale_.y;
    float depth = 0.2f * scale_.z;

    if (fabsf(sinf(radY)) > 0.707f)
    {
        std::swap(width, depth);
    }

    Vector3 halfSize = { width * 0.5f, height * 0.5f, depth * 0.5f };

    return {
        Vector3Subtract(currentPos, halfSize),
        Vector3Add(currentPos, halfSize)
    };
}

DoorCheckpoint DoorMove::CaptureCheckpoint() const
{
    return { {position_, rotation_, scale_}, slideSpeed_, openDistance_, interactRadius_, slideProgress_, isOpen_, lockType_ };
}

void DoorMove::RestoreCheckpoint(const DoorCheckpoint& data)
{
    position_ = data.transform.position;
    rotation_ = data.transform.rotation;
    scale_ = data.transform.scale;
    closedPosition_ = position_;
    slideSpeed_ = data.speed;
    openDistance_ = data.distance;
    interactRadius_ = data.radius;
    slideProgress_ = Clamp(data.progress, 0, 1);
    isOpen_ = data.open;
    lockType_ = data.lockType;
}