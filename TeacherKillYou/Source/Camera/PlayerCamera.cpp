
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

// ============================================
// 被ダメージ時のカメラシェイク開始
// ============================================
void PlayerCamera::StartDamageImpact()
{
    damageImpactTimer_ = damageImpactDuration_;
}

// ============================================
// カメラ更新
// ============================================
void PlayerCamera::Update()
{
    Vector2 mouseDelta = GetMouseDelta();

    // 1. マウス入力
    yaw_ -= mouseDelta.x * sensitivity_;
    pitch_ -= mouseDelta.y * sensitivity_;

    // 2. 上下の視点制限
    if (pitch_ > MaxPitch)
        pitch_ = MaxPitch;

    if (pitch_ < MinPitch)
        pitch_ = MinPitch;

    // 3. 正面方向
    Vector3 forward =
    {
        cosf(pitch_) * sinf(yaw_),
        sinf(pitch_),
        cosf(pitch_) * cosf(yaw_)
    };

    forward = Vector3Normalize(forward);

    // 4. プレイヤーの頭の位置
    camera_.position = Vector3Add(
        playerPosition_,
        Vector3{ 0.0f, eyeHeight_, 0.0f }
    );

    // 5. カメラターゲット
    camera_.target = Vector3Add(
        camera_.position,
        forward
    );

    // ========================================
    // ★ NEW: Damage Impact Camera Shake
    // ========================================
    if (damageImpactTimer_ > 0.0f)
    {
        // 経過時間
        float elapsed =
            damageImpactDuration_ - damageImpactTimer_;

        // 時間経過で揺れを弱くする
        float strength =
            damageImpactTimer_ / damageImpactDuration_;

        // 横方向の揺れ
        float shakeX =
            sinf(elapsed * 75.0f) * 0.028f * strength;

        // 縦方向の揺れ
        float shakeY =
            cosf(elapsed * 60.0f) * 0.018f * strength;

        // カメラの右方向
        Vector3 right =
        {
            cosf(yaw_),
            0.0f,
            -sinf(yaw_)
        };

        // 揺れの位置オフセット
        Vector3 shakeOffset = Vector3Add(
            Vector3Scale(right, shakeX),
            Vector3{ 0.0f, shakeY, 0.0f }
        );

        // カメラとターゲットを同じ量だけ移動
        camera_.position = Vector3Add(
            camera_.position, shakeOffset);

        camera_.target = Vector3Add(
            camera_.target, shakeOffset);

        // タイマー更新
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
