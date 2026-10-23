#pragma once

#include "DebugUI.h"
#include "GameContext.h"

class SystemMode
{
public:
    SystemMode() = default;

    void Init(GameContext* gameContext);
    void Reset();
    void Update(float deltaTime);
    void Draw();

private:
    GameContext* gameContext_{ nullptr };
    DebugUI debugUI_;
};