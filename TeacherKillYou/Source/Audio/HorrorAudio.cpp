#include "HorrorAudio.h"
#include "AudioManager.h"
#include "Player.h"
#include "PlayerCamera.h"
#include "EnemyManager.h"
#include "Stage.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace {
int NextVariation(int previous) {
    int result = GetRandomValue(0, 2);
    if (result == previous) result = (result + GetRandomValue(1, 2)) % 3;
    return result;
}
}

void HorrorAudio::Reset(Vector3 position) {
    previousPosition_ = position;
    previousEnemyPosition_ = {};
    trackedEnemy_ = 0;
    stepDistance_ = enemyStepDistance_ = 0.0f;
    previousPlayerStep_ = previousFemaleStep_ = -1;
    AM().StopChase();
    AM().StopSe(SeId::Heartbeat);
    AM().StopSe(SeId::MaleFootstep);
}

void HorrorAudio::Update(float dt, const Player& player, const PlayerCamera& camera,
    const demo::EnemyManager& enemies, const Stage& stage, bool playerView) {
    (void)dt;
    // Keep the track playing until every enemy stops pursuing or searching.
    const auto& list = enemies.GetEnemies();
    const bool detected = playerView && player.IsAlive() && std::any_of(list.begin(), list.end(), [](const demo::Enemy& enemy) {
        const auto state = enemy.GetState();
        return enemy.IsAlive() && (state == demo::EnemyState::Chase ||
            state == demo::EnemyState::Attack || state == demo::EnemyState::Search);
    });
    AM().SetChaseActive(detected);
    const auto position = player.GetPosition();
    const float distance = Vector2Distance({position.x, position.z}, {previousPosition_.x, previousPosition_.z});
    previousPosition_ = position;
    if (!playerView || !player.IsAlive()) {
        stepDistance_ = enemyStepDistance_ = 0.0f;
        trackedEnemy_ = 0;
        AM().StopSe(SeId::Heartbeat);
        AM().StopSe(SeId::MaleFootstep);
        return;
    }

    // Only play while actually walking on the floor, not while jumping or blocked.
    if (player.IsGrounded() && distance > 0.0001f && distance < 0.8f) {
        stepDistance_ += distance;
        const float stride = player.IsSprinting() ? 1.35f : 0.95f;
        if (stepDistance_ >= stride) {
            stepDistance_ = std::fmod(stepDistance_, stride);
            previousPlayerStep_ = NextVariation(previousPlayerStep_);
            constexpr SeId playerSteps[] = {SeId::PlayerFootstep1, SeId::PlayerFootstep2, SeId::PlayerFootstep3};
            AM().PlaySe(playerSteps[previousPlayerStep_], player.IsSprinting() ? 0.85f : 0.58f,
                GetRandomValue(96, 104) / 100.0f);
        }
    } else stepDistance_ = 0.0f;

    const float hp = player.GetMaxHp() > 0 ? static_cast<float>(player.GetHp()) / player.GetMaxHp() : 1.0f;
    if (hp <= 0.4f) {
        const float gain = 0.25f + 0.6f * (1.0f - Clamp(hp / 0.4f, 0.0f, 1.0f));
        if (!AM().IsSePlaying(SeId::Heartbeat)) AM().PlaySe(SeId::Heartbeat, gain);
        else AM().SetSeGain(SeId::Heartbeat, gain);
    } else AM().StopSe(SeId::Heartbeat);

    // Use the nearest moving enemy so distant crowds do not drown out the game.
    const demo::Enemy* nearest = nullptr;
    float nearestDistance = 12.0f;
    for (const auto& enemy : enemies.GetEnemies()) {
        const float separation = Vector3Distance(enemy.GetPosition(), position);
        if (enemy.IsAlive() && enemy.IsMoving() && std::fabs(enemy.GetPosition().y - position.y) < 2.0f && separation < nearestDistance) {
            nearestDistance = separation;
            nearest = &enemy;
        }
    }
    if (!nearest) {
        trackedEnemy_ = 0;
        enemyStepDistance_ = 0.0f;
        AM().StopSe(SeId::MaleFootstep);
        return;
    }
    const auto enemyPosition = nearest->GetPosition();
    if (trackedEnemy_ != nearest->GetId()) {
        trackedEnemy_ = nearest->GetId();
        previousEnemyPosition_ = enemyPosition;
        enemyStepDistance_ = 0.0f;
        AM().StopSe(SeId::MaleFootstep);
        return;
    }
    const float moved = Vector2Distance({enemyPosition.x, enemyPosition.z}, {previousEnemyPosition_.x, previousEnemyPosition_.z});
    previousEnemyPosition_ = enemyPosition;
    if (moved <= 0.0001f || moved > 0.8f) {
        enemyStepDistance_ = 0.0f;
        AM().StopSe(SeId::MaleFootstep);
        return;
    }
    enemyStepDistance_ += moved;
    const bool female = nearest->GetType() == demo::EnemyType::FemaleZombie;
    const float stride = female ? 0.8f : 1.0f;
    if (enemyStepDistance_ < stride) return;
    if (!female && AM().IsSePlaying(SeId::MaleFootstep)) return;
    enemyStepDistance_ = std::fmod(enemyStepDistance_, stride);

    const Vector3 listener = camera.GetPosition();
    const Vector3 source = Vector3Add(enemyPosition, {0, 0.5f, 0});
    const Vector3 delta = Vector3Subtract(source, listener);
    const float rayLength = Vector3Length(delta);
    const Vector3 direction = rayLength > 0.001f ? Vector3Scale(delta, 1.0f / rayLength) : Vector3{};
    float occlusion = 1.0f;
    if (rayLength > 0.001f) {
        const auto wall = stage.Raycast({listener, direction}, rayLength);
        if (wall.hit && wall.distance < rayLength - 0.1f) occlusion = 0.3f;
    }
    const float gain = 0.8f * occlusion * (1.0f - nearestDistance / 12.0f);
    const float pan = Clamp(0.5f - 0.45f * Vector3DotProduct(direction, camera.GetRightVector()), 0.05f, 0.95f);
    if (female) {
        previousFemaleStep_ = NextVariation(previousFemaleStep_);
        constexpr SeId steps[] = {SeId::FemaleFootstep1, SeId::FemaleFootstep2, SeId::FemaleFootstep3};
        AM().PlaySe(steps[previousFemaleStep_], gain, GetRandomValue(96, 104) / 100.0f, pan);
    } else AM().PlaySe(SeId::MaleFootstep, gain, 1.0f, pan);
}
