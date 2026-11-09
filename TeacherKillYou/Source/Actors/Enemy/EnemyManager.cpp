#include "EnemyManager.h"
#include "Player.h"
#include "Inventory.h"
#include <algorithm>

namespace demo {

EnemyId EnemyManager::Spawn(Vector3 position, const EnemyConfig& config, float yaw) {
    // 配列の削除・前詰めとは独立してIDを発行する。ClearでもnextId_は維持する。
    const EnemyId id = nextId_++;
    enemies_.emplace_back(id, config, position, yaw);
    return id;
}

static bool HasLostItemBeenCollected(const Inventory* inventory) {
    if (!inventory) return false;
    for (const auto& item : inventory->GetWorldItems()) {
        if (item.GetType() == ItemType::LostItem && item.IsCollected()) {
            return true;
        }
    }
    return false;
}


void EnemyManager::AddSpawnPoint(const Vector3& position, const EnemyConfig& config, EnemySpawnCondition condition, float yaw) {
    EnemySpawnData sp;
    sp.SetPosition(position);
    sp.config = config;
    sp.condition = condition;
    sp.yaw = yaw;
	spawnPoints_.push_back(sp);
}

void EnemyManager::Clear() {
    enemies_.clear();
}

void EnemyManager::RespawnFromPlacements() {
    Clear();
    for (auto& placement : spawnPoints_) {
        placement.spawned = placement.condition == EnemySpawnCondition::Normal;
        placement.spawnedEnemyId = placement.spawned
            ? Spawn(placement.GetPosition(), placement.config, placement.yaw)
            : 0;
    }
}

void EnemyManager::Update(float dt, const TargetInfo& target, const Stage& stage, const Inventory* inventory) {
    // 条件付きスポーンのチェック
    for (auto& sp : spawnPoints_) {
        if (sp.spawned) continue;

        bool shouldSpawn = false;
        if (sp.condition == EnemySpawnCondition::Normal) {
            shouldSpawn = true;
        }
        else if (sp.condition == EnemySpawnCondition::LostItemCollected) {
            if (HasLostItemBeenCollected(inventory)) {
                shouldSpawn = true;
            }
        }

        if (shouldSpawn) {
            EnemyId id = Spawn(sp.GetPosition(), sp.config, sp.yaw);
            sp.spawned = true;
            sp.spawnedEnemyId = id;
        }
    }

    for (auto& enemy : enemies_) enemy.Update(dt, target, stage);
}

void EnemyManager::Update(float dt, const Player& player, const Stage& stage, const Inventory* inventory) {
    Update(dt, TargetInfo::FromPlayer(player), stage, inventory);
}

void EnemyManager::Update(float dt, Player& player, const Stage& stage, const BoundingBox* blocker, const Inventory* inventory) {

    for (auto& sp : spawnPoints_) {
        if (sp.spawned) continue;

        bool shouldSpawn = false;
        if (sp.condition == EnemySpawnCondition::Normal) {
            shouldSpawn = true;
        }
        else if (sp.condition == EnemySpawnCondition::LostItemCollected) {
            if (HasLostItemBeenCollected(inventory)) {
                shouldSpawn = true;
            }
        }

        if (shouldSpawn) {
            EnemyId id = Spawn(sp.GetPosition(), sp.config, sp.yaw);
            sp.spawned = true;
            sp.spawnedEnemyId = id;
        }
    }

    for (auto& enemy : enemies_) {
        enemy.Update(dt, TargetInfo::FromPlayer(player), stage, blocker);
        player.TakeDamage(enemy.ConsumeAttackDamage());
    }
}

Enemy* EnemyManager::Find(EnemyId id) {
    for (auto& enemy : enemies_) {
        if (enemy.GetId() == id) return &enemy;
    }
    return nullptr;
}

const Enemy* EnemyManager::Find(EnemyId id) const {
    for (const auto& enemy : enemies_) {
        if (enemy.GetId() == id) return &enemy;
    }
    return nullptr;
}

EnemyDamageResult EnemyManager::ApplyDamage(EnemyId id, int amount) {
    Enemy* enemy = Find(id);
    if (!enemy) return {};
    return enemy->TakeDamage(amount);
}

void EnemyManager::RemoveExpired() {
    // remove_ifは保持する要素を前詰めするだけ。eraseで不要な末尾を実際に削除する。
    const auto expiredBegin = std::remove_if(enemies_.begin(), enemies_.end(),
        [](const Enemy& enemy) { return enemy.IsRemovalReady(); });
    enemies_.erase(expiredBegin, enemies_.end());
}

void EnemyManager::Draw() const {

    for (const auto& enemy : enemies_) enemy.Draw();
}

int EnemyManager::AliveCount() const {
    return static_cast<int>(std::count_if(enemies_.begin(), enemies_.end(),
        [](const Enemy& enemy) { return enemy.IsAlive(); }));
}

void EnemyManager::DespawnBySecurityTeleport()
{
    for (auto& sp : spawnPoints_) {
        if (sp.despawnCondition == EnemyDespawnCondition::OnSecurityTeleport && sp.spawned) {
            Enemy* enemy = Find(sp.spawnedEnemyId);
            if (enemy && enemy->IsAlive()) {
                enemy->TakeDamage(9999); // 強制的にダメージを与えて撃破・デスポーンさせる
            }
        }
    }
}

}
