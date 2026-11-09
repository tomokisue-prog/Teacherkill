#pragma once

#include "CameraController.h"
#include "Player.h"
#include "Stage.h"
#include "EnemyManager.h"
#include "HorrorLighting.h"
#include "BloodEffect.h"
#include "LowHpBloodOverlay.h"
#include "DoorMove.h"
#include "TeleportPoint.h"
#include "GameFade.h"
#include "Inventory.h"
#include "GoalPoint.h"
#include "MessageWindow.h"
#include "HorrorAudio.h"
#include "CheckpointData.h"
#include "../Events/EventManager.h"
#include "../Events/Tutorial4FFloorEvent.h"
#include "../Events/SecurityPC.h"
#include "../Events/EventDialoguePoint.h"
#include "../Events/SecurityTeleportPoint.h"
#include "../UI/GunTutorialUI.h"
#include <vector>
#ifndef NDEBUG
#include "DebugGameplay.h"
#endif

class GameContext
{
public:
    GameContext() = default;
    ~GameContext() = default;

    void Init();
    void Reset();
    void Update(float deltaTime);
    void Draw() const;
    void SetDisplaySettings(float brightness, float greenStrength);
    void End();
    HorrorLighting& GetLighting() { return lighting_; }
    const HorrorLighting& GetLighting() const { return lighting_; }

    CameraController& GetCameraController() { return cameraController_; }
    const CameraController& GetCameraController() const { return cameraController_; }

    Player& GetPlayer() { return player_; }
    const Player& GetPlayer() const { return player_; }

    Stage& GetStage() { return stage_; }
    const Stage& GetStage() const { return stage_; }

    demo::EnemyManager& GetEnemyManager() { return enemyManager_; }
    const demo::EnemyManager& GetEnemyManager() const { return enemyManager_; }
    std::vector<demo::EnemyPlacement>& GetEnemyPlacements() { return enemyPlacements_; }
    const std::vector<demo::EnemyPlacement>& GetEnemyPlacements() const { return enemyPlacements_; }
    void ApplyEnemyPlacements();

    std::vector<DoorMove>& GetDoors() { return doors_; }
    const std::vector<DoorMove>& GetDoors() const { return doors_; }

    GoalPoint& GetGoalPoint() { return goalPoint_; }
    const GoalPoint& GetGoalPoint() const { return goalPoint_; }

    std::vector<TeleportPoint>& GetTeleportPoints() { return teleportPoints_; }
    const std::vector<TeleportPoint>& GetTeleportPoints() const { return teleportPoints_; }

    std::vector<EventDialoguePoint>& GetEventDialoguePoints() { return eventDialoguePoints_; }
    const std::vector<EventDialoguePoint>& GetEventDialoguePoints() const { return eventDialoguePoints_; }

    std::vector<SecurityTeleportPoint>& GetSecurityTeleportPoints() { return securityTeleportPoints_; }
    const std::vector<SecurityTeleportPoint>& GetSecurityTeleportPoints() const { return securityTeleportPoints_; }

    void ShowDialogue(const std::string& dialogueId);

    bool IsMessageActive() const { return messageWindow_.IsOpen(); }

    Inventory& GetInventory() { return inventory_; }
    const Inventory& GetInventory() const { return inventory_; }
    CheckpointData CaptureCheckpoint() const;
    void RestoreCheckpoint(const CheckpointData& data);
    bool CanSaveCheckpoint() const;
    bool ConsumeCheckpointRequest();
    bool ConsumeGoalRequest() { const bool result = goalRequested_; goalRequested_ = false; return result; }
#ifndef NDEBUG
    DebugGameplay& GetDebugGameplay() { return debugGameplay_; }
    const DebugGameplay& GetDebugGameplay() const { return debugGameplay_; }
    bool DebugTeleportPlayer(Vector3 position, float yaw, float pitch);
#endif

    SecurityPC& GetSecurityPC() { return securityPC_; }
    const SecurityPC& GetSecurityPC() const { return securityPC_; }

private:
    void PlaceCharacters();
    void ProcessPlayerShot();
    void UpdateShotEffects(float deltaTime);
    void DrawShotEffects() const;
    void DrawBulletMarks() const;

#ifndef NDEBUG
    DebugGameplay debugGameplay_;
#endif
    mutable HorrorLighting lighting_;
    BloodEffect bloodEffect_;
    LowHpBloodOverlay lowHpBlood_;
    HorrorAudio horrorAudio_;
    bool checkpointRequested_ = false;
    bool goalRequested_ = false;
    CameraController cameraController_;
    Player player_;
    Stage stage_;
    demo::EnemyManager enemyManager_;
    std::vector<DoorMove> doors_;
    std::vector<demo::EnemyPlacement> enemyPlacements_;
    GameFade gameFade_;
    Inventory inventory_;
    GoalPoint goalPoint_;
    MessageWindow messageWindow_;
    Tutorial4FFloorEvent tutorial4FFloorEvent;
    SecurityPC securityPC_;
    GunTutorialUI gunTutorialUI_;

    Vector3 paladinPosition_{ 0.0f, 0.0f, 3.0f };
    std::vector<TeleportPoint> teleportPoints_;

    std::vector<EventDialoguePoint> eventDialoguePoints_;
    std::vector<SecurityTeleportPoint> securityTeleportPoints_;

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

    struct BulletMark
    {
        Vector3 position{};
        Vector3 normal{};
        float remainingTime = 0.0f;
    };

    std::vector<BulletMark> bulletMarks_;
    static constexpr float bulletMarkLifetime_ = 2.0f;
    static constexpr int maxBulletMarks_ = 40;

    bool hasTriggeredLostItemDialogue_{ false };
    bool securityTeleportUnlocked_{ false };
    bool pendingSecurityDialogue_{ false };

    bool showGunPin_{ false };
    Vector3 gunPinPosition_{ 0.0f, 0.0f, 0.0f };

    bool showPCPin_{ false };
    bool showAuthKeyPin_{ false };
    bool hasTriggeredAuthKeyPickup_{ false };
    bool showSecurityDoorPin_{ false };

    bool showEmergencyStairsDoorPin_{ false };
    bool showEmergencyKeyPin_{ false }; // 非常階段のキーへの誘導ピン
    bool hasTriggeredEmergencyDoorDialogue_{ false }; // 非常階段ドア接触セリフの重複防止フラグ

    bool wasSystemCamera_{ false };
};