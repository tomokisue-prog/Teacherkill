#pragma once
#include "CameraController.h"
#include "Player.h"
#include "Stage.h"
#include "EnemyManager.h"
#include "HorrorLighting.h"
#include "DoorMove.h"
#include <vector>
#include "TeleportPoint.h"
#include "GameFade.h"

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

    std::vector<TeleportPoint>& GetTeleportPoints() { return teleportPoints_; }
    const std::vector<TeleportPoint>& GetTeleportPoints() const { return teleportPoints_; }

private:
    void PlaceCharacters();
    void ProcessPlayerShot();

    mutable HorrorLighting lighting_;
    CameraController cameraController_;
    Player player_;
    Stage stage_;
    demo::EnemyManager enemyManager_;
	DoorMove door_;
    GameFade gameFade_;

    Vector3 paladinPosition_{ 0.0f, 0.0f, 3.0f };

    std::vector<TeleportPoint> teleportPoints_;

    float hitMarkerTimer_{ 0.0f };
    bool hitMarkerKill_{ false };

    int animFrame_{ 0 };
    int animIndex_{ 0 };

};
