#include "SecurityTeleportPoint.h"
#include "rlgl.h"

void SecurityTeleportPoint::Init()
{
    Reset();
}

void SecurityTeleportPoint::Reset()
{
    position_ = { 0.0f, 0.0f, 0.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
    radius_ = 2.0f;
    pointNumber_ = 1;
    type_ = SecurityTeleportType::Source;
}

void SecurityTeleportPoint::Draw() const
{
    rlPushMatrix();
    {
        rlTranslatef(position_.x, position_.y, position_.z);
        rlScalef(scale_.x, scale_.y, scale_.z);

        // 移動元は青色、移動先（警備室）はマゼンタ等で色を分けるとエディタで見分けやすいです
        Color col = (type_ == SecurityTeleportType::Source) ? BLUE : MAGENTA;
        DrawSphereWires({ 0, 0, 0 }, radius_, 8, 8, col);
    }
    rlPopMatrix();
}