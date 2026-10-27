#pragma once
#include "EventBase.h"
#include <string>
#include "raylib.h"

class Tutorial4FFloorEvent : public EventBase
{
public:
    Tutorial4FFloorEvent() = default;
    ~Tutorial4FFloorEvent() override = default;

    void Init() override;
    void Update(float deltaTime) override;
    void Draw() const override;


private:
    enum class Phase
    {
        OpeningDialogue, // オープニング会話中
        WaitWASD,        // WASD操作ガイド表示・キー入力待ち
        FadeOut,         // WASDキー入力後フェードアウト
        MissionActive    // 終了
    };

    Phase currentPhase_{ Phase::OpeningDialogue };
    bool hasMoved_{ false };
    float fadeTimer_{ 0.0f }; // フェード用タイマー
    static constexpr float fadeDuration_{ 0.8f }; // ふんわり変化する時間

    //忘れ物の目標位置
    Vector3 targetItemPos_{ 5.0f, 0.5f, 5.0f };
    float markerAnimTimer_{ 0.0f };// ピンをふわふわ動かすためのアニメーション用タイマー
};