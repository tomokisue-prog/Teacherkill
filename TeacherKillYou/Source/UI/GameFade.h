#pragma once
#include "raylib.h"

enum class FadeState {
    Idle,      // 待機
    FadeOut,   // 暗くする
    Holding,   // 暗転を維持
    FadeIn     // 明るく戻す
};

class GameFade {
public:
    GameFade();
    ~GameFade() = default;

    void Update(float deltaTime);
    void Draw() const;

    void StartFade(float speed = 2.0f);

    bool IsFading() const { return state_ != FadeState::Idle; }
    // 真っ暗になったらテレポートを実行
    bool IsMidpoint() const { return isMidpointTriggered_; }

    void Reset();

private:
    FadeState state_{ FadeState::Idle };
    float alpha_{ 0.0f };
    float speed_{ 2.0f };
    bool isMidpointTriggered_{ false };
};