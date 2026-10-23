#include "GoalPoint.h"
#include "raylib.h"
#include "rlgl.h"

void GoalPoint::Init()
{
    Reset();
}

void GoalPoint::Reset()
{
    position_ = { 5.0f, 0.0f, 0.0f }; // 初期位置の例
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
    radius_ = 1.5f;
}

void GoalPoint::Draw() const
{
    // ゴール地点は黄金色のワイヤーフレームで可視化
    Color color = { 255, 215, 0, 255 };

    rlPushMatrix();
    {
        rlTranslatef(position_.x, position_.y, position_.z);
        rlScalef(scale_.x, scale_.y, scale_.z);

        DrawSphereWires({ 0, 0, 0 }, radius_, 8, 8, color);
    }
    rlPopMatrix();
}