#pragma once
#include "raylib.h"

class Stage;

namespace demo {

    struct EnemyConfig;

    class EnemyMovement {
    public:
        EnemyMovement();

        void Reset(Vector3 position, float verticalVelocity = 0.0f);

        // 毎フレームの移動・衝突・接地処理をまとめて実行
        void Update(float dt, Vector3 displacement, const BoundingBox* targetBounds, const Stage& stage, const EnemyConfig& config, const BoundingBox* blocker = nullptr);

        Vector3 GetPosition() const { return position_; }
        void SetPosition(Vector3 pos) { position_ = pos; }

        float GetVerticalVelocity() const { return verticalVelocity_; }
        void SetVerticalVelocity(float vel) { verticalVelocity_ = vel; }

        BoundingBox GetBodyBounds(const EnemyConfig& config) const;

    private:
        enum class MoveAxis { X, Z };

        void Move(Vector3 displacement, const BoundingBox* bounds, const Stage& stage, const EnemyConfig& config, const BoundingBox* blocker);
        void SeparateFromTarget(const BoundingBox& bounds, const Stage& stage, const EnemyConfig& config, const BoundingBox* blocker);
        void UpdateGround(float dt, const Stage& stage);
        float AllowedTravel(Vector3 position, float amount, MoveAxis axis, float radius, float height, const Stage& stage, const BoundingBox* target, const BoundingBox* blocker) const;

        Vector3 position_ = {};
        float verticalVelocity_ = 0.0f;
    };

}