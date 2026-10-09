#include "DebugUI.h"
#include "GameContext.h"
#include "imgui.h"

void DebugUI::Draw(GameContext& gameContext) {
    auto& cameraController = gameContext.GetCameraController();

    if (cameraController.GetActiveType() == CameraType::System) {
        ImGuizmo::BeginFrame();

        DrawHierarchy();
        DrawInspector(gameContext);

        // 選択されている列挙型に応じてターゲットの GameObject* を特定
        GameObject* targetObj = nullptr;
        if (selectedObject_ == SelectedObjectType::Player) {
            targetObj = &gameContext.GetPlayer();
        }

        // ギズモ描画クラスへポインタを渡す
        gizmoDrawer_.Draw(gameContext, targetObj);
    }

    DrawCameraController(cameraController);
}

void DebugUI::DrawHierarchy()
{
    ImGui::Begin("Hierarchy");

    if (ImGui::Selectable("Player", selectedObject_ == SelectedObjectType::Player))
    {
        selectedObject_ = SelectedObjectType::Player;
    }

    if (ImGui::Selectable("Enemy Manager", selectedObject_ == SelectedObjectType::EnemyManager))
    {
        selectedObject_ = SelectedObjectType::EnemyManager;
    }

    if (ImGui::Selectable("Stage", selectedObject_ == SelectedObjectType::Stage))
    {
        selectedObject_ = SelectedObjectType::Stage;
    }

    if (ImGui::Selectable("Paladin Model", selectedObject_ == SelectedObjectType::PaladinModel))
    {
        selectedObject_ = SelectedObjectType::PaladinModel;
    }

    ImGui::End();
}

void DebugUI::DrawInspector(GameContext& gameContext)
{
    ImGui::Begin("Inspector");

    // ----------------------------------------------------
    // ギズモ操作モード切替 UI（Translate / Rotate / Scale）
    // ----------------------------------------------------
    ImGui::Text("Gizmo Mode");
    ImGuizmo::OPERATION currentOp = gizmoDrawer_.GetOperation();

    if (ImGui::RadioButton("Translate (W)", currentOp == ImGuizmo::TRANSLATE)) {
        gizmoDrawer_.SetOperation(ImGuizmo::TRANSLATE);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate (E)", currentOp == ImGuizmo::ROTATE)) {
        gizmoDrawer_.SetOperation(ImGuizmo::ROTATE);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale (R)", currentOp == ImGuizmo::SCALE)) {
        gizmoDrawer_.SetOperation(ImGuizmo::SCALE);
    }

    ImGui::Separator();

    // ----------------------------------------------------
    // オブジェクトの Transform パラメータ編集
    // ----------------------------------------------------
    switch (selectedObject_)
    {
    case SelectedObjectType::Player:
    {
        Player& player = gameContext.GetPlayer();

        Vector3 position = player.GetPosition();
        Vector3 rotation = player.GetRotation();
        Vector3 scale = player.GetScale();

        ImGui::Text("Player Transform");

        if (ImGui::DragFloat3("Position", &position.x, 0.1f))
        {
            player.SetPosition(position);
        }

        if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f))
        {
            player.SetRotation(rotation);
        }

        if (ImGui::DragFloat3("Scale", &scale.x, 0.05f, 0.1f, 10.0f))
        {
            player.SetScale(scale);
        }

        break;
    }

    case SelectedObjectType::EnemyManager:
        ImGui::Text("Enemy Manager");
        break;

    case SelectedObjectType::Stage:
        ImGui::Text("Stage");
        break;

    case SelectedObjectType::PaladinModel:
        ImGui::Text("Paladin Model");
        break;

    default:
        ImGui::Text("No object selected.");
        break;
    }

    ImGui::End();
}

void DebugUI::DrawCameraController(CameraController& cameraController)
{
    ImGui::Begin("Camera Controller");

    const bool isPlayerCamera =
        cameraController.GetActiveType() == CameraType::Player;

    if (ImGui::RadioButton("Player Camera", isPlayerCamera))
    {
        cameraController.SetActiveCamera(CameraType::Player);
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("System Camera", !isPlayerCamera))
    {
        cameraController.SetActiveCamera(CameraType::System);
    }

    const auto& camera = cameraController.GetActiveRaylibCamera();

    ImGui::Separator();

    ImGui::Text("Pos   : (%.2f, %.2f, %.2f)",
        camera.position.x,
        camera.position.y,
        camera.position.z);

    ImGui::Text("Target: (%.2f, %.2f, %.2f)",
        camera.target.x,
        camera.target.y,
        camera.target.z);

    ImGui::End();
}