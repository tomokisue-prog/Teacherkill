
#include "PlayerCamera.h"
#include "raymath.h"
#include <cmath>

const float PlayerCamera::MinPitch = -1.4f;
const float PlayerCamera::MaxPitch = 1.4f;

PlayerCamera::PlayerCamera()
{
    camera_.position = Vector3{ 0.0f, eyeHeight_, 0.0f };
    camera_.target = Vector3{ 0.0f, eyeHeight_, 1.0f };
    camera_.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera_.fovy = 70.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

// 被弾時の揺れを開始
void PlayerCamera::StartDamageImpact()
{
    damageImpactTimer_ = damageImpactDuration_;
}

void PlayerCamera::Update()
{
    Vector2 mouseDelta = GetMouseDelta();

    // マウス移動を視点の角度に反映
    yaw_ -= mouseDelta.x * sensitivity_;
    pitch_ -= mouseDelta.y * sensitivity_;

    // 上下を向きすぎないよう制限
    if (pitch_ > MaxPitch)
        pitch_ = MaxPitch;

    if (pitch_ < MinPitch)
        pitch_ = MinPitch;

    Vector3 forward =
    {
        cosf(pitch_) * sinf(yaw_),
        sinf(pitch_),
        cosf(pitch_) * cosf(yaw_)
    };

    forward = Vector3Normalize(forward);

    // プレイヤーの足元から目線の高さまで上げる
    camera_.position = Vector3Add(
        playerPosition_,
        Vector3{ 0.0f, eyeHeight_, 0.0f }
    );

    camera_.target = Vector3Add(
        camera_.position,
        forward
    );

    // 被弾した直後だけ視点を揺らす
    if (damageImpactTimer_ > 0.0f)
    {
        float elapsed =
            damageImpactDuration_ - damageImpactTimer_;

        // 時間がたつほど揺れを弱める
        float strength =
            damageImpactTimer_ / damageImpactDuration_;

        float shakeX =
            sinf(elapsed * 75.0f) * 0.028f * strength;

        float shakeY =
            cosf(elapsed * 60.0f) * 0.018f * strength;

        Vector3 right =
        {
            cosf(yaw_),
            0.0f,
            -sinf(yaw_)
        };

        Vector3 shakeOffset = Vector3Add(
            Vector3Scale(right, shakeX),
            Vector3{ 0.0f, shakeY, 0.0f }
        );

        // 向きが変わらないよう、カメラと注視点を同じ量だけずらす
        camera_.position = Vector3Add(
            camera_.position, shakeOffset);

        camera_.target = Vector3Add(
            camera_.target, shakeOffset);

        damageImpactTimer_ -= GetFrameTime();

        if (damageImpactTimer_ < 0.0f)
        {
            damageImpactTimer_ = 0.0f;
        }
    }
}

void PlayerCamera::OnActivate()
{
    DisableCursor();
}

void PlayerCamera::Reset()
{
    yaw_ = pitch_ = damageImpactTimer_ = 0.0f;
    camera_.position = Vector3Add(playerPosition_, {0.0f, eyeHeight_, 0.0f});
    camera_.target = Vector3Add(camera_.position, {0.0f, 0.0f, 1.0f});
}

Vector3 PlayerCamera::GetForwardVector() const
{
    Vector3 forward =
    {
        sinf(yaw_),
        0.0f,
        cosf(yaw_)
    };

    return Vector3Normalize(forward);
}

Vector3 PlayerCamera::GetRightVector() const
{
    Vector3 forward = GetForwardVector();

    return Vector3
    {
        forward.z,
        0.0f,
        -forward.x
    };
}

Vector3 PlayerCamera::GetPosition() const
{
    return camera_.position;
}

Vector3 PlayerCamera::GetUpVector() const
{
    return camera_.up;
}

float PlayerCamera::GetPitch() const
{
    return pitch_;
}

float PlayerCamera::GetYaw() const { return yaw_; }

void PlayerCamera::SetSensitivity(float multiplier)
{
    if (std::isfinite(multiplier)) sensitivity_ = .0025f * Clamp(multiplier, .1f, 3.0f);
}

void PlayerCamera::SetViewAngles(float yaw, float pitch)
{
    Reset();
    yaw_ = std::isfinite(yaw) ? std::remainder(yaw, 2 * PI) : 0;
    pitch_ = std::isfinite(pitch) ? Clamp(pitch, MinPitch, MaxPitch) : 0;
    const Vector3 forward{std::cos(pitch_) * std::sin(yaw_), std::sin(pitch_), std::cos(pitch_) * std::cos(yaw_)};
    camera_.target = Vector3Add(camera_.position, forward);
}

void PlayerCamera::SetEyeHeight(float height)
{
    if (std::isfinite(height)) eyeHeight_ = Clamp(height, .1f, 100.0f);
}
