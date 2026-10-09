#pragma once
#include "raylib.h"

#include <cstdint>

class Player;
class Stage;

namespace demo {

// 配列添字と独立した識別子。EnemyManagerが生成時に発行する。
using EnemyId = std::uint64_t;

// Enemyへコピーする設定値。現在HPや移動状態などの実行時データは含まない。
// ワールド座標はY-up。距離・寸法はm、速度はm/s、角度はrad。
struct EnemyConfig {
    int maxHp = 100;             // 最大HP。Reset時の現在HPにも使う
    float moveSpeed = 3.0f;      // 水平移動速度 [m/s]
    float turnSpeed = PI;        // 最大旋回速度 [rad/s]
    float bodyRadius = 0.4f;     // 衝突用AABBのX/Z方向の半幅 [m]
    float bodyHeight = 1.8f;     // 足元からのAABB高さ [m]
    float detectionRange = 15.0f; // IdleからChaseへ遷移する水平距離の上限
    float loseRange = 20.0f;      // Chaseを維持できる水平距離の上限
    float stopDistance = 1.2f;    // この距離以下で移動要求を停止
    float resumeDistance = 1.5f;  // 停止後、この距離以上で移動要求を再開
    float corpseDuration = 2.0f;  // Deadへ遷移してから削除可能になるまでの秒数
};

// TakeDamageの呼び出し結果。敵の状態を保持するための構造体ではない。
struct EnemyDamageResult {
    bool applied = false;    // 正のダメージを生存中の敵に適用できたか
    int hp = 0;              // 処理後の現在HPのコピー
    bool becameDead = false; // 今回の呼び出しで生存状態からDeadへ遷移したか
};

enum class EnemyState { Idle, Chase, Dead };

// 更新時点の対象情報。Playerの参照を保持せず、各フレームで作り直す。
struct TargetInfo {
    Vector3 position = {};       // 足元中心のワールド座標
    BoundingBox bodyBounds = {}; // ワールド座標のAABB。対象への侵入防止に使う
    bool alive = false;
    bool valid = false;

    // Playerの位置と固定寸法から生成する。Playerに死亡状態がないためalive=true。
    static TargetInfo FromPlayer(const Player& player);
};

// 敵1体のAI・移動・HPを管理する。所有と削除はEnemyManagerが担当する。
class Enemy {
public:
    // positionは足元中心、yawは水平角。yaw=0の前方は-Z。
    Enemy(EnemyId id, const EnemyConfig& config, Vector3 position, float yaw = 0.0f);
    // IDと設定を維持し、現在HP・位置・状態・死亡タイマーを初期化する。
    void Reset(Vector3 position, float yaw = 0.0f);
    // dtは秒。無効な値・非正値は無視。Deadでは死亡タイマーのみ更新する。
    void Update(float dt, const TargetInfo& target, const Stage& stage);
    // amount<=0またはDeadの場合は適用しない。被弾無敵時間は設けていない。
    EnemyDamageResult TakeDamage(int amount);
    // 入力値を補正してコピーする。現在HPは上限内に収めるだけで回復しない。
    void SetConfig(const EnemyConfig& config);
    // BeginMode3D～EndMode3D内で呼ぶ。モデルはResourceManagerが所有する。
    void Draw() const;

    EnemyId GetId() const { return id_; }
    Vector3 GetPosition() const { return position_; }
    float GetYaw() const { return yaw_; }
    BoundingBox GetBodyBounds() const;
    int GetHp() const { return hp_; }
    EnemyState GetState() const { return state_; }
    bool IsAlive() const { return state_ != EnemyState::Dead; }
    // 削除判断のみ行う。実際の削除は走査終了後にEnemyManagerへ委譲する。
    bool IsRemovalReady() const;
    // 移動要求の有無。壁に阻まれて実移動量が0でもtrueになり得る。
    bool IsMoving() const { return shouldMove_ && state_ == EnemyState::Chase; }

private:
    // 状態と向きを更新し、そのフレームの水平移動要求を返す。
    Vector3 ChaseTarget(float dt, Vector3 targetPosition);
    // targetBounds==nullptrなら対象との衝突を省略し、ステージだけを判定する。
    void Move(Vector3 displacement, const BoundingBox* targetBounds, const Stage& stage);
    void SeparateFromTarget(const BoundingBox& targetBounds, const Stage& stage);
    void UpdateGround(float dt, const Stage& stage);

    EnemyId id_;
    EnemyConfig config_;
    Vector3 position_ = {}; // 足元中心のワールド座標 [m]
    float yaw_ = 0.0f; // 水平角 [rad]。前方は{sin(yaw), 0, -cos(yaw)}
    float verticalVelocity_ = 0.0f; // Y方向速度 [m/s]
    int hp_ = 100; // 現在HP。コンストラクタとResetでconfig_.maxHpから設定する
    EnemyState state_ = EnemyState::Idle;
    bool shouldMove_ = true; // 停止・再開距離の間で保持する移動要求
    float corpseTimer_ = 0.0f; // Deadへ遷移してからの経過時間 [s]
    float animationTime_ = 0.0f; // 再生位置 [s]。停止時は先頭へ戻し、死亡時は維持する
};

} // namespace demo
