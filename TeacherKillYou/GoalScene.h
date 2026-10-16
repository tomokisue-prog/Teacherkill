#pragma once
#include "Scene.h"

class GoalScene final : public Scene
{
public:
    explicit GoalScene(GameContext* context) : Scene(context) {}

    void Init() override;
    void Update(float deltaTime) override;
    void Render() const override;
    void End() override;
};