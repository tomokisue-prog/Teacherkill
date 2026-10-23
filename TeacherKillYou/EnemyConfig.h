#pragma once
#include "raylib.h"

namespace demo {

    enum class EnemyType { Monster, FemaleZombie };

    struct EnemyConfig {
        int maxHp = 100;
        float moveSpeed = 3.0f;
        float turnSpeed = PI;
        float bodyRadius = 0.4f;
        float bodyHeight = 1.8f;
        float detectionRange = 10.0f;
        float loseRange = 15.0f;
        float stopDistance = 1.2f;
        float resumeDistance = 1.5f;
        float corpseDuration = 2.0f;
        int attackDamage = 15;
        float attackRange = 1.6f;
        float attackHitFraction = 0.4f;
        float attackRecovery = 0.65f;
        EnemyType type = EnemyType::Monster;
        // クリップの再生倍率。攻撃判定と死亡・被弾の終了時刻もこの倍率に合わせる。
        float animationSpeed = 1.5f;

        // 不正な値が入らないように自動で調整する関数
        void Validate();
    };

}