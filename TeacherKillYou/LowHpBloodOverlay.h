#pragma once

#include "raylib.h"

// Screen-space damage vignette. Independent from the 3D BloodEffect.
// This uses the player's HP ratio, not an independent damage timer.
class LowHpBloodOverlay
{
public:
    LowHpBloodOverlay() = default;
    ~LowHpBloodOverlay() = default;

    void Init();
    void Reset();
    void Update(float deltaTime, int hp, int maxHp);
    void Draw() const;
    void Shutdown();

private:
    static float StrengthForHp(float hpPercent);

    Texture2D texture_{};
    float opacity_ = 0.0f;
};