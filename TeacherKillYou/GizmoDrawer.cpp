
#include "GizmoDrawer.h"
#include "DebugUI.h"
#include "GameContext.h"
#include "raymath.h"
#include "GameObject.h"
#include "Item.h"

#include <algorithm>
#include <cstring>

// ==========================================
// Existing GameObject Gizmo
// ==========================================
void GizmoDrawer::Draw(
    GameContext& gameContext,
    GameObject* targetObj)
{
    if (!targetObj)
        return;

    // Shortcut keys
    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
        if (IsKeyPressed(KEY_W) &&
            IsKeyDown(KEY_LEFT_CONTROL))
        {
            currentOperation_ = ImGuizmo::TRANSLATE;
        }

        if (IsKeyPressed(KEY_E) &&
            IsKeyDown(KEY_LEFT_CONTROL))
        {
            currentOperation_ = ImGuizmo::ROTATE;
        }

        if (IsKeyPressed(KEY_R) &&
            IsKeyDown(KEY_LEFT_CONTROL))
        {
            currentOperation_ = ImGuizmo::SCALE;
        }
    }

    auto& cameraController =
        gameContext.GetCameraController();

    const Camera3D& camera =
        cameraController.GetActiveRaylibCamera();

    ImGuiIO& io = ImGui::GetIO();

    if (io.DisplaySize.y <= 0.0f)
        return;

    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

    ImGuizmo::SetRect(
        0.0f,
        0.0f,
        io.DisplaySize.x,
        io.DisplaySize.y
    );

    ImGuizmo::Enable(true);

    float aspect =
        io.DisplaySize.x / io.DisplaySize.y;

    Matrix rlView = GetCameraMatrix(camera);

    Matrix rlProj = MatrixPerspective(
        camera.fovy * DEG2RAD,
        aspect,
        0.01f,
        1000.0f
    );

    Matrix viewMat = MatrixTranspose(rlView);
    Matrix projMat = MatrixTranspose(rlProj);

    Vector3 pos = targetObj->GetPosition();
    Vector3 rot = targetObj->GetRotation();
    Vector3 scale = targetObj->GetScale();

    scale.x = (std::max)(0.001f, scale.x);
    scale.y = (std::max)(0.001f, scale.y);
    scale.z = (std::max)(0.001f, scale.z);

    Matrix rlScale =
        MatrixScale(scale.x, scale.y, scale.z);

    Matrix rlRotX =
        MatrixRotateX(rot.x * DEG2RAD);

    Matrix rlRotY =
        MatrixRotateY(rot.y * DEG2RAD);

    Matrix rlRotZ =
        MatrixRotateZ(rot.z * DEG2RAD);

    Matrix rlRot = MatrixMultiply(
        MatrixMultiply(rlRotZ, rlRotX),
        rlRotY
    );

    Matrix rlTrans =
        MatrixTranslate(pos.x, pos.y, pos.z);

    Matrix rlModel = MatrixMultiply(
        MatrixMultiply(rlScale, rlRot),
        rlTrans
    );

    Matrix modelMat = MatrixTranspose(rlModel);

    bool manipulated = ImGuizmo::Manipulate(
        &viewMat.m0,
        &projMat.m0,
        currentOperation_,
        ImGuizmo::WORLD,
        &modelMat.m0
    );

    if (manipulated)
    {
        float matrix[16];

        std::memcpy(
            matrix,
            &modelMat.m0,
            sizeof(float) * 16
        );

        float matrixTranslation[3];
        float matrixRotation[3];
        float matrixScale[3];

        ImGuizmo::DecomposeMatrixToComponents(
            matrix,
            matrixTranslation,
            matrixRotation,
            matrixScale
        );

        matrixScale[0] =
            (std::max)(0.001f, matrixScale[0]);

        matrixScale[1] =
            (std::max)(0.001f, matrixScale[1]);

        matrixScale[2] =
            (std::max)(0.001f, matrixScale[2]);

        targetObj->SetPosition({
            matrixTranslation[0],
            matrixTranslation[1],
            matrixTranslation[2]
            });

        targetObj->SetRotation({
            matrixRotation[0],
            matrixRotation[1],
            matrixRotation[2]
            });

        targetObj->SetScale({
            matrixScale[0],
            matrixScale[1],
            matrixScale[2]
            });
    }
}

void GizmoDrawer::DrawItem(
    GameContext& gameContext,
    Item* item)
{
    if (item == nullptr || item->IsCollected())
        return;

    auto& cameraController =
        gameContext.GetCameraController();

    const Camera3D& camera =
        cameraController.GetActiveRaylibCamera();

    ImGuiIO& io = ImGui::GetIO();

    if (io.DisplaySize.y <= 0.0f)
        return;

    ImGuizmo::SetDrawlist(
        ImGui::GetForegroundDrawList()
    );

    ImGuizmo::SetRect(
        0.0f,
        0.0f,
        io.DisplaySize.x,
        io.DisplaySize.y
    );

    ImGuizmo::Enable(true);

    const float aspect =
        io.DisplaySize.x / io.DisplaySize.y;

    Matrix rlView = GetCameraMatrix(camera);

    Matrix rlProj = MatrixPerspective(
        camera.fovy * DEG2RAD,
        aspect,
        0.01f,
        1000.0f
    );

    Matrix viewMat = MatrixTranspose(rlView);
    Matrix projMat = MatrixTranspose(rlProj);

    // Item world position
    Vector3 position = item->GetPosition();

    // Create translation-only matrix
    Matrix rlModel = MatrixTranslate(
        position.x,
        position.y,
        position.z
    );

    Matrix modelMat = MatrixTranspose(rlModel);

    // Items currently support translation only
    bool manipulated = ImGuizmo::Manipulate(
        &viewMat.m0,
        &projMat.m0,
        ImGuizmo::TRANSLATE,
        ImGuizmo::WORLD,
        &modelMat.m0
    );

    if (manipulated)
    {
        float translation[3];
        float rotation[3];
        float scale[3];

        ImGuizmo::DecomposeMatrixToComponents(
            &modelMat.m0,
            translation,
            rotation,
            scale
        );

        item->SetPosition({
            translation[0],
            translation[1],
            translation[2]
            });
    }
}
