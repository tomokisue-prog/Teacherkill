#pragma once
#include "raylib.h"
#include "WorldScale.h"
#include <cstddef>

namespace demo {

    enum class EnemyType { Monster, FemaleZombie, MonsterMale2 };

    struct EnemyConfig {
		int maxHp = 100; // 最大HP
		float moveSpeed = 3.0f; // 移動速度
		float turnSpeed = PI; // 回転速度
		float bodyRadius = WorldScale::Monster.radius; // 体の半径
		float bodyHeight = WorldScale::Monster.height; // 体の高さ
		float detectionRange = 10.0f; // プレイヤーを検知する距離
		float loseRange = 15.0f; // プレイヤーを見失う距離
		float stopDistance = 1.2f; // プレイヤーに近づきすぎない距離
		float resumeDistance = 1.5f;// プレイヤーに近づきすぎない距離の解除距離
		float corpseDuration = 2.0f; // 死亡後の消滅までの時間
		int attackDamage = 15; // 攻撃力
		float attackRange = 1.6f; // 攻撃判定の距離
		float attackHitFraction = 0.4f; // 攻撃判定のヒットタイミング
        float attackRecovery = 0.65f; // 攻撃回復時間
		EnemyType type = EnemyType::Monster; // 敵の種類
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