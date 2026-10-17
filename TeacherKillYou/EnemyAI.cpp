#include "EnemyAI.h"
#include "Enemy.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float ContactEpsilon = 0.001f;

    float WrapAngle(float angle) {
        return std::isfinite(angle) ? std::remainder(angle, 2.0f * PI) : 0.0f;
    }
}

namespace demo {

    EnemyAI::EnemyAI() = default;

    void EnemyAI::Reset() {
        state_ = EnemyState::Idle;
        shouldMove_ = true;
        attackCooldown_ = 0.0f;
    }

    void EnemyAI::ReduceAttackCooldown(float dt) {
        attackCooldown_ = (std::max)(0.0f, attackCooldown_ - dt);
    }

    Vector3 EnemyAI::CalculateChaseDisplacement(const Vector3& currentPosition, Vector3 targetPosition, const EnemyConfig& config, float dt, float& outYaw, float currentYaw) {
        Vector3 delta = Vector3Subtract(targetPosition, currentPosition);
        // 追跡距離はXZ平面で評価し、目線や足場の高さの差を含めない。
        delta.y = 0.0f;
        const float distance = Vector3Length(delta);

        // 検知距離と喪失距離の間は直前の状態を保持する（ヒステリシス）。
        if (state_ == EnemyState::Idle && distance <= config.detectionRange) {
            state_ = EnemyState::Chase;
            shouldMove_ = true;
        }
        else if (state_ == EnemyState::Chase && distance > config.loseRange) {
            state_ = EnemyState::Idle;
            shouldMove_ = true;
        }

        if (state_ != EnemyState::Chase) return {};

        if (distance > ContactEpsilon) {
            // 前方が-Zなのでatan2(x, -z)。旋回量はturnSpeed*dt以内に制限する。
            const float desiredYaw = std::atan2(delta.x, -delta.z);
            const float maxTurn = config.turnSpeed * dt;
            const float turn = Clamp(WrapAngle(desiredYaw - currentYaw), -maxTurn, maxTurn);
            outYaw = WrapAngle(currentYaw + turn);
        }
        else {
            outYaw = currentYaw;
        }

        // 停止後はresumeDistanceまで離れない限り再開しない。
        if (distance <= config.stopDistance) shouldMove_ = false;
        else if (distance >= config.resumeDistance) shouldMove_ = true;

        if (!shouldMove_ || distance <= ContactEpsilon) return {};

        // 停止距離を越えて接近しないよう、残距離を移動量の上限にする。
        const float travel = (std::min)(config.moveSpeed * dt, distance - config.stopDistance);
        return Vector3Scale(delta, travel / distance);
    }

} // namespace demo