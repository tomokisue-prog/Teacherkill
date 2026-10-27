#include "Enemy.h"
#include "CheckpointData.h"
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

    struct EnemyAssets { const char* model; const char* attack; const char* death; const char* hit; };
    const EnemyAssets& AssetsFor(demo::EnemyType type) {
        static constexpr EnemyAssets monster{ ResourceKeys::Model_Enemy, ResourceKeys::Animation_EnemyAttack, ResourceKeys::Animation_EnemyDeath, nullptr };
        static constexpr EnemyAssets female{ ResourceKeys::Model_FemaleZombie, ResourceKeys::Animation_FemaleZombieAttack, ResourceKeys::Animation_FemaleZombieDeath, ResourceKeys::Animation_FemaleZombieHit };
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
#ifndef NDEBUG
        debugDesiredMove_ = debugActualMove_ = {};
#endif
        movement_.Reset(position, 0.0f);
        yaw_ = WrapAngle(yaw);
        hp_ = config_.maxHp;
        ai_.Reset();
        sightTimer_ = 0.0f;
        sightVisible_ = false;
        observedPosition_ = {};
        corpseTimer_ = 0.0f;
        hitTime_ = -1.0f;
        animationTime_ = 0.0f;
        attackTime_ = 0.0f;
        attackHitChecked_ = false;
        attackPose_ = false;
        pendingAttackDamage_ = 0;
    }

    float Enemy::AttackDuration() const {
        const auto clip = RM().GetModelAnimation(AssetsFor(config_.type).attack);
        return (clip.frameCount > 1 ? (clip.frameCount - 1) * AnimationFrameDuration : 0.9f) / config_.animationSpeed;
    }

    float Enemy::DeathDuration() const {
        const auto clip = RM().GetModelAnimation(AssetsFor(config_.type).death);
        return clip.frameCount > 1 ? (clip.frameCount - 1) * AnimationFrameDuration / config_.animationSpeed : 0.0f;
    }

    float Enemy::HitDuration() const {
        const char* key = AssetsFor(config_.type).hit;
        if (!key) return 0.0f;
        const auto clip = RM().GetModelAnimation(key);
        return clip.frameCount > 1 ? (clip.frameCount - 1) * AnimationFrameDuration / config_.animationSpeed : 0.0f;
    }

    bool Enemy::CanSeeTarget(const TargetInfo& target, const Stage& stage, const BoundingBox* blocker) const {
        if (!target.valid || !target.alive || !IsFinite(target.position)) return false;
        const Vector3 position = GetPosition();
        const float range = ai_.GetState() == EnemyState::Idle && !ai_.HasLastSeenPosition() ? config_.detectionRange : config_.loseRange;
        if (Vector2Distance({position.x, position.z}, {target.position.x, target.position.z}) > range) return false;
        const Vector3 origin = Vector3Add(position, {0, config_.bodyHeight * .85f, 0});
        if (blocker && CheckCollisionBoxSphere(*blocker, origin, ContactEpsilon)) return false;
        // 胴と頭を確認する。低い障害物の上から見える場合は視認を許可する。
        for (float fraction : {.55f, .9f}) {
            const Vector3 aim{target.position.x, Lerp(target.bodyBounds.min.y, target.bodyBounds.max.y, fraction), target.position.z};
            const Vector3 delta = Vector3Subtract(aim, origin);
            const float length = Vector3Length(delta);
            if (length <= ContactEpsilon) return true;
            const Ray ray{origin, Vector3Scale(delta, 1.0f / length)};
            if (blocker) {
                const auto door = GetRayCollisionBox(ray, *blocker);
                if (door.hit && door.distance < length - ContactEpsilon) continue;
            }
            const auto wall = stage.Raycast(ray, length);
            if (!wall.hit || wall.distance >= length - ContactEpsilon) return true;
        }
        return false;
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
#ifndef NDEBUG
        debugDesiredMove_ = debugActualMove_ = {};
#endif

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
            ai_.ForgetTarget();
            sightTimer_ = 0.0f;
            sightVisible_ = false;
            attackPose_ = false;
            animationTime_ = 0.0f;
        }
        else {
            sightTimer_ -= dt;
            if (sightTimer_ <= 0.0f) {
                sightVisible_ = CanSeeTarget(target, stage, blocker);
                if (sightVisible_) observedPosition_ = target.position;
                sightTimer_ = 0.1f;
            }
        }
        float newYaw = yaw_;
        Vector3 displacement{};
        if (validTarget) {
            displacement = ai_.CalculateChaseDisplacement(GetPosition(), observedPosition_, config_, dt, newYaw, yaw_, sightVisible_);
        }

        if (hitTime_ >= 0.0f && hitTime_ < HitDuration()) {
            // 被弾中は追跡と攻撃を止める。地面との接触と重力は更新を続ける。
            hitTime_ = (std::min)(HitDuration(), hitTime_ + dt);
            ai_.SetShouldMove(false);
            movement_.Update(dt, {}, validTarget ? &target.bodyBounds : nullptr, stage, config_, blocker);
            return;
        }
        hitTime_ = -1.0f;

        const bool attacking = ai_.GetState() == EnemyState::Attack;
        const Vector3 previousPosition = movement_.GetPosition();

        if (!attacking) yaw_ = newYaw;
        movement_.Update(dt, displacement, validTarget ? &target.bodyBounds : nullptr, stage, config_, blocker);
        const Vector3 actualMove = Vector3Subtract(GetPosition(), previousPosition);
        ai_.ReportMovement(GetPosition(), displacement, actualMove, dt);
#ifndef NDEBUG
        debugDesiredMove_ = displacement;
        debugActualMove_ = Vector3Subtract(movement_.GetPosition(), previousPosition);
#endif

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
                ai_.FinishAttack();
                attackPose_ = false;
                animationTime_ = 0.0f;
                ai_.SetAttackCooldown(config_.attackRecovery);
            }
            return;
        }

        if (ai_.IsTargetVisible() && ai_.GetState() == EnemyState::Chase && ai_.GetAttackCooldown() <= 0.0f && config_.attackDamage > 0 && CanHitTarget(target, stage, blocker)) {
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
            const float totalDuration = animation.frameCount * AnimationFrameDuration / config_.animationSpeed;
            if (animationTime_ >= totalDuration) {
                animationTime_ = std::fmod(animationTime_, totalDuration);
            }
        }
        else {
            // 停止時は0（立ちポーズ）に戻す
            animationTime_ = 0.0f;
        }
    }

#ifndef NDEBUG
    EnemyDebugInfo Enemy::GetDebugInfo(const TargetInfo& target, const Stage& stage, const BoundingBox* blocker) const {
        EnemyDebugInfo info;
        info.config = config_; info.cooldown = ai_.GetAttackCooldown();
        info.targetVisible = IsAlive() && sightVisible_; info.hasLastSeen = ai_.HasLastSeenPosition();
        info.lastSeenPosition = ai_.GetLastSeenPosition(); info.searchTime = ai_.GetSearchTimeRemaining();
        info.movementBlocked = ai_.IsMovementBlocked();
        info.sightOrigin = Vector3Add(GetPosition(), {0, config_.bodyHeight * .85f, 0});
        info.desiredMove = debugDesiredMove_; info.actualMove = debugActualMove_;
        const Vector2 desired{info.desiredMove.x,info.desiredMove.z}, actual{info.actualMove.x,info.actualMove.z};
        info.movementLimited = Vector2Length(desired) > .001f && Vector2Distance(desired,actual) > .002f;
        info.hitReaction = hitTime_ >= 0 && hitTime_ < HitDuration();
        info.attackProgress = ai_.GetState() == EnemyState::Attack ? Clamp(attackTime_ / AttackDuration(),0,1) : 0;
        info.origin = Vector3Add(GetPosition(),{0,config_.bodyHeight * .6f,0});
        info.aim = {target.position.x,(target.bodyBounds.min.y+target.bodyBounds.max.y)*.5f,target.position.z};
        if (!IsAlive()) { info.reason=DebugAttackBlock::Dead; return info; }
        if (!target.valid || !target.alive || !IsFinite(target.position)) return info;
        Vector3 delta=Vector3Subtract(target.position,GetPosition());delta.y=0;info.distance=Vector3Length(delta);
        const Vector3 sight=Vector3Subtract(info.aim,info.origin);const float length=Vector3Length(sight);
        if(length>ContactEpsilon) {
            const Ray ray{info.origin,Vector3Scale(sight,1/length)};const auto wall=stage.Raycast(ray,length);
            info.wallBlocked=wall.hit && wall.distance<length-ContactEpsilon;
            if(info.wallBlocked) info.obstruction=wall.point;
            if(blocker) {
                const bool inside=CheckCollisionBoxSphere(*blocker,info.origin,ContactEpsilon);const auto door=GetRayCollisionBox(ray,*blocker);
                info.doorBlocked=inside || (door.hit && door.distance<length-ContactEpsilon);
                if(info.doorBlocked && (!info.wallBlocked || door.distance<wall.distance)) info.obstruction=inside?info.origin:door.point;
            }
        }
        // 結果は実際の攻撃判定から取得する。表示用の理由は同じ条件順で分類する。
        info.canHit=CanHitTarget(target,stage,blocker);
        const auto body=GetBodyBounds();
        if(body.max.y<=target.bodyBounds.min.y || body.min.y>=target.bodyBounds.max.y) info.reason=DebugAttackBlock::Height;
        else if(info.distance>config_.attackRange) info.reason=DebugAttackBlock::Range;
        else if(info.distance>ContactEpsilon && Vector3DotProduct(Vector3Scale(delta,1/info.distance),{std::sin(yaw_),0,-std::cos(yaw_)})<.7f) info.reason=DebugAttackBlock::Facing;
        else if(info.wallBlocked) info.reason=DebugAttackBlock::Wall;
        else if(info.doorBlocked) info.reason=DebugAttackBlock::Door;
        else info.reason=DebugAttackBlock::Ready;
        return info;
    }
#endif

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
        const bool startHit = !died && hitTime_ < 0.0f && HitDuration() > 0.0f;
        if (died) hitTime_ = -1.0f;
        // 再被弾でもHPとSEは反映するが、再生中の被弾クリップは巻き戻さない。
        else if (startHit) hitTime_ = 0.0f;
        if (startHit) {
            // 途中の攻撃を中断し、被弾姿勢のまま攻撃判定だけが出ることを防ぐ。
            pendingAttackDamage_ = 0;
            attackPose_ = false;
            attackTime_ = 0.0f;
            animationTime_ = 0.0f;
            if (ai_.GetState() == EnemyState::Attack) {
                ai_.SetState(EnemyState::Chase);
                ai_.SetAttackCooldown(config_.attackRecovery);
            }
        }
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
            const bool hitPose = IsAlive() && hitTime_ >= 0.0f && assets.hit;
            const char* clipKey = deathPose ? assets.death : hitPose ? assets.hit :
                attackPose_ && RM().GetModelAnimation(assets.attack).frameCount > 0 ? assets.attack : assets.model;
            const ModelAnimation animation = RM().GetModelAnimation(clipKey);
            if (animation.frameCount > 0) {
                const float poseTime = hitPose ? hitTime_ : animationTime_;
                const int elapsedFrame = static_cast<int>(poseTime * config_.animationSpeed / AnimationFrameDuration);
                const int frame = deathPose && corpseTimer_ >= DeathDuration() ? animation.frameCount - 1 :
                    deathPose || hitPose || attackPose_ ? (std::min)(elapsedFrame, animation.frameCount - 1) : elapsedFrame % animation.frameCount;
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
        else if (ai_.GetState() == EnemyState::Chase || ai_.GetState() == EnemyState::Search) color = { 242, 146, 65, 255 };

        const float radius = config_.bodyRadius;
        const Vector3 center = Vector3Add(position, { 0, config_.bodyHeight * 0.5f, 0 });
        const Vector3 size = { radius * 2, config_.bodyHeight, radius * 2 };
        DrawCubeV(center, size, color);
        DrawCubeWiresV(center, size, Fade(BLACK, 0.5f));
    }

}

EnemyCheckpoint demo::Enemy::CaptureCheckpoint() const
{
    return {config_, GetPosition(), yaw_, hp_};
}

void demo::Enemy::RestoreCheckpoint(const EnemyCheckpoint& data)
{
    SetConfig(data.config);
    Reset(data.position, data.yaw);
    hp_ = std::clamp(data.hp, 1, config_.maxHp);
}
