#include "EnemyMovement.h"
#include "Enemy.h" // EnemyConfig‚Ì‚½‚ß
#include "Stage.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float ContactEpsilon = 0.001f;
    enum class MoveAxis { X, Z };

    bool IsFinite(Vector3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    BoundingBox BodyBounds(Vector3 position, float radius, float height) {
        return { { position.x - radius, position.y, position.z - radius },
                 { position.x + radius, position.y + height, position.z + radius } };
    }

    bool Overlaps(BoundingBox a, BoundingBox b) {
        return a.min.x < b.max.x && a.max.x > b.min.x &&
            a.min.y < b.max.y && a.max.y > b.min.y &&
            a.min.z < b.max.z && a.max.z > b.min.z;
    }
}

namespace demo {

    EnemyMovement::EnemyMovement() = default;

    void EnemyMovement::Reset(Vector3 position, float verticalVelocity) {
        position_ = IsFinite(position) ? position : Vector3{};
        verticalVelocity_ = verticalVelocity;
    }

    BoundingBox EnemyMovement::GetBodyBounds(const EnemyConfig& config) const {
        return BodyBounds(position_, config.bodyRadius, config.bodyHeight);
    }

    void EnemyMovement::Update(float dt, Vector3 displacement, const BoundingBox* targetBounds, const Stage& stage, const EnemyConfig& config) {
        Move(displacement, targetBounds, stage, config);
        UpdateGround(dt, stage);
    }

    void EnemyMovement::Move(Vector3 displacement, const BoundingBox* bounds, const Stage& stage, const EnemyConfig& config) {
        if (bounds && Overlaps(GetBodyBounds(config), *bounds)) {
            SeparateFromTarget(*bounds, stage, config);
        }
        position_.x += AllowedTravel(position_, displacement.x, MoveAxis::X, config.bodyRadius, config.bodyHeight, stage, bounds);
        position_.z += AllowedTravel(position_, displacement.z, MoveAxis::Z, config.bodyRadius, config.bodyHeight, stage, bounds);
    }

    void EnemyMovement::SeparateFromTarget(const BoundingBox& bounds, const Stage& stage, const EnemyConfig& config) {
        struct Separation { float amount; MoveAxis axis; };
        const float padding = config.bodyRadius + ContactEpsilon;
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
            const float allowed = AllowedTravel(position_, choice.amount, choice.axis, config.bodyRadius, config.bodyHeight, stage, nullptr);
            if (std::fabs(allowed - choice.amount) > ContactEpsilon * 0.5f) continue;
            if (choice.axis == MoveAxis::X) position_.x += allowed;
            else position_.z += allowed;
            return;
        }
    }

    void EnemyMovement::UpdateGround(float dt, const Stage& stage) {
        constexpr float groundProbeHeight = 0.25f;
        verticalVelocity_ -= 9.81f * dt;
        const float nextY = position_.y + verticalVelocity_ * dt;
        const Ray downRay = { Vector3Add(position_, { 0, groundProbeHeight, 0 }), { 0, -1, 0 } };
        const float probeDistance = groundProbeHeight + (std::max)(0.0f, position_.y - nextY) + ContactEpsilon;
        const RayCollision hit = stage.RaycastGround(downRay, probeDistance);
        const float groundY = downRay.position.y - hit.distance;
        if (hit.hit && std::isfinite(groundY) && nextY <= groundY + ContactEpsilon) {
            position_.y = groundY;
            verticalVelocity_ = 0.0f;
        }
        else {
            position_.y = nextY;
        }
    }

    float EnemyMovement::AllowedTravel(Vector3 position, float amount, MoveAxis axis, float radius, float height, const Stage& stage, const BoundingBox* target) const {
        if (amount == 0.0f) return 0.0f;
        const bool xAxis = axis == MoveAxis::X;
        const float sign = amount > 0.0f ? 1.0f : -1.0f;
        const Vector3 direction = xAxis ? Vector3{ sign, 0, 0 } : Vector3{ 0, 0, sign };
        float travel = std::fabs(amount);

        const float heights[] = { height * 0.1f, height * 0.5f, height * 0.9f };
        const float offsets[] = { -radius, 0.0f, radius };
        for (float sampleHeight : heights) {
            for (float offset : offsets) {
                Vector3 origin = position;
                origin.y += sampleHeight;
                if (xAxis) origin.z += offset;
                else origin.x += offset;
                const RayCollision hit = stage.Raycast({ origin, direction }, travel + radius + ContactEpsilon);
                if (hit.hit && std::isfinite(hit.distance) && hit.distance >= 0.0f) {
                    const float clearance = (std::max)(0.0f, hit.distance - radius - ContactEpsilon);
                    travel = (std::min)(travel, clearance);
                }
            }
        }

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
        const float gap = sign > 0.0f ? targetMin - bodyMax : bodyMin - targetMax;
        if (gap >= 0.0f) travel = (std::min)(travel, (std::max)(0.0f, gap - ContactEpsilon));
        return sign * travel;
    }

}