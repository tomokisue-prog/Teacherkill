#pragma once

class EventBase
{
public:
    EventBase() = default;
    virtual ~EventBase() = default;

    // イベント開始時に呼ばれる
    virtual void Init() = 0;

    // 毎フレームの更新
    virtual void Update(float deltaTime) = 0;

    // UIや専用の2D/3D描画が必要な場合に呼ぶ
    virtual void Draw() const {}

    // イベントが完了したかどうか
    bool IsFinished() const { return isFinished_; }

protected:
    bool isFinished_{ false };
};