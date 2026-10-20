
#pragma once

#include "imgui.h"
#include "ImGuizmo.h"
#include "raylib.h"

class GameContext;
class GameObject;
class Item;

class GizmoDrawer
{
public:
    GizmoDrawer() = default;

    void Draw(GameContext& gameContext, GameObject* targetObj);

    void DrawItem(GameContext& gameContext, Item* item);

    ImGuizmo::OPERATION GetOperation() const
    {
        return currentOperation_;
    }

    void SetOperation(ImGuizmo::OPERATION op)
    {
        currentOperation_ = op;
    }

private:
    ImGuizmo::OPERATION currentOperation_{
        ImGuizmo::TRANSLATE
    };
};
