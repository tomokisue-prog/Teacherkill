#pragma once
#include "raylib.h"
#include "EnemyAI.h"
#include "EnemyMovement.h"
#include "EnemyConfig.h" // 設定クラスをインクルード
#include <cstdint>

class Player;
class Stage;

namespace demo {

    using EnemyId = std::uint64_t;

    struct EnemyDamageResult {
        bool applied = false;
        int hp = 0;
        bool becameDead = false;
    };

    struct TargetInfo {
        Vector3 position = {};
        BoundingBox bodyBounds = {};
        bool alive = false;
        bool valid = false;

        static TargetInfo FromPlayer(const Player& player);
    };

    class Enemy {
    public:
        Enemy(EnemyId id, const EnemyConfig& config, Vector3 position, float yaw = 0.0f);
        void Reset(Vector3 position, float yaw = 0.0f);
        void Update(float dt, const TargetInfo& target, const Stage& stage, const BoundingBox* blocker = nullptr);
        int ConsumeAttackDamage();
        float GetDamageFlashStrength() const;
        EnemyDamageResult TakeDamage(int amount);
        void SetConfig(const EnemyConfig& config);
        void Draw() const;

        EnemyId GetId() const { return id_; }
        Vector3 GetPosition() const { return movement_.GetPosition(); }
        float GetYaw() const { return yaw_; }
        BoundingBox GetBodyBounds() const { return movement_.GetBodyBounds(config_); }
        int GetHp() const { return hp_; }
        EnemyState GetState() const { return ai_.GetState(); }
        bool IsAlive() const { return ai_.GetState() != EnemyState::Dead; }
        bool IsRemovalReady() const;
        bool IsMoving() const { return ai_.ShouldMove() && ai_.GetState() == EnemyState::Chase; }

    private:
        bool CanHitTarget(const TargetInfo& target, const Stage& stage, const BoundingBox* blocker) const;
        float AttackDuration() const;
        float DeathDuration() const;

        EnemyId id_;
        EnemyConfig config_;
        EnemyAI ai_;                 // 頭脳（AI）
        EnemyMovement movement_;     // 肉体・物理（Movement）
        float yaw_ = 0.0f;
        int hp_ = 100;
        float corpseTimer_ = 0.0f;
        float attackTime_ = 0.0f;
        bool attackHitChecked_ = false;
        int pendingAttackDamage_ = 0;
        float damageFlashTimer_ = 0.0f;
        bool attackPose_ = false;
        float animationTime_ = 0.0f;
    };

} // namespace demo