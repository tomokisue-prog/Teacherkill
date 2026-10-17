#pragma once
#include "raylib.h"

namespace demo {

    // 敵の状態の定義はAIクラスのヘッダーに唯一の正（Single Source of Truth）として置く
    enum class EnemyState { Idle, Chase, Dead, Attack };
    struct EnemyConfig;

    class EnemyAI {
    public:
        EnemyAI();

        Vector3 CalculateChaseDisplacement(const Vector3& currentPosition, Vector3 targetPosition, const EnemyConfig& config, float dt, float& outYaw, float currentYaw);

        EnemyState GetState() const { return state_; }
        void SetState(EnemyState state) { state_ = state; }

        bool ShouldMove() const { return shouldMove_; }
        void SetShouldMove(bool move) { shouldMove_ = move; }

        float GetAttackCooldown() const { return attackCooldown_; }
        void SetAttackCooldown(float cooldown) { attackCooldown_ = cooldown; }
        void ReduceAttackCooldown(float dt);

        void Reset();

    private:
        EnemyState state_ = EnemyState::Idle;
        bool shouldMove_ = true;
        float attackCooldown_ = 0.0f;
    };

}