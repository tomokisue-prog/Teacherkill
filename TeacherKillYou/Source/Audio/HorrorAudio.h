#pragma once
#include "raylib.h"
#include <cstdint>
class Player;
class PlayerCamera;
class Stage;
namespace demo { class EnemyManager; }

// Footsteps follow actual movement; the heartbeat responds to remaining HP.
class HorrorAudio {
public:
    void Reset(Vector3 position);
    void Update(float deltaTime, const Player& player, const PlayerCamera& camera,
        const demo::EnemyManager& enemies, const Stage& stage, bool playerView);
private:
    Vector3 previousPosition_{};
    Vector3 previousEnemyPosition_{};
    std::uint64_t trackedEnemy_ = 0;
    float stepDistance_ = 0.0f;
    float enemyStepDistance_ = 0.0f;
    int previousPlayerStep_ = -1;
    int previousFemaleStep_ = -1;
};
