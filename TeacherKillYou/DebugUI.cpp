#include "DebugUI.h"
#include "GameContext.h"
#include "imgui.h"

void DebugUI::Draw(GameContext& gameContext) {
    auto& cameraController = gameContext.GetCameraController();

    if (cameraController.GetActiveType() == CameraType::System) {
        ImGuizmo::BeginFrame();

        DrawHierarchy(gameContext);
        DrawInspector(gameContext);

        // 選択対象のポインタをギズモに渡す
        GameObject* targetObj = nullptr;
        if (selectedObject_ == SelectedObjectType::Player) {
            targetObj = &gameContext.GetPlayer();
        }
        else if (selectedObject_ == SelectedObjectType::Door) {
            targetObj = &gameContext.GetDoor(); // GameContext に Door の getter を用意
        }

        else if (selectedObject_ == SelectedObjectType::TeleportPoint) {
            auto& tps = gameContext.GetTeleportPoints();
            if (!tps.empty() && selectedTeleportIndex_ >= 0 && selectedTeleportIndex_ < tps.size()) {
                targetObj = &tps[selectedTeleportIndex_]; // 選択中のテレポートポイントをギズモに渡す
            }
        }

        gizmoDrawer_.Draw(gameContext, targetObj);
    }

    DrawCameraController(cameraController);
}

void DebugUI::DrawHierarchy(GameContext& gameContext)
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

    if (ImGui::Selectable("Door", selectedObject_ == SelectedObjectType::Door))
    {
        selectedObject_ = SelectedObjectType::Door;
    }

    auto& tps = gameContext.GetTeleportPoints();
    for (size_t i = 0; i < tps.size(); ++i)
    {
        char label[64];
        const char* typeStr = (tps[i].GetTeleportType() == TeleportType::Source) ? "Source" : "Destination";
        snprintf(label, sizeof(label), "Teleport Point %d [%s]", (int)i, typeStr);

        bool isSelected = (selectedObject_ == SelectedObjectType::TeleportPoint && selectedTeleportIndex_ == (int)i);
        if (ImGui::Selectable(label, isSelected))
        {
            selectedObject_ = SelectedObjectType::TeleportPoint;
            selectedTeleportIndex_ = static_cast<int>(i);
        }
    }

    if (ImGui::Button("+ Add Teleport Point"))
    {
        TeleportPoint newTp;
        newTp.SetPosition({ 0.0f, 0.0f, 0.0f });
        tps.push_back(newTp);
        selectedObject_ = SelectedObjectType::TeleportPoint;
        selectedTeleportIndex_ = static_cast<int>(tps.size()) - 1;
    }

    ImGui::End();
}

void DebugUI::DrawInspector(GameContext& gameContext)
{
    // 1つのウィンドウ（"Inspector"）の先頭で Begin を1回だけ呼ぶ
    ImGui::Begin("Inspector");

    // --- シーブ・ロード用のボタンをここに配置 ---
    if (ImGui::Button("Save Scene (JSON)"))
    {
		gameContext.SaveScene("scene.json");
    }

    ImGui::Separator();

    ImGui::Separator();

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

    case SelectedObjectType::Door:
    {
        DoorMove& door = gameContext.GetDoor();

        Vector3 position = door.GetPosition();
        Vector3 rotation = door.GetRotation();
        Vector3 scale = door.GetScale();
        float speed = door.GetSlideSpeed();
        float distance = door.GetOpenDistance();

        ImGui::Text("Door Transform");

        if (ImGui::DragFloat3("Position", &position.x, 0.1f)) door.SetPosition(position);
        if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) door.SetRotation(rotation);
        if (ImGui::DragFloat3("Scale", &scale.x, 0.05f, 0.1f, 10.0f)) door.SetScale(scale);

        ImGui::Separator();
        ImGui::Text("Door Controls");

        // スライド速度・移動距離の調整
        if (ImGui::SliderFloat("Slide Speed", &speed, 0.1f, 10.0f)) door.SetSlideSpeed(speed);
        if (ImGui::DragFloat("Open Distance", &distance, 0.1f)) door.SetOpenDistance(distance);

        // インスペクター上でのテスト開閉ボタン
        if (ImGui::Button("Toggle Door"))
        {
            door.Toggle();
        }
        break;
    }

    case SelectedObjectType::TeleportPoint:
    {
        auto& tps = gameContext.GetTeleportPoints();
        if (!tps.empty())
        {
            if (selectedTeleportIndex_ >= tps.size()) selectedTeleportIndex_ = 0;

            ImGui::Text("Teleport Points Count: %d", (int)tps.size());
            if (ImGui::Button("Add Point"))
            {
                TeleportPoint newTp;
                newTp.SetPosition(gameContext.GetPlayer().GetPosition());
                tps.push_back(newTp);
                selectedTeleportIndex_ = static_cast<int>(tps.size()) - 1;
            }

            ImGui::Separator();

            TeleportPoint& tp = tps[selectedTeleportIndex_];
            Vector3 pos = tp.GetPosition();
            Vector3 rot = tp.GetRotation();
            Vector3 scale = tp.GetScale();
            int pointNum = tp.GetPointNumber();
            int typeVal = static_cast<int>(tp.GetTeleportType());
            float radius = tp.GetRadius();

            // インデックス変更用
            ImGui::DragInt("Point Index", &selectedTeleportIndex_, 1.0f, 0, (int)tps.size() - 1);
            if (selectedTeleportIndex_ < 0) selectedTeleportIndex_ = 0;
            if (selectedTeleportIndex_ >= tps.size()) selectedTeleportIndex_ = static_cast<int>(tps.size()) - 1;

            ImGui::Text("Transform");
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) tp.SetPosition(pos);
            if (ImGui::DragFloat3("Rotation", &rot.x, 1.0f)) tp.SetRotation(rot);
            if (ImGui::DragFloat3("Scale", &scale.x, 0.05f, 0.1f, 10.0f)) tp.SetScale(scale);

            ImGui::Separator();
            ImGui::Text("Teleport Settings");
            if (ImGui::DragInt("Pair Number", &pointNum, 1.0f, 0, 99)) tp.SetPointNumber(pointNum);

            const char* types[] = { "Source (Origin)", "Destination (Target)" };
            if (ImGui::Combo("Type", &typeVal, types, IM_ARRAYSIZE(types)))
            {
                tp.SetTeleportType(static_cast<TeleportType>(typeVal));
            }
            if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 10.0f)) tp.SetRadius(radius);
        }
        else
        {
            if (ImGui::Button("Add Point"))
            {
                TeleportPoint newTp;
                tps.push_back(newTp);
                selectedTeleportIndex_ = 0;
            }
        }
        break;
    }

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