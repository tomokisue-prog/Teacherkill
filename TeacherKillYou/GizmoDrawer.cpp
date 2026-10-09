#include "GizmoDrawer.h"
#include "DebugUI.h"
#include "GameContext.h"
#include "raymath.h"
#include "GameObject.h"

void GizmoDrawer::Draw(GameContext& gameContext, GameObject* targetObj) {
    if (!targetObj) return;

    // ショートカットキー切り替え (W: 移動, E: 回転, R: 拡大)
    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
		//ショートカットキー　W+Ctrl,E+Ctrl,R+Ctrlで切り替え
        if (IsKeyPressed(KEY_W) && IsKeyDown(KEY_LEFT_CONTROL)) currentOperation_ = ImGuizmo::TRANSLATE;
        if (IsKeyPressed(KEY_E) && IsKeyDown(KEY_LEFT_CONTROL)) currentOperation_ = ImGuizmo::ROTATE;
        if (IsKeyPressed(KEY_R) && IsKeyDown(KEY_LEFT_CONTROL)) currentOperation_ = ImGuizmo::SCALE;
    }

    auto& cameraController = gameContext.GetCameraController();
    const Camera3D& camera = cameraController.GetActiveRaylibCamera();

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
    ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Enable(true);

    float aspect = io.DisplaySize.x / io.DisplaySize.y;
    Matrix rlView = GetCameraMatrix(camera);
    Matrix rlProj = MatrixPerspective(camera.fovy * DEG2RAD, aspect, 0.01f, 1000.0f);

    Matrix viewMat = MatrixTranspose(rlView);
    Matrix projMat = MatrixTranspose(rlProj);

    // GameObject から Transform を取得
    Vector3 pos = targetObj->GetPosition();
    Vector3 rot = targetObj->GetRotation();
    Vector3 scale = targetObj->GetScale();

    Matrix rlScale = MatrixScale(scale.x, scale.y, scale.z);
    Matrix rlRotX = MatrixRotateX(rot.x * DEG2RAD);
    Matrix rlRotY = MatrixRotateY(rot.y * DEG2RAD);
    Matrix rlRotZ = MatrixRotateZ(rot.z * DEG2RAD);
    Matrix rlRot = MatrixMultiply(MatrixMultiply(rlRotZ, rlRotX), rlRotY);
    Matrix rlTrans = MatrixTranslate(pos.x, pos.y, pos.z);

    Matrix rlModel = MatrixMultiply(MatrixMultiply(rlScale, rlRot), rlTrans);
    Matrix modelMat = MatrixTranspose(rlModel);

    bool manipulated = ImGuizmo::Manipulate(
        &viewMat.m0, &projMat.m0, currentOperation_, ImGuizmo::WORLD, &modelMat.m0
    );

    if (manipulated)
    {
        float matrix[16];
        memcpy(matrix, &modelMat.m0, sizeof(float) * 16);

        float matrixTranslation[3], matrixRotation[3], matrixScale[3];
        ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);

        // GameObject に直接結果を反映
        targetObj->SetPosition({ matrixTranslation[0], matrixTranslation[1], matrixTranslation[2] });
        targetObj->SetRotation({ matrixRotation[0], matrixRotation[1], matrixRotation[2] });
        targetObj->SetScale({ matrixScale[0], matrixScale[1], matrixScale[2] });
    }
}