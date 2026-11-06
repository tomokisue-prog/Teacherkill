#pragma once
#include "raylib.h"

enum class FadeState {
    Idle,      // 待機中
    FadeOut,   // 暗転中
    Holding,   // 暗転維持（ロード・切り替え間の演出）
    FadeIn     // 明転中
};

class GameFade {
public:
    GameFade();
    ~GameFade() = default;

    void Update(float deltaTime);
    void Draw() const;

    // フェードを開始する
    void StartFade(float speed = 2.0f);

    bool IsFading() const { return state_ != FadeState::Idle; }
    // 画面が真っ暗になった瞬間（テレポートを実行するタイミング）
    bool IsMidpoint() const { return isMidpointTriggered_; }

    void Reset();

private:
    FadeState state_{ FadeState::Idle };
    float alpha_{ 0.0f };
    float speed_{ 2.0f };
    bool isMidpointTriggered_{ false };
};