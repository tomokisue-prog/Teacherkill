#pragma once
#include "ICamera.h"
#include "WorldScale.h"

class PlayerCamera : public ICamera {
public:
    PlayerCamera();
    static const float MinPitch;
    static const float MaxPitch;

    void Update() override;
    void OnActivate() override;

    void StartDamageImpact();
    void Reset(); // ラウンド開始時の向きと揺れをリセット

    Vector3 GetForwardVector() const;
    Vector3 GetRightVector() const;
    Vector3 GetPosition() const;
    Vector3 GetUpVector() const;
    float GetPitch() const;
    float GetYaw() const;
    void SetViewAngles(float yaw, float pitch);
    void SetSensitivity(float multiplier);
    void SetEyeHeight(float height);
    void SetPlayerPosition(Vector3 pos) { playerPosition_ = pos; }

private:
    Vector3 playerPosition_{ 0.0f, 0.0f, 0.0f }; // 追従するプレイヤーの足元
    float eyeHeight_{ WorldScale::Player.height * WorldScale::EyeHeightRatio };       // 目線の高さ
    float sensitivity_{ 0.0025f };  // マウス感度

    // 被弾時の揺れ
    float damageImpactTimer_ = 0.0f;

    static constexpr float damageImpactDuration_ = 0.15f;

    // 視点の角度はラジアン
    float yaw_{ 0.0f };             // 左右
    float pitch_{ 0.0f };           // 上下

   
};