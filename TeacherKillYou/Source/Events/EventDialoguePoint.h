#pragma once
#include "GameObject.h"
#include "raymath.h"
#include "rlgl.h"
#include <string>
#include <vector>
#include "Inventory.h"
#include "DoorMove.h"

// イベントポイントの有効化条件
enum class EventDialogueCondition
{
    None,                       // 無効 / 発動しない
    Normal,                     // 通常（常時有効）
    LostItemCollected,          // 忘れ物（LostItem）収集後に有効
    SecurityPCDoorUnlocked,     // ドアの開ける条件がSecurityPCActivatedでそのドアが開けるようになったら有効
    EmergencyStairsDoorCanOpen, // 非常階段のキー所持（EmergencyStairsKeyドアが開ける状態）で有効
    EmergencyStairsDoorOpened,  // ドアの開ける条件がEmergencyStairsKeyのドアが開いたら有効 ★今回追加
};

class EventDialoguePoint : public GameObject
{
public:
    EventDialoguePoint() : GameObject(GameObjectType::Stage, "EventDialoguePoint") {}
    ~EventDialoguePoint() override = default;

    void Init()
    {
        Reset();
    }

    void Reset()
    {
        position_ = { 0.0f, 0.0f, 0.0f };
        rotation_ = { 0.0f, 0.0f, 0.0f };
        scale_ = { 1.0f, 1.0f, 1.0f };
        radius_ = 2.0f;
        dialogueId_ = "stairs_blocked_3f";
        condition_ = EventDialogueCondition::None;
        triggered_ = false;
    }

    void Draw() const
    {
        if (triggered_) return;

        rlPushMatrix();
        {
            rlTranslatef(position_.x, position_.y, position_.z);
            rlScalef(scale_.x, scale_.y, scale_.z);

            Color col = YELLOW;
            if (condition_ == EventDialogueCondition::SecurityPCDoorUnlocked) col = ORANGE;
            else if (condition_ == EventDialogueCondition::EmergencyStairsDoorCanOpen) col = GREEN;
            else if (condition_ == EventDialogueCondition::EmergencyStairsDoorOpened) col = SKYBLUE;

            DrawSphereWires({ 0, 0, 0 }, radius_, 8, 8, col);
        }
        rlPopMatrix();
    }

    float GetRadius() const { return radius_; }
    void SetRadius(float r) { radius_ = r; }

    const std::string& GetDialogueId() const { return dialogueId_; }
    void SetDialogueId(const std::string& id) { dialogueId_ = id; }

    bool IsTriggered() const { return triggered_; }
    void SetTriggered(bool triggered) { triggered_ = triggered; }

    EventDialogueCondition GetCondition() const { return condition_; }
    void SetCondition(EventDialogueCondition cond) { condition_ = cond; }

    // プレイヤーがエリア内にいて、かつ有効化条件を満たしているかチェック
    bool CheckPlayerInArea(Vector3 playerPos, const Inventory& inventory, const std::vector<DoorMove>* doors = nullptr) const
    {
        if (triggered_) return false;

        if (condition_ == EventDialogueCondition::None) return false;

        // 忘れ物取得後に有効
        if (condition_ == EventDialogueCondition::LostItemCollected && !inventory.IsLostItemCollected())
        {
            return false;
        }

        // SecurityPCActivatedドアが開ける状態になったら有効
        if (condition_ == EventDialogueCondition::SecurityPCDoorUnlocked)
        {
            if (!doors || doors->empty()) return false;

            bool canOpen = false;
            for (const auto& d : *doors)
            {
                if (d.GetOpenConditionType() == DoorOpenConditionType::SecurityPCActivated &&
                    d.IsOpenConditionSatisfied())
                {
                    canOpen = true;
                    break;
                }
            }
            if (!canOpen) return false;
        }

        // EmergencyStairsKeyドアが開ける状態（鍵所持）になったら有効
        if (condition_ == EventDialogueCondition::EmergencyStairsDoorCanOpen)
        {
            if (!inventory.HasEmergencyStairsKey()) return false;
        }

        // ★ ドアの開ける条件がEmergencyStairsKeyのドアが実際に開いたら有効
        if (condition_ == EventDialogueCondition::EmergencyStairsDoorOpened)
        {
            if (!doors || doors->empty()) return false;

            bool opened = false;
            for (const auto& d : *doors)
            {
                if (d.GetOpenConditionType() == DoorOpenConditionType::EmergencyStairsKey &&
                    d.IsOpen())
                {
                    opened = true;
                    break;
                }
            }
            if (!opened) return false;
        }

        float dist = Vector3Distance(position_, playerPos);
        return dist <= radius_;
    }

private:
    float radius_{ 2.0f };
    std::string dialogueId_{ "stairs_blocked_3f" };
    bool triggered_{ false };
    EventDialogueCondition condition_{ EventDialogueCondition::None };
};