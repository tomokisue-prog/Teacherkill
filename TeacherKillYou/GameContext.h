#pragma once

#include "CameraController.h"
#include "Player.h"
#include "Stage.h"
#include "EnemyManager.h"
#include "HorrorLighting.h"
#include "BloodEffect.h"
#include "DoorMove.h"
#include "TeleportPoint.h"
#include "GameFade.h"
#include "Inventory.h"
#include "GoalPoint.h"
#include "MessageWindow.h"

#include <vector>

class GameContext
{
public:
    GameContext() = default;
    ~GameContext() = default;

    void Init();
    void Reset();
    void Update(float deltaTime);
    void Draw() const;
    void End();

    CameraController& GetCameraController() { return cameraController_; }
    const CameraController& GetCameraController() const { return cameraController_; }

    Player& GetPlayer() { return player_; }
    const Player& GetPlayer() const { return player_; }

    Stage& GetStage() { return stage_; }
    const Stage& GetStage() const { return stage_; }

    demo::EnemyManager& GetEnemyManager() { return enemyManager_; }
    const demo::EnemyManager& GetEnemyManager() const { return enemyManager_; }

    DoorMove& GetDoor() { return door_; }
    const DoorMove& GetDoor() const { return door_; }

    GoalPoint& GetGoalPoint() { return goalPoint_; }
    const GoalPoint& GetGoalPoint() const { return goalPoint_; }

    std::vector<TeleportPoint>& GetTeleportPoints() { return teleportPoints_; }
    const std::vector<TeleportPoint>& GetTeleportPoints() const { return teleportPoints_; }

    // IDを指定して1行でセリフを呼び出す関数
    void ShowDialogue(const std::string& dialogueId);

    // メッセージウィンドウが開いている最中かどうか
    bool IsMessageActive() const { return messageWindow_.IsOpen(); }

    Inventory& GetInventory() { return inventory_; }

private:
    void PlaceCharacters();
    void ProcessPlayerShot();
    void UpdateShotEffects(float deltaTime);
    void DrawShotEffects() const;
    void DrawBulletMarks() const;

    mutable HorrorLighting lighting_;
    BloodEffect bloodEffect_;
    CameraController cameraController_;
    Player player_;
    Stage stage_;
    demo::EnemyManager enemyManager_;
    DoorMove door_;
    GameFade gameFade_;
    Inventory inventory_;
    GoalPoint goalPoint_;
	MessageWindow messageWindow_;

    Vector3 paladinPosition_{ 0.0f, 0.0f, 3.0f };
    std::vector<TeleportPoint> teleportPoints_;

    float hitMarkerTimer_{ 0.0f };
    bool hitMarkerKill_{ false };

    Texture2D muzzleFlashTexture_{};
    float muzzleFlashTimer_{ 0.0f };
    int muzzleFlashFrame_{ 0 };

    Vector3 tracerStart_{};
    Vector3 tracerEnd_{};
    float tracerAge_{ 0.0f };
    float tracerDuration_{ 0.05f };
    bool tracerActive_{ false };

    // Small, temporary bullet-hole marks on stage walls.
    struct BulletMark
    {
        Vector3 position{};
        Vector3 normal{};
        float remainingTime = 0.0f;
    };

    std::vector<BulletMark> bulletMarks_;
    static constexpr float bulletMarkLifetime_ = 2.0f;
    static constexpr int maxBulletMarks_ = 40;
};
