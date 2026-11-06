#include "LowHpBloodOverlay.h"
#include <algorithm>
#include <cmath>

void LowHpBloodOverlay::Init()
{
    Shutdown();
    texture_ = LoadTexture("Data/Image/UI/LowHP_Blood.png");
    if (texture_.id != 0)
    {
        SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
    }
    else
    {
        TraceLog(LOG_WARNING, "LowHpBloodOverlay: texture missing (Data/Image/UI/LowHP_Blood.png)");
    }
    opacity_ = 0.0f;
}

void LowHpBloodOverlay::Reset()
{
    opacity_ = 0.0f;
}

void LowHpBloodOverlay::Update(float deltaTime, int hp, int maxHp)
{
    const float hpPercent = maxHp > 0
        ? std::clamp(100.0f * static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 100.0f)
        : 100.0f;

    const float target = StrengthForHp(hpPercent);
    // Frame-rate independent easing: natural appearance after hits / healing.
    const float dt = std::clamp(deltaTime, 0.0f, 0.1f);
    const float blend = 1.0f - std::exp(-7.0f * dt);
    opacity_ += (target - opacity_) * blend;
    if (std::fabs(target - opacity_) < 0.001f) opacity_ = target;
}

void LowHpBloodOverlay::Draw() const
{
    if (texture_.id == 0 || opacity_ < 0.005f) return;

    DrawTexturePro(
        texture_,
        Rectangle{ 0.0f, 0.0f, static_cast<float>(texture_.width), static_cast<float>(texture_.height) },
        Rectangle{ 0.0f, 0.0f, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) },
        Vector2{ 0.0f, 0.0f },
        0.0f,
        Fade(WHITE, std::clamp(opacity_, 0.0f, 1.0f))
    );
}

void LowHpBloodOverlay::Shutdown()
{
    if (texture_.id != 0)
    {
        UnloadTexture(texture_);
        texture_ = {};
    }
    opacity_ = 0.0f;
}

float LowHpBloodOverlay::StrengthForHp(float hpPercent)
{
    // HP 100 -> 0%; HP 50 -> 21%; HP 30 -> 30%;
    // HP 20 -> 60%; HP 10 -> 90%; HP 0 -> 100%.
    if (hpPercent >= 50.0f)
        return (100.0f - hpPercent) / 50.0f * 0.21f;
    if (hpPercent >= 30.0f)
        return 0.21f + (50.0f - hpPercent) / 20.0f * 0.09f;
    if (hpPercent >= 10.0f)
        return 0.30f + (30.0f - hpPercent) / 20.0f * 0.60f;
    return 0.90f + (10.0f - hpPercent) / 10.0f * 0.10f;
}
