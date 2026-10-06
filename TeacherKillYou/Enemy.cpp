#include "Enemy.h"
#include "Player.h"
#include "Stage.h"
#include "ResourceManager.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace {

// 接触面との余白 [m]。丸め誤差による侵入と距離0付近の除算を避ける。
constexpr float ContactEpsilon = 0.001f;
enum class MoveAxis { X, Z };
// raylib 5.5のglTFアニメーションは17ms間隔で読み込まれる。
constexpr float AnimationFrameDuration = 0.017f;


// NaN/Infは既定値へ置換し、有限値は下限へクランプする。
float AtLeast(float value, float minimum, float fallback) {
    return (std::max)(minimum, std::isfinite(value) ? value : fallback);
}

bool IsFinite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

// [-PI, PI]へ正規化し、旋回時の角度差を最短方向で扱う。
float WrapAngle(float angle) {
    return std::isfinite(angle) ? std::remainder(angle, 2.0f * PI) : 0.0f;
}

BoundingBox BodyBounds(Vector3 position, float radius, float height) {
    return { { position.x - radius, position.y, position.z - radius },
             { position.x + radius, position.y + height, position.z + radius } };
}

// 面同士の接触は侵入と見なさないため、等号を含めない。
bool Overlaps(BoundingBox a, BoundingBox b) {
    return a.min.x < b.max.x && a.max.x > b.min.x &&
           a.min.y < b.max.y && a.max.y > b.min.y &&
           a.min.z < b.max.z && a.max.z > b.min.z;
}


// amount [m]を移動可能な符号付き距離へ制限する。target==nullptrならStageだけを判定。
// X/Zを独立して解決することで、片方の軸が塞がれても壁沿いの移動を許可する。
float AllowedTravel(Vector3 position, float amount, MoveAxis axis, float radius,
                    float height, const Stage& stage, const BoundingBox* target) {
    if (amount == 0.0f) return 0.0f;
    const bool xAxis = axis == MoveAxis::X;
    const float sign = amount > 0.0f ? 1.0f : -1.0f;
    const Vector3 direction = xAxis ? Vector3{ sign, 0, 0 } : Vector3{ 0, 0, sign };
    float travel = std::fabs(amount);


    // 3高さ×3横位置をレイでサンプリングする。厳密な体積掃引ではない。
    const float heights[] = { height * 0.1f, height * 0.5f, height * 0.9f };
    const float offsets[] = { -radius, 0.0f, radius };
    for (float sampleHeight : heights) {
        for (float offset : offsets) {
            Vector3 origin = position;
            origin.y += sampleHeight;
            if (xAxis) origin.z += offset;
            else origin.x += offset;
            // 中心から体の前端までの半幅も含める。必要な区間だけをレイキャストする。
            const RayCollision hit = stage.Raycast({ origin, direction }, travel + radius + ContactEpsilon);
            if (hit.hit && std::isfinite(hit.distance) && hit.distance >= 0.0f) {
                const float clearance = (std::max)(0.0f, hit.distance - radius - ContactEpsilon);
                travel = (std::min)(travel, clearance);
            }
        }
    }


    // Yと移動軸に直交する軸が重なる場合だけ、対象への接近距離を制限する。
    const BoundingBox body = BodyBounds(position, radius, height);
    if (!target) return sign * travel;
    const bool overlapsHeight = body.min.y < target->max.y && body.max.y > target->min.y;
    if (!overlapsHeight) return sign * travel;

    const bool overlapsSide = xAxis ?
        body.min.z < target->max.z && body.max.z > target->min.z :
        body.min.x < target->max.x && body.max.x > target->min.x;
    if (!overlapsSide) return sign * travel;

    const float bodyMin = xAxis ? body.min.x : body.min.z;
    const float bodyMax = xAxis ? body.max.x : body.max.z;
    const float targetMin = xAxis ? target->min.x : target->min.z;
    const float targetMax = xAxis ? target->max.x : target->max.z;
    // gap<0は対象が背後または既に侵入中。侵入の解消はSeparateFromTargetで扱う。
    const float gap = sign > 0.0f ? targetMin - bodyMax : bodyMin - targetMax;
    if (gap >= 0.0f) travel = (std::min)(travel, (std::max)(0.0f, gap - ContactEpsilon));
    return sign * travel;
}

}

namespace demo {

TargetInfo TargetInfo::FromPlayer(const Player& player) {
    TargetInfo target;
    target.position = player.GetPosition();
    // Playerのprivateな衝突寸法と対応するため、Player側の寸法変更時はここも更新する。
    target.bodyBounds = BodyBounds(target.position, 0.4f, 1.8f);
    target.alive = true;
    target.valid = IsFinite(target.position);
    return target;
}

// SetConfigを先に適用し、補正済みのmaxHpでResetする。処理順を維持すること。
Enemy::Enemy(EnemyId id, const EnemyConfig& config, Vector3 position, float yaw) : id_(id) {
    SetConfig(config);
    Reset(position, yaw);
}

void Enemy::SetConfig(const EnemyConfig& config) {
    const EnemyConfig defaults;
    config_ = config;
    config_.maxHp = (std::max)(1, config_.maxHp);
    config_.moveSpeed = AtLeast(config_.moveSpeed, 0.0f, defaults.moveSpeed);
    config_.turnSpeed = AtLeast(config_.turnSpeed, 0.0f, defaults.turnSpeed);
    config_.bodyRadius = AtLeast(config_.bodyRadius, 0.01f, defaults.bodyRadius);
    config_.bodyHeight = AtLeast(config_.bodyHeight, 0.01f, defaults.bodyHeight);
    config_.detectionRange = AtLeast(config_.detectionRange, 0.0f, defaults.detectionRange);
    // 状態切り替えの振動を抑えるため、開始・停止側の閾値に0.01mの余白を設ける。
    config_.loseRange = AtLeast(config_.loseRange, config_.detectionRange + 0.01f, defaults.loseRange);
    config_.stopDistance = AtLeast(config_.stopDistance, 0.0f, defaults.stopDistance);
    config_.resumeDistance = AtLeast(config_.resumeDistance, config_.stopDistance + 0.01f, defaults.resumeDistance);
    config_.corpseDuration = AtLeast(config_.corpseDuration, 0.0f, defaults.corpseDuration);
    hp_ = (std::min)(hp_, config_.maxHp);
}

void Enemy::Reset(Vector3 position, float yaw) {
    position_ = IsFinite(position) ? position : Vector3{};
    yaw_ = WrapAngle(yaw);
    verticalVelocity_ = 0.0f;
    hp_ = config_.maxHp;
    state_ = EnemyState::Idle;
    shouldMove_ = true;
    corpseTimer_ = 0.0f;
    animationTime_ = 0.0f;
}

void Enemy::Update(float dt, const TargetInfo& target, const Stage& stage) {
    if (!std::isfinite(dt) || dt <= 0.0f) return;
    if (!IsAlive()) {
        corpseTimer_ = (std::min)(config_.corpseDuration, corpseTimer_ + dt);
        return;
    }


    // 生存中のシミュレーション時間だけ上限を設ける。死亡タイマーは実際のdtを使う。
    // 衝突処理をdtに応じて反復しないことで、遅延時の負荷増幅を防ぐ。
    dt = (std::min)(dt, 0.25f);
    Vector3 displacement = {};
    const BoundingBox* targetBounds = nullptr;
    if (target.valid && target.alive && IsFinite(target.position)) {
        displacement = ChaseTarget(dt, target.position);
        targetBounds = &target.bodyBounds;
    } else {
        state_ = EnemyState::Idle;
        shouldMove_ = true;
    }
    // AIは移動要求を算出するだけ。確定位置は衝突解決と接地処理で更新する。
    const Vector3 previousPosition = position_;
    Move(displacement, targetBounds, stage);
    UpdateGround(dt, stage);
    // 衝突解決後に水平移動できたときだけ再生する。壁で止まった場合は歩かせない。
    const bool moved = position_.x != previousPosition.x || position_.z != previousPosition.z;
    const ModelAnimation animation = RM().GetModelAnimation(ResourceKeys::Model_Enemy);
    if (IsMoving() && moved && animation.frameCount > 0) {
        animationTime_ = std::fmod(animationTime_ + dt, animation.frameCount * AnimationFrameDuration);
    } else {
        animationTime_ = 0.0f;
    }
}

Vector3 Enemy::ChaseTarget(float dt, Vector3 targetPosition) {
    Vector3 delta = Vector3Subtract(targetPosition, position_);
    // 追跡距離はXZ平面で評価し、目線や足場の高さの差を含めない。
    delta.y = 0.0f;
    const float distance = Vector3Length(delta);


    // 検知距離と喪失距離の間は直前の状態を保持する（ヒステリシス）。
    if (state_ == EnemyState::Idle && distance <= config_.detectionRange) {
        state_ = EnemyState::Chase;
        shouldMove_ = true;
    } else if (state_ == EnemyState::Chase && distance > config_.loseRange) {
        state_ = EnemyState::Idle;
        shouldMove_ = true;
    }
    if (state_ != EnemyState::Chase) return {};

    if (distance > ContactEpsilon) {
        // 前方が-Zなのでatan2(x, -z)。旋回量はturnSpeed*dt以内に制限する。
        const float desiredYaw = std::atan2(delta.x, -delta.z);
        const float maxTurn = config_.turnSpeed * dt;
        const float turn = Clamp(WrapAngle(desiredYaw - yaw_), -maxTurn, maxTurn);
        yaw_ = WrapAngle(yaw_ + turn);
    }
    // 停止後はresumeDistanceまで離れない限り再開しない。壁による停止とは別の判定。
    if (distance <= config_.stopDistance) shouldMove_ = false;
    else if (distance >= config_.resumeDistance) shouldMove_ = true;
    if (!shouldMove_ || distance <= ContactEpsilon) return {};

    // 停止距離を越えて接近しないよう、残距離を移動量の上限にする。
    const float travel = (std::min)(config_.moveSpeed * dt, distance - config_.stopDistance);
    return Vector3Scale(delta, travel / distance);
}

void Enemy::Move(Vector3 displacement, const BoundingBox* bounds, const Stage& stage) {
    if (bounds && Overlaps(GetBodyBounds(), *bounds)) SeparateFromTarget(*bounds, stage);
    // X更新後の位置を使ってZを解決する。軸の処理順は固定する。
    position_.x += AllowedTravel(position_, displacement.x, MoveAxis::X,
                                 config_.bodyRadius, config_.bodyHeight, stage, bounds);
    position_.z += AllowedTravel(position_, displacement.z, MoveAxis::Z,
                                 config_.bodyRadius, config_.bodyHeight, stage, bounds);
}

// 対象が先に侵入した場合、4方向の分離候補を距離の短い順に試す。
// 完全に分離できる候補だけを採用し、全候補が壁に阻まれる場合は変更しない。
void Enemy::SeparateFromTarget(const BoundingBox& bounds, const Stage& stage) {

    struct Separation { float amount; MoveAxis axis; };
    const float padding = config_.bodyRadius + ContactEpsilon;
    Separation choices[] = {
        { bounds.min.x - padding - position_.x, MoveAxis::X },
        { bounds.max.x + padding - position_.x, MoveAxis::X },
        { bounds.min.z - padding - position_.z, MoveAxis::Z },
        { bounds.max.z + padding - position_.z, MoveAxis::Z }
    };
    std::sort(choices, choices + 4, [](const Separation& a, const Separation& b) {
        return std::fabs(a.amount) < std::fabs(b.amount);
    });
    for (const Separation& choice : choices) {
        const float allowed = AllowedTravel(position_, choice.amount, choice.axis,
            config_.bodyRadius, config_.bodyHeight, stage, nullptr);
        if (std::fabs(allowed - choice.amount) > ContactEpsilon * 0.5f) continue;
        if (choice.axis == MoveAxis::X) position_.x += allowed;
        else position_.z += allowed;
        return;
    }
}

void Enemy::UpdateGround(float dt, const Stage& stage) {
    // 速度→位置の順で重力を積分する。レイ長は今フレームの落下距離まで含める。
    constexpr float groundProbeHeight = 0.25f;
    verticalVelocity_ -= 9.81f * dt;
    const float nextY = position_.y + verticalVelocity_ * dt;
    const Ray downRay = { Vector3Add(position_, { 0, groundProbeHeight, 0 }), { 0, -1, 0 } };
    const float probeDistance = groundProbeHeight + (std::max)(0.0f, position_.y - nextY) + ContactEpsilon;
    const RayCollision hit = stage.Raycast(downRay, probeDistance);
    const float groundY = downRay.position.y - hit.distance;
    // 上向き法線を持つ面だけに接地し、壁面や下向きの面を床として扱わない。
    if (hit.hit && hit.normal.y > 0.5f && std::isfinite(groundY) && nextY <= groundY + ContactEpsilon) {
        position_.y = groundY;
        verticalVelocity_ = 0.0f;
    } else {
        position_.y = nextY;
    }
}

EnemyDamageResult Enemy::TakeDamage(int amount) {
    if (amount <= 0 || !IsAlive()) return { false, hp_, false };
    // 減算前に比較し、大きなダメージでもHPを負値にしない。
    hp_ = amount >= hp_ ? 0 : hp_ - amount;
    // Deadへの遷移はこの呼び出しで一度だけ報告する。以降の被弾は冒頭で拒否する。
    const bool died = hp_ == 0;
    if (died) {
        state_ = EnemyState::Dead;
        shouldMove_ = false;
        corpseTimer_ = 0.0f;
    }
    return { true, hp_, died };
}

BoundingBox Enemy::GetBodyBounds() const {
    return BodyBounds(position_, config_.bodyRadius, config_.bodyHeight);
}

bool Enemy::IsRemovalReady() const {
    return !IsAlive() && corpseTimer_ >= config_.corpseDuration;
}

void Enemy::Draw() const {
    Model model = RM().GetModel(ResourceKeys::Model_Enemy);
    if (model.meshCount > 0) {
        const ModelAnimation animation = RM().GetModelAnimation(ResourceKeys::Model_Enemy);
        if (animation.frameCount > 0) {
            const int frame = static_cast<int>(animationTime_ / AnimationFrameDuration) % animation.frameCount;
            // メッシュを共有するため、各敵の描画直前にその敵のポーズを適用する。
            UpdateModelAnimation(model, animation, frame);
        }
        const BoundingBox bounds = RM().GetModelBounds(ResourceKeys::Model_Enemy);
        const float height = bounds.max.y - bounds.min.y;
        const float scale = height > ContactEpsilon ? config_.bodyHeight / height : 1.0f;
        // モデルの原点を足元中心へ補正する。衝突用AABBの位置・寸法は変更しない。
        model.transform = MatrixTranslate(-(bounds.min.x + bounds.max.x) * 0.5f,
                                          -bounds.min.y, -(bounds.min.z + bounds.max.z) * 0.5f);
        // モデルの正面は+Z。Enemyのyaw=0（-Z）と旋回方向へ合わせる。
        const Color tint = IsAlive() ? WHITE : Color{ 110, 110, 110, 255 };
        DrawModelEx(model, position_, { 0, 1, 0 }, 180.0f - yaw_ * RAD2DEG, { scale, scale, scale }, tint);
        return;

    }
    // モデルが読み込めない場合は、位置を確認できるよう箱を表示する。
    // 描画用の中心は足元から半高分ずらす。論理座標と衝突AABBは回転させない。
    Color color = { 57, 187, 173, 255 };
    if (!IsAlive()) color = { 91, 99, 110, 255 };
    else if (state_ == EnemyState::Chase) color = { 242, 146, 65, 255 };

    const float radius = config_.bodyRadius;
    const Vector3 center = Vector3Add(position_, { 0, config_.bodyHeight * 0.5f, 0 });
    const Vector3 size = { radius * 2, config_.bodyHeight, radius * 2 };
    DrawCubeV(center, size, color);
    DrawCubeWiresV(center, size, Fade(BLACK, 0.5f));
}

}
