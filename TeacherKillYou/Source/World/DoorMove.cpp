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
        openConditionType_ == DoorOpenConditionType::SecurityPCActivated ||
        openConditionType_ == DoorOpenConditionType::EmergencyStairsKey ||
        openConditionType_ == DoorOpenConditionType::Crowbar)
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

    if (openConditionType_ == DoorOpenConditionType::Crowbar)
        return !inventory || !inventory->HasCrowbar();

    if (openConditionType_ == DoorOpenConditionType::EmergencyStairsKey)
    {
        if (inventory != nullptr && inventory->HasEmergencyStairsKey())
        {
            return false;
        }
        return true;
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

Vector3 DoorMove::CurrentPosition() const
{
    const float offset = openDistance_ * slideProgress_;
    if (openConditionType_ == DoorOpenConditionType::Crowbar)
        return Vector3Add(position_, {0, offset, 0});
    const float angle = rotation_.y * DEG2RAD;
    return Vector3Add(position_, {offset * cosf(angle), 0, offset * sinf(angle)});
}

BoundingBox DoorMove::BoundsAt(Vector3 position) const
{
    const float yaw = rotation_.y * DEG2RAD;
    const float cosine = fabsf(cosf(yaw));
    const float sine = fabsf(sinf(yaw));
    const float halfWidth = fabsf(scale_.x) * .6f;
    const float halfDepth = fabsf(scale_.z) * .1f;
    const Vector3 halfSize{halfWidth * cosine + halfDepth * sine, fabsf(scale_.y),
        halfWidth * sine + halfDepth * cosine};
    if (openConditionType_ != DoorOpenConditionType::Crowbar) position.y += scale_.y;
    return {Vector3Subtract(position, halfSize), Vector3Add(position, halfSize)};
}

bool DoorMove::IsPlayerInInteractionRange(const Player& player) const
{
    const Vector3 currentPos = openConditionType_ == DoorOpenConditionType::Crowbar ? position_ : CurrentPosition();
    const BoundingBox box = BoundsAt(currentPos);
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
    // 非常階段の鍵を持っている場合はアンロック完了にする
    if (openConditionType_ == DoorOpenConditionType::EmergencyStairsKey)
    {
        if (inventory != nullptr && inventory->HasEmergencyStairsKey())
        {
            openConditionSatisfied_ = true;
        }
    }

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
                if (openConditionType_ == DoorOpenConditionType::Crowbar) {
                    openConditionSatisfied_ = true;
                    Open();
                } else {
                    const Vector3 playerCenter = Vector3Add(player.GetPosition(), {0, player.GetHeight() * .5f, 0});
                    if (!isOpen_ || !CheckCollisionBoxSphere(BoundsAt(position_), playerCenter, player.GetRadius()))
                        Toggle();
                }
            }
            else
            {
                AM().PlaySe(SeId::Error);
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
    const char* modelKey = openConditionType_ == DoorOpenConditionType::Crowbar
        ? ResourceKeys::Model_ExitShutter : ResourceKeys::Model_Door;
    const Model doorModel = RM().GetModel(modelKey);
    const Vector3 currentPos = CurrentPosition();

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
    if (IsFullyOpen()) return {};
    // The shutter blocks the vestibule until its full opening sequence completes.
    return BoundsAt(openConditionType_ == DoorOpenConditionType::Crowbar ? position_ : CurrentPosition());
}

DoorCheckpoint DoorMove::CaptureCheckpoint() const
{
    return { {position_, rotation_, scale_}, slideSpeed_, openDistance_, interactRadius_, slideProgress_, isOpen_, lockType_, openConditionSatisfied_ };
}

void DoorMove::RestoreCheckpoint(const DoorCheckpoint& data)
{
    position_ = data.transform.position;
    rotation_ = data.transform.rotation;
    scale_ = data.transform.scale;
    closedPosition_ = position_;
    if (openConditionType_ != DoorOpenConditionType::Crowbar) slideSpeed_ = data.speed;
    openDistance_ = data.distance;
    interactRadius_ = data.radius;
    slideProgress_ = Clamp(data.progress, 0, 1);
    isOpen_ = data.open;
    lockType_ = data.lockType;
    openConditionSatisfied_ = data.conditionSatisfied;
}