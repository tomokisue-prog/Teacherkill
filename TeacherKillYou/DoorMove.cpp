#include "DoorMove.h"
#include "Player.h"
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
}

void DoorMove::Update(float deltaTime, const Player& player)
{
    // 1. プレイヤーとドアの距離チェック
    float distance = Vector3Distance(position_, player.GetPosition());

    // プレイヤーがドアの近傍（触れている距離）にいて E キーが押されたら開閉切り替え
    if (distance <= interactRadius_ && IsKeyPressed(KEY_E))
    {
        Toggle();
    }

    // 2. 開閉状態に合わせて進行度 (0.0 〜 1.0) を補間
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

    // スライド進行度に応じた現在位置の計算 (初期位置 + X軸移動量)
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
            DrawMesh(doorModel.meshes[i], doorModel.materials[doorModel.meshMaterial[i]], MatrixIdentity());
        }
    }
    rlPopMatrix();
}

BoundingBox DoorMove::GetBoundingBox() const
{
    // スライド進行度を反映した現在の中心位置
    Vector3 currentPos = Vector3Add(
        position_,
        Vector3Scale({ 1.0f, 0.0f, 0.0f }, openDistance_ * slideProgress_)
    );

    // ドアの標準的なサイズ（幅1.2, 高さ2.0, 奥行き0.2 と仮定し、scale_ を掛ける）
    Vector3 halfSize = {
        0.6f * scale_.x,
        1.0f * scale_.y,
        0.1f * scale_.z
    };

    return {
        Vector3Subtract(currentPos, halfSize),
        Vector3Add(currentPos, halfSize)
    };
}
