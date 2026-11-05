#pragma once
#include "GameObject.h"
#include "raymath.h"
#include <string>

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

    // プレイヤーが範囲内に入ったか判定
    bool CheckPlayerInArea(Vector3 playerPos) const
    {
        if (triggered_) return false;
        float dist = Vector3Distance(position_, playerPos);
        return dist <= radius_;
    }

private:
    float radius_{ 2.0f };               // 判定の広さ
    std::string dialogueId_{ "zombie_encounter" }; // 再生するセリフID
    bool triggered_{ false };            // 一度再生したか
};