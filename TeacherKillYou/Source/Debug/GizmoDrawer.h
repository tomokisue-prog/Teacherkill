
#pragma once

#include "imgui.h"
#include "ImGuizmo.h"
#include "raylib.h"

class GameContext;
class GameObject;

// ‘ÎÛ‚ÌŠ—LŒ ‚Íæ“¾‚¹‚¸AˆÚ“®E‰ñ“]EŠgk‚ÌƒMƒYƒ‚‘€ì‚ğ”½‰f‚·‚éB
class GizmoDrawer
{
public:
    GizmoDrawer() = default;

    bool Draw(GameContext& gameContext, GameObject* targetObj, bool uprightOnly = false);


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
