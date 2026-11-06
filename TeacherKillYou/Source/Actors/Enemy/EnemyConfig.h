#pragma once
#include "raylib.h"
#include "WorldScale.h"
#include <cstddef>

namespace demo {

    // Persisted IDs stay stable: 0 is retired, 1 and 2 remain unchanged.
    enum class EnemyType { FemaleZombie = 1, MonsterMale2 = 2 };

    struct EnemyConfig {
        int maxHp = 100;
        float moveSpeed = 3.0f;
        float turnSpeed = PI;
        float bodyRadius = WorldScale::Zombie.radius;
        float bodyHeight = WorldScale::Zombie.height;
        float detectionRange = 10.0f;
        float loseRange = 15.0f;
        float stopDistance = 1.2f;
        float resumeDistance = 1.5f;
        float corpseDuration = 2.0f;
        int attackDamage = 15;
        float attackRange = 1.6f;
        float attackHitFraction = 0.4f;
        float attackRecovery = 0.65f;
        EnemyType type = EnemyType::FemaleZombie;
        // クリップの再生倍率。攻撃判定と死亡・被弾の終了時刻もこの倍率に合わせる。
        float animationSpeed = 1.5f; // アニメーションの再生倍率

        // 不正な値が入らないように自動で調整する関数
        void Validate();
        static EnemyConfig ForType(EnemyType type);
    };

    struct EnemyPlacement {
        Vector3 position{};
        float yaw = 0.0f;
        EnemyConfig config;
    };

    inline constexpr std::size_t MaxEnemyPlacements = 512;
}