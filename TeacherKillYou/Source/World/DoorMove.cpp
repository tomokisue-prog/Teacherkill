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
    AM().StopSe(SeId::DoorClose);
    AM().PlaySe(SeId::DoorOpen);
}
void DoorMove::Close() {
    if (!isOpen_) return;
    isOpen_ = false;
    AM().StopSe(SeId::DoorOpen);
    AM().PlaySe(SeId::DoorClose);
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
        openConditionType_ == DoorOpenConditionType::SecurityTeleportEntered)
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
    // すでにMキー（開ける条件）で一度解除されている場合は、永遠にロックされない（ずっと開く）
    if (openConditionSatisfied_)
    {
        return false;
    }

    //「閉じる条件」が設定されている場合
    if (lockType_ != DoorLockType::None)
    {
        // まだ閉じる条件が発動していなければ、ロックされていない（普通の扉）
        if (!HasClosedByCondition(inventory))
        {
            return false;
        }
        // 閉じる条件が発動した後は、開ける条件（Mキー）が満たされるまでロック状態
        return !IsOpenConditionSatisfied();
    }

    //「閉じる条件が None」で「開ける条件だけ」が設定されている場合
    if (lockType_ == DoorLockType::None && openConditionType_ != DoorOpenConditionType::None)
    {
        return !IsOpenConditionSatisfied();
    }

    return false;
}

bool DoorMove::CanInteract(const Player& player, const Inventory* inventory) const
{
    float distance = Vector3Distance(position_, player.GetPosition());
    if (distance > interactRadius_) return false;

    // ロックされていなければインタラクション可能
    return !IsLocked(inventory);
}

void DoorMove::Update(float deltaTime, const Player& player, const Inventory* inventory)
{
    bool closedByCondition = HasClosedByCondition(inventory);
    bool openSatisfied = IsOpenConditionSatisfied();

    // 閉じる条件の評価 閉じる条件が成立した瞬間、ドアが開いていたら強制的に閉じる
    if (lockType_ != DoorLockType::None && closedByCondition && !openSatisfied && isOpen_)
    {
        Close();
    }

    float distance = Vector3Distance(position_, player.GetPosition());

    if (distance <= interactRadius_)
    {
        // Mキーによる「開ける条件」の解除判定
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
            openConditionSatisfied_ = true; // 一度解除したら以後ずっと開く
            AM().PlaySe(SeId::Confirm);     // 解除音
        }

        // Eキーによる開閉
        if (IsKeyPressed(KEY_E))
        {
            if (CanInteract(player, inventory))
            {
                Toggle();
            }
        }
    }

    // スライド補間処理
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

    Vector3 currentPos = Vector3Add(
        position_,
        Vector3Scale({ 1.0f, 0.0f, 0.0f }, openDistance_ * slideProgress_)
    );

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
    if (slideProgress_ >= 0.95f)
    {
        return { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
    }

    Vector3 localOffset = { openDistance_ * slideProgress_, 0.0f, 0.0f };

    float radY = rotation_.y * DEG2RAD;
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