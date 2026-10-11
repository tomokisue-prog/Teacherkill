#include "EnemyManager.h"
#include "Player.h"

#include <algorithm>

namespace demo {

EnemyId EnemyManager::Spawn(Vector3 position, const EnemyConfig& config, float yaw) {
    // 配列の削除・前詰めとは独立してIDを発行する。ClearでもnextId_は維持する。
    const EnemyId id = nextId_++;
    enemies_.emplace_back(id, config, position, yaw);
    return id;
}

void EnemyManager::Clear() {
    enemies_.clear();
}

void EnemyManager::Update(float dt, const TargetInfo& target, const Stage& stage) {
    for (auto& enemy : enemies_) enemy.Update(dt, target, stage);
}

void EnemyManager::Update(float dt, const Player& player, const Stage& stage) {
    Update(dt, TargetInfo::FromPlayer(player), stage);
}

void EnemyManager::Update(float dt, Player& player, const Stage& stage, const BoundingBox* blocker) {
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

} // namespace demo
