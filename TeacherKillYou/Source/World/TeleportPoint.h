#pragma once
#include "GameObject.h"

enum class TeleportType {
    Source,      // 元の位置（この範囲に入ると移動）
    Destination  // 移動先の位置（ここへテレポートする）
};

class TeleportPoint : public GameObject
{
public:
    TeleportPoint() : GameObject(GameObjectType::Stage, "TeleportPoint") {}
    ~TeleportPoint() override = default;

    void Init();
    void Reset();
    void Draw() const;

    // パラメータの取得・設定
    int GetPointNumber() const { return pointNumber_; }
    void SetPointNumber(int num) { pointNumber_ = num; }

    TeleportType GetTeleportType() const { return type_; }
    void SetTeleportType(TeleportType type) { type_ = type; }

    float GetRadius() const { return radius_; }
    void SetRadius(float r) { radius_ = r; }

private:
    int pointNumber_{ 0 };            // テレポートのペア番号
    TeleportType type_{ TeleportType::Source }; // 種類 (Source / Destination)
    float radius_{ 1.5f };            // 接触判定の半径
};