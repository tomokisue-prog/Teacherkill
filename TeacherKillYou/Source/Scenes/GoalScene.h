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

private:
    Texture2D cover_{};
    Texture2D curtains_[3]{};
    Texture2D leaves_{};
    float elapsed_ = 0.0f;
};
