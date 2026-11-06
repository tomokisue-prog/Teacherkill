#pragma once
#include "Scene.h"
#include "IntroVideoPlayer.h"

class TitleScene final : public Scene
{
public:
    explicit TitleScene(GameContext* context) : Scene(context) {}

    void Init() override;
    void Update(float deltaTime) override;
    void Render() const override;
    void End() override;

private:
    bool checkpointAvailable_ = false;
    int hoveredButton_ = -1;
    IntroVideoPlayer intro_;
    Texture2D cover_{};
    Texture2D canopy_{};
    Texture2D leaves_{};
    Texture2D logo_{};
    Texture2D menu_{};
    Texture2D lightning_{}; // Optional: loaded when the designer's PNG exists.
    float windTime_ = 0.0f;
};
