#include "SystemMode.h"

void SystemMode::Init(GameContext* gameContext)
{
    gameContext_ = gameContext;
}

void SystemMode::Update(float deltaTime)
{
    // ImGuiの入力と表示はDraw側で扱い、ここではゲーム状態を更新しない。
}

void SystemMode::Draw()
{
    if (!gameContext_) return;

    debugUI_.Draw(*gameContext_);
}