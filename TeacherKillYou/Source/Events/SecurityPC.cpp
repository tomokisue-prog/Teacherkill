#include "SecurityPC.h"
#include "HorrorLighting.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"

SecurityPC::SecurityPC()
    : GameObject(GameObjectType::None, "SecurityPC")
{
}

void SecurityPC::Init()
{
    Reset();
}

void SecurityPC::Reset()
{
    // 初期位置やスケールのリセット
    position_ = { 0.0f, 0.0f, 0.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
    isActivated_ = false;
}

void SecurityPC::Update(float deltaTime)
{
}

void SecurityPC::Draw() const
{
    Model model = ResourceManager::GetInstance().GetModel(ResourceKeys::Security_PC);
    if (model.meshCount == 0) return;

    DrawHorrorModelEx(model, position_, { 0.0f, 1.0f, 0.0f }, rotation_.y, scale_, WHITE);
}