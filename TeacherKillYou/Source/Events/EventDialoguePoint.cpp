#include "EventDialoguePoint.h"
#include "rlgl.h"

void EventDialoguePoint::Init()
{
    Reset();
}

void EventDialoguePoint::Reset()
{
    position_ = { 0.0f, 0.0f, 0.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
    radius_ = 2.0f;
    dialogueId_ = "zombie_encounter";
    triggered_ = false;
}

void EventDialoguePoint::Draw() const
{
    if (triggered_) return;

    // ギズモの Transform（位置・回転・スケール）を反映させる
    rlPushMatrix();
    {
        rlTranslatef(position_.x, position_.y, position_.z);
        // 必要に応じて回転やスケールを反映
        rlScalef(scale_.x, scale_.y, scale_.z);

        // 判定範囲を黄色のワイヤーフレームの球で可視化（中心が原点になるためズレない）
        DrawSphereWires({ 0, 0, 0 }, radius_, 8, 8, YELLOW);
    }
    rlPopMatrix();
}