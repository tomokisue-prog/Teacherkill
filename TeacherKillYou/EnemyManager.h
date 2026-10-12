#pragma once
#include "Enemy.h"

#include <vector>

namespace demo {

// Enemyを値で所有する。更新中は要素を削除せず、RemoveExpiredでまとめて削除する。
class EnemyManager {
public:
    // 生成した敵のIDを返す。IDとvectorの配列添字は一致するとは限らない。
    EnemyId Spawn(Vector3 position, const EnemyConfig& config, float yaw = 0.0f);
    // 敵一覧を破棄する。ID発行カウンターは維持する。
    void Clear();
    void Update(float dt, const TargetInfo& target, const Stage& stage);
    void Update(float dt, const Player& player, const Stage& stage);
    // 生存状態を毎回取り直し、各攻撃イベントを1回だけPlayerへ適用する。
    void Update(float dt, Player& player, const Stage& stage, const BoundingBox* blocker = nullptr);
    // 未検出のIDには既定結果（applied=false、hp=0、becameDead=false）を返す。
    EnemyDamageResult ApplyDamage(EnemyId id, int amount);
    // 更新・敵一覧の走査が終わった後に呼ぶ。削除待ち時間を過ぎた死体を除去する。
    void RemoveExpired();
    // 呼び出し側でBeginMode3D～EndMode3Dを開始・終了する。
    void Draw() const;

    // 未検出時はnullptr。Spawnによる再配置や削除でポインターが無効になり得る。
    // 長期保存にはEnemyIdを使い、必要な時点で再検索する。
    Enemy* Find(EnemyId id);
    const Enemy* Find(EnemyId id) const;
    // 要素への参照・イテレーターにも、Findのポインターと同じ有効期間の制約がある。
    const std::vector<Enemy>& GetEnemies() const { return enemies_; }
    // 表示中の死体を除いた、生存中の敵の数。
    int AliveCount() const;

private:
    // Clearでは巻き戻さず、シーン再開後の敵へ過去のIDが対応することを防ぐ。
    EnemyId nextId_ = 1;
    std::vector<Enemy> enemies_;
};

} // namespace demo
