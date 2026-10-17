#pragma once
#include "ICamera.h"

class PlayerCamera : public ICamera {
public:
    PlayerCamera();

    // インターフェースのオーバーライド（1つに集約）
    void Update() override;
    void OnActivate() override;

    void StartDamageImpact();

    Vector3 GetForwardVector() const;
    Vector3 GetRightVector() const;
    Vector3 GetPosition() const;
    Vector3 GetUpVector() const;
    float GetPitch() const;
    // プレイヤーの位置を設定するヘルパーメソッド
    void SetPlayerPosition(Vector3 pos) { playerPosition_ = pos; }

private:
    Vector3 playerPosition_{ 0.0f, 0.0f, 0.0f }; // 追従対象の位置
    float eyeHeight_{ 1.6f };       // 目線の高さ（）
    float sensitivity_{ 0.0025f };  // マウス感度

    // 被ダメージ時のカメラシェイク
    float damageImpactTimer_ = 0.0f;

    // シェイク時間（0.15秒）
    static constexpr float damageImpactDuration_ = 0.15f;

    // カメラの現在の角度（ラジアン）5
    float yaw_{ 0.0f };             // 水平角度
    float pitch_{ 0.0f };           // 垂直角度

   
};