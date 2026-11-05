#pragma once
#include "GameObject.h"
#include "raymath.h"
#include <string>
#include "Inventory.h"

// イベントポイントの有効化条件
enum class EventDialogueCondition
{
    None,               // 無効 / 条件なし
    Normal,             // 通常（常時有効）
    LostItemCollected,  // LostItem回収後に有効
};

class EventDialoguePoint : public GameObject
{
public:
    EventDialoguePoint() : GameObject(GameObjectType::Stage, "EventDialoguePoint") {}
    ~EventDialoguePoint() override = default;

    void Init();
    void Reset();
    void Draw() const;

    // パラメータの取得・設定
    float GetRadius() const { return radius_; }
    void SetRadius(float r) { radius_ = r; }

    const std::string& GetDialogueId() const { return dialogueId_; }
    void SetDialogueId(const std::string& id) { dialogueId_ = id; }

    bool IsTriggered() const { return triggered_; }
    void SetTriggered(bool triggered) { triggered_ = triggered; }

    EventDialogueCondition GetCondition() const { return condition_; }
    void SetCondition(EventDialogueCondition cond) { condition_ = cond; }

    // inventory を引数として受け取る
    bool CheckPlayerInArea(Vector3 playerPos, const Inventory& inventory) const
    {
        if (triggered_) return false;

        // 条件チェック
        if (condition_ == EventDialogueCondition::None) return false;
        if (condition_ == EventDialogueCondition::LostItemCollected && !inventory.IsLostItemCollected())
        {
            return false;
        }

        float dist = Vector3Distance(position_, playerPos);
        return dist <= radius_;
    }

private:
    float radius_{ 2.0f };
    std::string dialogueId_{ "zombie_encounter" };
    bool triggered_{ false };
    EventDialogueCondition condition_{ EventDialogueCondition::Normal };
};