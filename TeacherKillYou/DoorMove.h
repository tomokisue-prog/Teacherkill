#pragma once
#include "GameObject.h"

class Player; // 前方宣言

class DoorMove : public GameObject
{
public:
    DoorMove() : GameObject(GameObjectType::Stage, "Door") {}
    ~DoorMove() override = default;

    void Init();
    void Reset();

    // Player を受け取って距離とEキー入力を判定
    void Update(float deltaTime, const Player& player);
    void Draw() const;

    // 開閉制御
    void Open() { isOpen_ = true; }
    void Close() { isOpen_ = false; }
    void Toggle() { isOpen_ = !isOpen_; }

    // インスペクター編集用パラメータ
    float GetSlideSpeed() const { return slideSpeed_; }
    void SetSlideSpeed(float speed) { slideSpeed_ = speed; }

    float GetOpenDistance() const { return openDistance_; }
    void SetOpenDistance(float distance) { openDistance_ = distance; }

    float GetInteractRadius() const { return interactRadius_; }
    void SetInteractRadius(float radius) { interactRadius_ = radius; }

private:
    bool isOpen_{ false };
    float slideProgress_{ 0.0f }; // 0.0f (全閉) 〜 1.0f (全開)

    float slideSpeed_{ 2.0f };       // 開閉速度
    float openDistance_{ 2.0f };    // スライド移動距離 (X軸方向)
    float interactRadius_{ 1.5f };  // インタラクション可能な距離
    Vector3 closedPosition_{ 0.0f, 0.0f, 0.0f };
};