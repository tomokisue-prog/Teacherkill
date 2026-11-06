#include "HorrorAudio.h"
#include "AudioManager.h"
#include "Player.h"
#include "PlayerCamera.h"
#include "EnemyManager.h"
#include "Stage.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

void HorrorAudio::Reset(Vector3 position)
{
    previousPosition_ = position;
    stepDistance_ = 0;
    growlTimer_ = 2;
}

void HorrorAudio::Update(float dt, const Player& player, const PlayerCamera& camera,
    const demo::EnemyManager& enemies, const Stage& stage, bool playerView)
{
    const auto position = player.GetPosition();
    const float distance = Vector2Distance({position.x, position.z}, {previousPosition_.x, previousPosition_.z});
    previousPosition_ = position;
    if (!playerView || !player.IsAlive()) { stepDistance_ = 0; return; }

    // Measure actual movement after collision. Walking into a wall, falling and teleporting are silent.
    if (player.IsGrounded() && distance > .0001f && distance < .8f) {
        stepDistance_ += distance;
        const float stride = player.IsSprinting() ? 1.35f : .95f;
        if (stepDistance_ >= stride) {
            stepDistance_ = std::fmod(stepDistance_, stride);
            AM().PlaySe(SeId::Footstep, player.IsSprinting() ? .85f : .58f, GetRandomValue(94, 106) / 100.0f);
        }
    }
    else stepDistance_ = 0;

    growlTimer_ -= (std::max)(0.0f, dt);
    if (growlTimer_ > 0 || AM().IsSePlaying(SeId::EnemyGrowl)) return;
    const demo::Enemy* nearest = nullptr;
    float nearestDistance = 12;
    for (const auto& enemy : enemies.GetEnemies()) {
        const float separation = Vector3Distance(enemy.GetPosition(), position);
        if (enemy.IsAlive() && std::fabs(enemy.GetPosition().y - position.y) < 2.0f && separation < nearestDistance) {
            nearestDistance = separation;
            nearest = &enemy;
        }
    }
    if (!nearest) { growlTimer_ = 1; return; }
    const auto listener = camera.GetPosition();
    const auto body = nearest->GetBodyBounds();
    const auto source = Vector3Add(nearest->GetPosition(), {0, (body.max.y - body.min.y) * .75f, 0});
    const auto direction = Vector3Normalize(Vector3Subtract(source, listener));
    const float rayLength = Vector3Distance(source, listener);
    const auto wall = stage.Raycast({listener, direction}, rayLength);
    const float occlusion = wall.hit && wall.distance < rayLength - .1f ? .25f : 1.0f;
    const float gain = .8f * occlusion * (1.0f - nearestDistance / 12.0f);
    const float pan = Clamp(.5f - .45f * Vector3DotProduct(direction, camera.GetRightVector()), .05f, .95f);
    AM().PlaySe(SeId::EnemyGrowl, gain, GetRandomValue(92, 104) / 100.0f, pan);
    growlTimer_ = static_cast<float>(GetRandomValue(4, 8));
}
