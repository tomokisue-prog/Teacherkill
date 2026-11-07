#include "GizmoDrawer.h"
#include "DebugUI.h"
#include "GameContext.h"
#include "raymath.h"
#include "GameObject.h"

#include <algorithm>
#include <cstring>
#include <cmath>

bool GizmoDrawer::Draw(
    GameContext& gameContext,
    GameObject* targetObj, bool uprightOnly)
{
    if (!targetObj)
        return false;

    // ImGuiキーボード入力使用時は、ギズモ用ショートカットを無効化
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

        if (!uprightOnly && IsKeyPressed(KEY_R) &&
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
        return false;

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

    // ImGuizmoの内部Decompose順序(XYZ)に合わせて合成
    Matrix rlRot = MatrixMultiply(
        MatrixMultiply(rlRotX, rlRotY),
        rlRotZ
    );

    Matrix rlTrans =
        MatrixTranslate(pos.x, pos.y, pos.z);

    Matrix rlModel = MatrixMultiply(
        MatrixMultiply(rlScale, rlRot),
        rlTrans
    );

    Matrix modelMat = MatrixTranspose(rlModel);

    ImGuizmo::OPERATION operation = currentOperation_;
    if (uprightOnly)
        operation = currentOperation_ == ImGuizmo::ROTATE ? ImGuizmo::ROTATE_Y : ImGuizmo::TRANSLATE;

    // LOCALモードで操作することで、選択した軸だけを独立して回転可能にする
    const bool manipulated = ImGuizmo::Manipulate(
        &viewMat.m0,
        &projMat.m0,
        operation,
        ImGuizmo::LOCAL,
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

        if (uprightOnly) {
            const float yaw = std::atan2(-matrix[2], matrix[0]) * RAD2DEG;
            targetObj->SetRotation({ 0.0f, yaw, 0.0f });
        }
        else {
            targetObj->SetRotation({ matrixRotation[0], matrixRotation[1], matrixRotation[2] });
        }

        targetObj->SetScale({
            matrixScale[0],
            matrixScale[1],
            matrixScale[2]
            });
    }
    return manipulated;
}