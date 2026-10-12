#include "TeleportPoint.h"
#include "raymath.h"
#include "rlgl.h"

void TeleportPoint::Init()
{
    Reset();
}

void TeleportPoint::Reset()
{
    position_ = { 0.0f, 0.0f, 0.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
    pointNumber_ = 0;
    type_ = TeleportType::Source;
    radius_ = 1.5f;
}

void TeleportPoint::Draw() const
{
    // 種類によって色を変える（Source: 青っぽい、Destination: 緑っぽい）
    Color color = (type_ == TeleportType::Source) ? BLUE : GREEN;

    rlPushMatrix();
    {
        rlTranslatef(position_.x, position_.y, position_.z);
        rlScalef(scale_.x, scale_.y, scale_.z);

        // 判定範囲をワイヤーフレームの球で可視化
        DrawSphereWires({ 0, 0, 0 }, radius_, 8, 8, color);
    }
    rlPopMatrix();
}