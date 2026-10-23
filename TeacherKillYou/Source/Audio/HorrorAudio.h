#pragma once
#include "raylib.h"
class Player;
class PlayerCamera;
class Stage;
namespace demo { class EnemyManager; }

// Gameplay timing stays here; AudioManager owns the loaded sounds and mixer state.
class HorrorAudio {
public:
    void Reset(Vector3 position);
    void Update(float deltaTime, const Player& player, const PlayerCamera& camera,
        const demo::EnemyManager& enemies, const Stage& stage, bool playerView);
private:
    Vector3 previousPosition_{};
    float stepDistance_ = 0;
    float growlTimer_ = 2;
};
