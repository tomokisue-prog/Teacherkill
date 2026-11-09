#pragma once

#include "CameraController.h"
#include "GizmoDrawer.h"
#include "DoorMove.h"
#include "EnemyManager.h"
#include "Item.h"
#include <optional>

enum class SelectedObjectType
{
    None,
    Player,
    EnemyManager,
    Stage,
    PaladinModel,
    Door,
    TeleportPoint,
    SecurityTeleportPoint,
    GoalPoint,
    Items,
    SecurityPC,
    EventDialoguePoint
};

class GameContext;

class DebugUI
{
public:
    void Draw(GameContext& gameContext);

private:
    void DrawHierarchy(GameContext& gameContext);
    void DrawInspector(GameContext& gameContext);
    void DrawEnemyHierarchy(GameContext& gameContext);
    void DrawEnemyInspector(GameContext& gameContext);
    void DrawItemHierarchy(GameContext& gameContext);
    void DrawItemInspector(GameContext& gameContext);
    void HandlePlacementViewport(GameContext& gameContext);
    void DrawCameraController(CameraController& cameraController);

    SelectedObjectType selectedObject_{
        SelectedObjectType::None
    };

    GizmoDrawer gizmoDrawer_;
    DoorMove door_;
    GameObject stageProxy_{ GameObjectType::Stage, "StageProxy" };

    int selectedTeleportIndex_{ 0 };
    int selectedEnemyIndex_{ 0 };
    int selectedSecurityTeleportIndex_{ 0 };
    int selectedItemIndex_{ 0 };
    int selectedEventPointIndex_{ 0 };
    int selectedDoorIndex_{ 0 };
    int selectedStageIndex_{ 0 };
    ItemType newItemType_ = ItemType::Medkit;
    std::optional<Item> pendingItem_;
    int newEnemyType_ = 0;
    std::optional<demo::EnemySpawnData> pendingEnemy_;
    const char* placementMessage_ = nullptr;
    const char* sceneSaveMessage_ = nullptr;
};