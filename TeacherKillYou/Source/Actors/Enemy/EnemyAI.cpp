#include "EnemyAI.h"
#include "EnemyConfig.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float ContactEpsilon = 0.001f;
    constexpr float SearchDuration = 3.0f;
    constexpr float BlockedDuration = 0.75f;
    constexpr float SearchArrivalDistance = 0.35f;

    float PlanarDistance(Vector3 a, Vector3 b) {
        return Vector2Distance({a.x, a.z}, {b.x, b.z});
    }
    float WrapAngle(float angle) {
        return std::isfinite(angle) ? std::remainder(angle, 2.0f * PI) : 0.0f;
    }
}

namespace demo {
    EnemyAI::EnemyAI() = default;

    void EnemyAI::Reset() {
        *this = EnemyAI{};
    }

    void EnemyAI::ForgetTarget() {
        state_ = EnemyState::Idle;
        shouldMove_ = false;
        targetVisible_ = hasLastSeen_ = false;
        searchTime_ = blockedTime_ = 0.0f;
        movementBlocked_ = hiddenSinceBlocked_ = false;
    }

    void EnemyAI::ReduceAttackCooldown(float dt) {
        attackCooldown_ = (std::max)(0.0f, attackCooldown_ - dt);
    }

    void EnemyAI::FinishAttack() {
        state_ = targetVisible_ ? EnemyState::Chase : hasLastSeen_ ? EnemyState::Search : EnemyState::Idle;
        shouldMove_ = state_ != EnemyState::Idle;
    }

    Vector3 EnemyAI::CalculateChaseDisplacement(const Vector3& currentPosition, Vector3 observedPosition,
        const EnemyConfig& config, float dt, float& outYaw, float currentYaw, bool targetVisible) {
        outYaw = currentYaw;
        if (state_ == EnemyState::Dead) return {};

        // 通れない同じ相手への再突進を防ぐ。相手の移動・再発見後は再試行できる。
        if (movementBlocked_) {
            if (!targetVisible) hiddenSinceBlocked_ = true;
            if (targetVisible && (hiddenSinceBlocked_ || PlanarDistance(observedPosition, blockedTarget_) > 0.6f ||
                PlanarDistance(currentPosition, blockedPosition_) > 0.25f)) {
                movementBlocked_ = hiddenSinceBlocked_ = false;
            }
            else targetVisible = false;
        }
        const float range = state_ == EnemyState::Idle && !hasLastSeen_ ? config.detectionRange : config.loseRange;
        targetVisible_ = targetVisible && PlanarDistance(observedPosition, currentPosition) <= range;
        if (targetVisible_) {
            lastSeenPosition_ = observedPosition;
            hasLastSeen_ = true;
            searchTime_ = SearchDuration;
        }
        else {
            searchTime_ = (std::max)(0.0f, searchTime_ - dt);
            if (searchTime_ <= 0.0f) hasLastSeen_ = false;
        }

        // 攻撃中も記憶は更新するが、アニメーション終了まで移動しない。
        if (state_ == EnemyState::Attack) return {};
        const auto previousState = state_;
        state_ = targetVisible_ ? EnemyState::Chase : hasLastSeen_ ? EnemyState::Search : EnemyState::Idle;
        if (state_ == EnemyState::Idle) { shouldMove_ = false; return {}; }
        if (state_ != previousState) shouldMove_ = true;

        // 隠れた標的の現在位置は使わず、最後に視認した位置だけを目的地にする。
        Vector3 delta = Vector3Subtract(lastSeenPosition_, currentPosition);
        delta.y = 0.0f;
        const float distance = Vector3Length(delta);
        if (distance > ContactEpsilon) {
            const float desiredYaw = std::atan2(delta.x, -delta.z);
            const float maxTurn = config.turnSpeed * dt;
            outYaw = WrapAngle(currentYaw + Clamp(WrapAngle(desiredYaw - currentYaw), -maxTurn, maxTurn));
        }
        const float stopDistance = state_ == EnemyState::Search ? SearchArrivalDistance : config.stopDistance;
        if (state_ == EnemyState::Search) shouldMove_ = distance > stopDistance;
        else if (distance <= stopDistance) shouldMove_ = false;
        else if (distance >= config.resumeDistance) shouldMove_ = true;
        if (!shouldMove_ || distance <= ContactEpsilon) return {};
        const float travel = (std::min)(config.moveSpeed * dt, distance - stopDistance);
        return Vector3Scale(delta, travel / distance);
    }

    void EnemyAI::ReportMovement(Vector3 position, Vector3 requested, Vector3 actual, float dt) {
        requested.y = actual.y = 0.0f;
        const float requestedLength = Vector3Length(requested);
        if ((state_ != EnemyState::Chase && state_ != EnemyState::Search) || requestedLength <= ContactEpsilon) {
            blockedTime_ = 0.0f;
            return;
        }
        const float progress = Vector3DotProduct(actual, Vector3Scale(requested, 1.0f / requestedLength));
        blockedTime_ = progress < requestedLength * 0.1f ? blockedTime_ + dt : 0.0f;
        if (blockedTime_ < BlockedDuration) return;
        blockedPosition_ = position;
        blockedTarget_ = lastSeenPosition_;
        movementBlocked_ = true;
        hiddenSinceBlocked_ = false;
        hasLastSeen_ = targetVisible_ = shouldMove_ = false;
        searchTime_ = blockedTime_ = 0.0f;
        state_ = EnemyState::Idle;
    }
}
