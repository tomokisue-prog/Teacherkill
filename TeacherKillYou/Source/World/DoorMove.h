#pragma once
#include "GameObject.h"
#include "raylib.h"
#include "Player.h"

struct DoorCheckpoint;
class Player;
class Inventory; // 前方宣言

// ドアが閉ざされる（開かなくなる）条件の種類
enum class DoorLockType
{
    None,               // 制限なし（普通に開け閉め可能）
    LostItemCollected,  // 忘れ物（LostItem）を取得したとき
    ReachedArea,        // 指定の場所（エリア）に到達したとき
    EnemyDefeated,      // 指定の敵を倒したとき
    CustomEventFlag,    // 任意のフラグが立ったとき
};

// ドアが開くための条件の種類（アンロック条件）
enum class DoorOpenConditionType
{
    None,                    // 制限なし（デフォルトで開閉可能）
    TestMKey,                // テスト用：近づいてMキーを押したとき
    SecurityTeleportEntered, // 警備室テレポートに入ったとき
    SecurityPCActivated,     // 警備室PCが起動されたとき
    EmergencyStairsKey,      // 非常階段のキー所持で開く
    Crowbar,
};

class DoorMove : public GameObject
{
public:
    DoorMove() : GameObject(GameObjectType::Stage, "Door") {}
    ~DoorMove() override = default;

    void Init();
    void Reset();

    // Inventory を受け取ってロック状態を判定できるようにする
    void Update(float deltaTime, const Player& player, const Inventory* inventory = nullptr);
    void Draw() const;

    BoundingBox GetBoundingBox() const;

    // 開閉制御
    void Open();
    void Close();
    void Toggle();
    DoorCheckpoint CaptureCheckpoint() const;
    void RestoreCheckpoint(const DoorCheckpoint& data);

    // インスペクター編集用パラメータ
    float GetSlideSpeed() const { return slideSpeed_; }
    void SetSlideSpeed(float speed) { slideSpeed_ = speed; }

    float GetOpenDistance() const { return openDistance_; }
    void SetOpenDistance(float distance) { openDistance_ = distance; }

    float GetInteractRadius() const { return interactRadius_; }
    void SetInteractRadius(float radius) { interactRadius_ = radius; }

    // ロック条件の取得・設定
    DoorLockType GetLockType() const { return lockType_; }
    void SetLockType(DoorLockType type) { lockType_ = type; }

    // 開ける条件の取得・設定
    DoorOpenConditionType GetOpenConditionType() const { return openConditionType_; }
    void SetOpenConditionType(DoorOpenConditionType type) { openConditionType_ = type; }

    bool HasClosedByCondition(const Inventory* inventory) const;

    bool IsOpenConditionSatisfied() const;

    bool IsOpen() const { return isOpen_; }
    bool IsFullyOpen() const { return slideProgress_ >= 0.95f; }
    bool CanInteract(const Player& player, const Inventory* inventory = nullptr) const;

    bool IsLocked(const Inventory* inventory) const;

    void SetOpenConditionSatisfied(bool satisfied) { openConditionSatisfied_ = satisfied; }

    bool IsPlayerInInteractionRange(const Player& player) const;
private:
    Vector3 CurrentPosition() const;

    Player player;

    bool isOpen_{ false };
    float slideProgress_{ 0.0f }; // 0.0f (全閉) 〜 1.0f (全開)

    float slideSpeed_{ 2.0f };       // 開閉速度
    float openDistance_{ 2.0f };    // スライド移動距離 (X軸方向)
    float interactRadius_{ 1.5f };  // インタラクション可能な距離
    Vector3 closedPosition_{ 0.0f, 0.0f, 0.0f };

    DoorLockType lockType_{ DoorLockType::None }; // 何をしたら開かなくなるかの種類

    // 開ける条件の取得・設定
    DoorOpenConditionType openConditionType_{ DoorOpenConditionType::None };
    bool openConditionSatisfied_{ false };
};