#include "SystemMode.h"

void SystemMode::Init(GameContext* gameContext)
{
    gameContext_ = gameContext;
}

void SystemMode::Update(float deltaTime)
{
}

void SystemMode::Draw()
{
    if (!gameContext_) return;

    debugUI_.Draw(*gameContext_);
}