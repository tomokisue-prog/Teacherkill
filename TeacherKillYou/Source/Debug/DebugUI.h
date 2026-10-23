#pragma once

#include "CameraController.h"
#include "GizmoDrawer.h"
#include "DoorMove.h"

enum class SelectedObjectType
{
    None,
    Player,
    EnemyManager,
    Stage,
    PaladinModel,
    Door,
    TeleportPoint,
    GoalPoint,
    Items
};

class GameContext;

class DebugUI
{
public:
    void Draw(GameContext& gameContext);

private:
    void DrawHierarchy(GameContext& gameContext);
    void DrawInspector(GameContext& gameContext);
    void DrawCameraController(CameraController& cameraController);

    SelectedObjectType selectedObject_{
        SelectedObjectType::None
    };

    GizmoDrawer gizmoDrawer_;
    DoorMove door_;

    int selectedTeleportIndex_{ 0 };

    int selectedItemIndex_{ 0 };
};