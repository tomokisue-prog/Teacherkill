#pragma once
#include "Enemy.h"
#include "GameObject.h"
#include <vector>

class Inventory; // 前方宣言

namespace demo {

    // 敵のスポーン条件の種類
    enum class EnemySpawnCondition {
        None,               // 何も選択していなければスポーンしない
        Normal,             // 「通常」
        LostItemCollected,  // 「忘れ物を拾った後スポーンする」
    };

    // 敵のデスポーン条件の種類
    enum class EnemyDespawnCondition {
        None,                   // デスポーンしない（通常）
        OnSecurityTeleport,     // 警備室テレポートポイントにプレイヤーが入ったらデスポーン
    };

    // スポーン設定データ
    struct EnemySpawnData : public GameObject {
        EnemyConfig config{};
        float yaw = 0.0f;
        EnemySpawnCondition condition = EnemySpawnCondition::None;
        EnemyDespawnCondition despawnCondition = EnemyDespawnCondition::None;
        bool spawned = false;
        EnemyId spawnedEnemyId = 0;

        EnemySpawnData() : GameObject(GameObjectType::Enemy, "EnemySpawner") {}
    };

    class EnemyManager {
    public:
        EnemyId Spawn(Vector3 position, const EnemyConfig& config, float yaw = 0.0f);

        // スポーンポイント（条件付き）の追加
        void AddSpawnPoint(const Vector3& position, const EnemyConfig& config, EnemySpawnCondition condition, float yaw = 0.0f);

        void Clear();

        // Inventory を受け取って条件判定できるように引数を追加
		void Update(float dt, const TargetInfo& target, const Stage& stage, const Inventory* inventory = nullptr);
		void Update(float dt, const Player& player, const Stage& stage, const Inventory* inventory = nullptr);
		void Update(float dt, Player& player, const Stage& stage, const BoundingBox* blocker = nullptr, const Inventory* inventory = nullptr);

        EnemyDamageResult ApplyDamage(EnemyId id, int amount);
        void RemoveExpired();
        void Draw() const;

        Enemy* Find(EnemyId id);
        const Enemy* Find(EnemyId id) const;
        const std::vector<Enemy>& GetEnemies() const { return enemies_; }
        int AliveCount() const;
        // 警備室テレポート使用時に条件付き敵をデスポーンさせる
        void DespawnBySecurityTeleport();
        // インスペクター編集用アクセサ
        std::vector<EnemySpawnData>& GetSpawnPoints() { return spawnPoints_; }
        const std::vector<EnemySpawnData>& GetSpawnPoints() const { return spawnPoints_; }

    private:
        EnemyId nextId_ = 1;
        std::vector<Enemy> enemies_;
        std::vector<EnemySpawnData> spawnPoints_; //スポーン設定のリスト
    };

}