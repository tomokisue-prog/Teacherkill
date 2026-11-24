#pragma once
#include "raymath.h"
#include <cmath>

// 被弾時に2回脈打って消える。残り時間は各キャラ側で持つ
inline float DamageFlashStrength(float remaining, float duration) {
    if (remaining <= 0.0f || duration <= 0.0f) return 0.0f;
    const float envelope = Clamp(remaining / duration, 0.0f, 1.0f);
    const float elapsed = 1.0f - envelope;
    const float pulse = 0.5f + 0.5f * std::cos(elapsed * 4.0f * PI);
    return envelope * (0.18f + 0.82f * pulse);
}
