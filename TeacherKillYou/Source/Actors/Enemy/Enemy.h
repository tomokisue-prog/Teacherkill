#pragma once
#include "raylib.h"
#include "EnemyAI.h"
#include "EnemyMovement.h"
#include "EnemyConfig.h" // 設定クラスをインクルード
#include <cstdint>

class Player;
class Stage;
struct EnemyCheckpoint;

namespace demo {

    using EnemyId = std::uint64_t;

    struct EnemyDamageResult {
        bool applied = false;
        int hp = 0;
        bool becameDead = false;
    };

#ifndef NDEBUG
    enum class DebugAttackBlock { Ready, Dead, NoTarget, Height, Range, Facing, Wall, Door };
    struct EnemyDebugInfo {
        EnemyConfig config;
        Vector3 origin{}, aim{}, obstruction{}, desiredMove{}, actualMove{};
        float distance = 0, cooldown = 0, attackProgress = 0, searchTime = 0;
        Vector3 sightOrigin{}, lastSeenPosition{};
        bool targetVisible = false, hasLastSeen = false, movementBlocked = false;
        bool canHit = false, wallBlocked = false, doorBlocked = false, movementLimited = false, hitReaction = false;
        DebugAttackBlock reason = DebugAttackBlock::NoTarget;
    };
#endif

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
        EnemyCheckpoint CaptureCheckpoint() const;
        void RestoreCheckpoint(const EnemyCheckpoint& data);

        EnemyId GetId() const { return id_; }
        EnemyType GetType() const { return config_.type; }
        Vector3 GetPosition() const { return movement_.GetPosition(); }
        float GetYaw() const { return yaw_; }
        BoundingBox GetBodyBounds() const { return movement_.GetBodyBounds(config_); }
        int GetHp() const { return hp_; }
        EnemyState GetState() const { return ai_.GetState(); }
        bool IsAlive() const { return ai_.GetState() != EnemyState::Dead; }
        bool IsRemovalReady() const;
#ifndef NDEBUG
        EnemyDebugInfo GetDebugInfo(const TargetInfo& target, const Stage& stage, const BoundingBox* blocker = nullptr) const;
#endif
        bool IsMoving() const { return ai_.ShouldMove() && (ai_.GetState() == EnemyState::Chase || ai_.GetState() == EnemyState::Search); }

    private:
        bool CanSeeTarget(const TargetInfo& target, const Stage& stage, const BoundingBox* blocker) const;
        bool CanHitTarget(const TargetInfo& target, const Stage& stage, const BoundingBox* blocker) const;
        float AttackDuration() const;
        float DeathDuration() const;
        float HitDuration() const;

#ifndef NDEBUG
        Vector3 debugDesiredMove_{}, debugActualMove_{};
#endif
        EnemyId id_;
        EnemyConfig config_;
        EnemyAI ai_;                 // 頭脳（AI）
        EnemyMovement movement_;     // 肉体・物理（Movement）
        // 視線判定は10Hzで行い、キャッシュには視認時の位置だけを保持する。
        float sightTimer_ = 0.0f;
        bool sightVisible_ = false;
        Vector3 observedPosition_{};
        float yaw_ = 0.0f;
        int hp_ = 100;
        float corpseTimer_ = 0.0f;
        float hitTime_ = -1.0f;        // 負値は未再生。終端姿勢を1フレーム描画してから解除する。
        float attackTime_ = 0.0f;
        bool attackHitChecked_ = false;
        int pendingAttackDamage_ = 0;
        bool attackPose_ = false;
        float animationTime_ = 0.0f;
    };

} // namespace demo
