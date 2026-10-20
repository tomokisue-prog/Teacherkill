#include "Enemy.h"
#include "AudioManager.h"
#include "Player.h"
#include "Stage.h"
#include "ResourceManager.h"
#include "HorrorLighting.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace {

    constexpr float ContactEpsilon = 0.001f;
    constexpr float AnimationFrameDuration = 0.017f;

    struct EnemyAssets { const char* model; const char* attack; const char* death; };
    const EnemyAssets& AssetsFor(demo::EnemyType type) {
        static constexpr EnemyAssets monster{ ResourceKeys::Model_Enemy, ResourceKeys::Animation_EnemyAttack, ResourceKeys::Animation_EnemyDeath };
        static constexpr EnemyAssets female{ ResourceKeys::Model_FemaleZombie, ResourceKeys::Animation_FemaleZombieAttack, ResourceKeys::Animation_FemaleZombieDeath };
        return type == demo::EnemyType::FemaleZombie ? female : monster;
    }

    bool IsFinite(Vector3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    float WrapAngle(float angle) {
        return std::isfinite(angle) ? std::remainder(angle, 2.0f * PI) : 0.0f;
    }

}

namespace demo {

    TargetInfo TargetInfo::FromPlayer(const Player& player) {
        TargetInfo target;
        target.position = player.GetPosition();
        target.bodyBounds = { { target.position.x - player.GetRadius(), target.position.y, target.position.z - player.GetRadius() },
                              { target.position.x + player.GetRadius(), target.position.y + player.GetHeight(), target.position.z + player.GetRadius() } };
        target.alive = player.IsAlive();
        target.valid = IsFinite(target.position);
        return target;
    }

    Enemy::Enemy(EnemyId id, const EnemyConfig& config, Vector3 position, float yaw) : id_(id) {
        SetConfig(config);
        Reset(position, yaw);
    }

    void Enemy::SetConfig(const EnemyConfig& config) {
        config_ = config;
        config_.Validate(); // EnemyConfig側のバリデーション関数を呼び出すだけ！
        hp_ = (std::min)(hp_, config_.maxHp);
    }

    void Enemy::Reset(Vector3 position, float yaw) {
        movement_.Reset(position, 0.0f);
        yaw_ = WrapAngle(yaw);
        hp_ = config_.maxHp;
        ai_.Reset();
        corpseTimer_ = 0.0f;
        animationTime_ = 0.0f;
        attackTime_ = 0.0f;
        attackHitChecked_ = false;
        attackPose_ = false;
        pendingAttackDamage_ = 0;
    }

    float Enemy::AttackDuration() const {
        const auto clip = RM().GetModelAnimation(AssetsFor(config_.type).attack);
        return clip.frameCount > 1 ? (clip.frameCount - 1) * AnimationFrameDuration : 0.9f;
    }

    float Enemy::DeathDuration() const {
        const auto clip = RM().GetModelAnimation(AssetsFor(config_.type).death);
        return clip.frameCount > 1 ? (clip.frameCount - 1) * AnimationFrameDuration : 0.0f;
    }

    bool Enemy::CanHitTarget(const TargetInfo& target, const Stage& stage, const BoundingBox* blocker) const {
        if (!target.valid || !target.alive || !IsFinite(target.position)) return false;
        const auto body = GetBodyBounds();
        if (body.max.y <= target.bodyBounds.min.y || body.min.y >= target.bodyBounds.max.y) return false;
        Vector3 delta = Vector3Subtract(target.position, movement_.GetPosition());
        delta.y = 0.0f;
        const float distance = Vector3Length(delta);
        if (distance > config_.attackRange) return false;
        if (distance > ContactEpsilon && Vector3DotProduct(Vector3Scale(delta, 1.0f / distance),
            { std::sin(yaw_), 0, -std::cos(yaw_) }) < 0.7f) return false;
        const Vector3 origin = Vector3Add(movement_.GetPosition(), { 0, config_.bodyHeight * 0.6f, 0 });
        const Vector3 aim = { target.position.x, (target.bodyBounds.min.y + target.bodyBounds.max.y) * 0.5f, target.position.z };
        const Vector3 rayDelta = Vector3Subtract(aim, origin);
        const float length = Vector3Length(rayDelta);
        if (length <= ContactEpsilon) return true;
        const Ray ray = { origin, Vector3Scale(rayDelta, 1.0f / length) };
        const auto wall = stage.Raycast(ray, length);
        if (wall.hit && wall.distance < length - ContactEpsilon) return false;
        if (blocker) {
            if (CheckCollisionBoxSphere(*blocker, origin, ContactEpsilon)) return false;
            const auto door = GetRayCollisionBox(ray, *blocker);
            if (door.hit && door.distance < length - ContactEpsilon) return false;
        }
        return true;
    }

    void Enemy::Update(float dt, const TargetInfo& target, const Stage& stage, const BoundingBox* blocker) {
        pendingAttackDamage_ = 0;
        if (!std::isfinite(dt) || dt <= 0.0f) return;

        if (!IsAlive()) {
            const float duration = DeathDuration();
            corpseTimer_ = (std::min)(duration + config_.corpseDuration, corpseTimer_ + dt);
            if (duration > 0.0f) animationTime_ = (std::min)(duration, corpseTimer_);
            return;
        }

        dt = (std::min)(dt, 0.25f);
        ai_.ReduceAttackCooldown(dt);

        const bool validTarget = target.valid && target.alive && IsFinite(target.position);
        if (!validTarget) {
            ai_.SetState(EnemyState::Idle);
            ai_.SetShouldMove(true);
            attackPose_ = false;
            animationTime_ = 0.0f;
        }

        const bool attacking = ai_.GetState() == EnemyState::Attack;
        const Vector3 previousPosition = movement_.GetPosition();

        float newYaw = yaw_;
        Vector3 displacement = {};
        if (validTarget && !attacking) {
            displacement = ai_.CalculateChaseDisplacement(movement_.GetPosition(), target.position, config_, dt, newYaw, yaw_);
            yaw_ = newYaw;
        }

        movement_.Update(dt, displacement, validTarget ? &target.bodyBounds : nullptr, stage, config_);

        if (attacking) {
            attackTime_ += dt;
            animationTime_ = attackTime_;
            const float duration = AttackDuration();
            if (!attackHitChecked_ && attackTime_ >= duration * config_.attackHitFraction) {
                attackHitChecked_ = true;
                AM().PlaySe(SeId::EnemyAttack);
                if (CanHitTarget(target, stage, blocker)) pendingAttackDamage_ = config_.attackDamage;
            }
            if (attackTime_ >= duration) {
                ai_.SetState(EnemyState::Chase);
                attackPose_ = false;
                animationTime_ = 0.0f;
                ai_.SetAttackCooldown(config_.attackRecovery);
            }
            return;
        }

        if (ai_.GetState() == EnemyState::Chase && ai_.GetAttackCooldown() <= 0.0f && config_.attackDamage > 0 && CanHitTarget(target, stage, blocker)) {
            ai_.SetState(EnemyState::Attack);
            ai_.SetShouldMove(false);
            attackPose_ = true;
            attackTime_ = animationTime_ = 0.0f;
            attackHitChecked_ = false;
            return;
        }

        attackPose_ = false;
        const Vector3 currentPos = movement_.GetPosition();
        const bool moved = currentPos.x != previousPosition.x || currentPos.z != previousPosition.z;
        const char* walkAnimKey = AssetsFor(config_.type).model;
        const ModelAnimation animation = RM().GetModelAnimation(walkAnimKey);

        if (IsMoving() && moved && animation.frameCount > 0) {
            // 時間を進めてループ再生させる
            animationTime_ += dt;
            const float totalDuration = animation.frameCount * AnimationFrameDuration;
            if (animationTime_ >= totalDuration) {
                animationTime_ = std::fmod(animationTime_, totalDuration);
            }
        }
        else {
            // 停止時は0（立ちポーズ）に戻す
            animationTime_ = 0.0f;
        }
    }

    int Enemy::ConsumeAttackDamage() {
        const int damage = pendingAttackDamage_;
        pendingAttackDamage_ = 0;
        return damage;
    }

    float Enemy::GetDamageFlashStrength() const {
        // 呼び出し側との互換性を保つ。被弾表示は命中位置の血しぶきに置き換えた。
        return 0.0f;
    }

    EnemyDamageResult Enemy::TakeDamage(int amount) {
        if (amount <= 0 || !IsAlive()) return { false, hp_, false };
        hp_ = amount >= hp_ ? 0 : hp_ - amount;
        const bool died = hp_ == 0;
        AM().PlaySe(died ? SeId::EnemyDeath : SeId::EnemyHit);
        if (died) {
            ai_.SetState(EnemyState::Dead);
            ai_.SetShouldMove(false);
            corpseTimer_ = 0.0f;
            pendingAttackDamage_ = 0;
            ai_.SetAttackCooldown(0.0f);
            if (RM().GetModelAnimation(AssetsFor(config_.type).death).frameCount > 0) {
                animationTime_ = 0.0f;
                attackPose_ = false;
            }
        }
        return { true, hp_, died };
    }

    bool Enemy::IsRemovalReady() const {
        return !IsAlive() && corpseTimer_ >= DeathDuration() + config_.corpseDuration;
    }

    void Enemy::Draw() const {
        const auto& assets = AssetsFor(config_.type);
        Model model = RM().GetModel(assets.model);
        const Vector3 position = movement_.GetPosition();
        if (model.meshCount > 0) {
            const bool deathPose = !IsAlive() && RM().GetModelAnimation(assets.death).frameCount > 0;
            const char* clipKey = deathPose ? assets.death :
                attackPose_ && RM().GetModelAnimation(assets.attack).frameCount > 0 ? assets.attack : assets.model;
            const ModelAnimation animation = RM().GetModelAnimation(clipKey);
            if (animation.frameCount > 0) {
                const int elapsedFrame = static_cast<int>(animationTime_ / AnimationFrameDuration);
                const int frame = deathPose && corpseTimer_ >= DeathDuration() ? animation.frameCount - 1 :
                    deathPose || attackPose_ ? (std::min)(elapsedFrame, animation.frameCount - 1) : elapsedFrame % animation.frameCount;
                RM().ApplyModelAnimation(clipKey, frame);
            }
            const BoundingBox bounds = RM().GetModelBounds(assets.model);
            const float height = bounds.max.y - bounds.min.y;
            const float scale = height > ContactEpsilon ? config_.bodyHeight / height : 1.0f;
            model.transform = MatrixTranslate(-(bounds.min.x + bounds.max.x) * 0.5f,
                -bounds.min.y, -(bounds.min.z + bounds.max.z) * 0.5f);
            const Color tint = IsAlive() ? WHITE : Color{ 110, 110, 110, 255 };
            DrawHorrorModelEx(model, position, { 0, 1, 0 }, 180.0f - yaw_ * RAD2DEG, { scale, scale, scale }, tint);
            return;
        }
        Color color = { 57, 187, 173, 255 };
        if (!IsAlive()) color = { 91, 99, 110, 255 };
        else if (ai_.GetState() == EnemyState::Chase) color = { 242, 146, 65, 255 };

        const float radius = config_.bodyRadius;
        const Vector3 center = Vector3Add(position, { 0, config_.bodyHeight * 0.5f, 0 });
        const Vector3 size = { radius * 2, config_.bodyHeight, radius * 2 };
        DrawCubeV(center, size, color);
        DrawCubeWiresV(center, size, Fade(BLACK, 0.5f));
    }

}
