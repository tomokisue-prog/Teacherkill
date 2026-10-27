#pragma once
#include "raylib.h"

namespace demo {
    // ä˘ë∂ÇÃèÛë‘ílÇÕà€éùÇµÅAå©é∏Ç¡ÇΩå„ÇÃë{çıÇññîˆÇ÷í«â¡Ç∑ÇÈÅB
    enum class EnemyState { Idle, Chase, Dead, Attack, Search };
    struct EnemyConfig;

    class EnemyAI {
    public:
        EnemyAI();
        Vector3 CalculateChaseDisplacement(const Vector3& currentPosition, Vector3 observedPosition,
            const EnemyConfig& config, float dt, float& outYaw, float currentYaw, bool targetVisible = true);
        void ReportMovement(Vector3 position, Vector3 requested, Vector3 actual, float dt);
        void FinishAttack();
        void ForgetTarget();

        EnemyState GetState() const { return state_; }
        void SetState(EnemyState state) { state_ = state; }
        bool ShouldMove() const { return shouldMove_; }
        void SetShouldMove(bool move) { shouldMove_ = move; }
        bool IsTargetVisible() const { return targetVisible_; }
        bool HasLastSeenPosition() const { return hasLastSeen_; }
        Vector3 GetLastSeenPosition() const { return lastSeenPosition_; }
        float GetSearchTimeRemaining() const { return searchTime_; }
        bool IsMovementBlocked() const { return movementBlocked_; }
        float GetAttackCooldown() const { return attackCooldown_; }
        void SetAttackCooldown(float cooldown) { attackCooldown_ = cooldown; }
        void ReduceAttackCooldown(float dt);
        void Reset();

    private:
        EnemyState state_ = EnemyState::Idle;
        bool shouldMove_ = true;
        float attackCooldown_ = 0.0f;
        bool targetVisible_ = false, hasLastSeen_ = false;
        Vector3 lastSeenPosition_{};
        float searchTime_ = 0.0f, blockedTime_ = 0.0f;
        bool movementBlocked_ = false, hiddenSinceBlocked_ = false;
        Vector3 blockedPosition_{}, blockedTarget_{};
    };
}
