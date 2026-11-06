#include "DebugUI.h"
#include "ItemCatalog.h"
#include "GameContext.h"
#include "imgui.h"
#include "SceneManager.h"
#include "SaveSystem.h"
#include "../Events/SecurityTeleportPoint.h"
#include "raymath.h"
#include <cmath>

void DebugUI::Draw(GameContext& gameContext)
{
    auto& cameraController =
        gameContext.GetCameraController();

    // 配置編集とギズモ操作は、システムカメラ使用時だけ有効にする。
    if (cameraController.GetActiveType() == CameraType::System)
    {
        ImGuizmo::BeginFrame();

        DrawHierarchy(gameContext);
        if (selectedObject_ != SelectedObjectType::EnemyManager) pendingEnemy_.reset();
        if (selectedObject_ != SelectedObjectType::Items) pendingItem_.reset();
        DrawInspector(gameContext);
        if (cameraController.GetActiveType() != CameraType::System) return;

        GameObject* targetObj = nullptr;

        if (selectedObject_ == SelectedObjectType::Player)
        {
            targetObj = &gameContext.GetPlayer();
        }
        else if (selectedObject_ == SelectedObjectType::Door)
        {
            auto& doors = gameContext.GetDoors();
            if (selectedDoorIndex_ >= 0 &&
                selectedDoorIndex_ < static_cast<int>(doors.size()))
            {
                targetObj = &doors[selectedDoorIndex_];
            }
        }
        else if (selectedObject_ == SelectedObjectType::TeleportPoint)
        {
            auto& tps = gameContext.GetTeleportPoints();

            if (selectedTeleportIndex_ >= 0 &&
                selectedTeleportIndex_ < static_cast<int>(tps.size()))
            {
                targetObj = &tps[selectedTeleportIndex_];
            }
        }
        else if (selectedObject_ == SelectedObjectType::GoalPoint)
        {
            targetObj = &gameContext.GetGoalPoint();
        }
        else if (selectedObject_ == SelectedObjectType::SecurityPC)
        {
            targetObj = &gameContext.GetSecurityPC();
        }
        else if (selectedObject_ == SelectedObjectType::EventDialoguePoint)
        {
            auto& eventPoints = gameContext.GetEventDialoguePoints();
            if (selectedEventPointIndex_ >= 0 &&
                selectedEventPointIndex_ < static_cast<int>(eventPoints.size()))
            {
                targetObj = static_cast<GameObject*>(&eventPoints[selectedEventPointIndex_]);
            }
        }
        else if (selectedObject_ == SelectedObjectType::SecurityTeleportPoint)
        {
            auto& stps = gameContext.GetSecurityTeleportPoints();

            if (selectedSecurityTeleportIndex_ >= 0 &&
                selectedSecurityTeleportIndex_ < static_cast<int>(stps.size()))
            {
                targetObj = &stps[selectedSecurityTeleportIndex_];
            }
        }

        gizmoDrawer_.Draw(gameContext, targetObj);
        HandlePlacementViewport(gameContext);
    }
    else
    {
        pendingEnemy_.reset();
        pendingItem_.reset();
    }

    DrawCameraController(cameraController);
}

void DebugUI::DrawHierarchy(GameContext& gameContext)
{
    ImGui::Begin("Scene Next");
    if (ImGui::Button("Goal Scene"))
    {
        SM().ChangeScene(SceneID::Goal);
    }
    ImGui::End();

    ImGui::Begin("Hierarchy");

    if (ImGui::Selectable("Player", selectedObject_ == SelectedObjectType::Player))
    {
        selectedObject_ = SelectedObjectType::Player;
    }

    if (ImGui::Selectable("Security PC", selectedObject_ == SelectedObjectType::SecurityPC))
    {
        selectedObject_ = SelectedObjectType::SecurityPC;
    }

    DrawEnemyHierarchy(gameContext);
    auto& eventPoints = gameContext.GetEventDialoguePoints();
    for (size_t i = 0; i < eventPoints.size(); ++i)
    {
        char label[64];
        snprintf(label, sizeof(label), "Event Point %d [%s]", (int)i, eventPoints[i].GetDialogueId().c_str());

        bool isSelected = (selectedObject_ == SelectedObjectType::EventDialoguePoint && selectedEventPointIndex_ == (int)i);
        if (ImGui::Selectable(label, isSelected))
        {
            selectedObject_ = SelectedObjectType::EventDialoguePoint;
            selectedEventPointIndex_ = static_cast<int>(i);
        }
    }

    if (ImGui::Button("+ Add Event Point"))
    {
        EventDialoguePoint newEp;
        newEp.SetPosition(gameContext.GetPlayer().GetPosition());
        eventPoints.push_back(newEp);
        selectedObject_ = SelectedObjectType::EventDialoguePoint;
        selectedEventPointIndex_ = static_cast<int>(eventPoints.size()) - 1;
    }
    DrawItemHierarchy(gameContext);

    if (ImGui::Selectable("Stage", selectedObject_ == SelectedObjectType::Stage))
    {
        selectedObject_ = SelectedObjectType::Stage;
    }

    if (ImGui::Selectable("Paladin Model", selectedObject_ == SelectedObjectType::PaladinModel))
    {
        selectedObject_ = SelectedObjectType::PaladinModel;
    }

    auto& doors = gameContext.GetDoors();
    for (size_t i = 0; i < doors.size(); ++i)
    {
        char label[64];
        snprintf(label, sizeof(label), "Door %d", (int)i);

        bool isSelected = (selectedObject_ == SelectedObjectType::Door && selectedDoorIndex_ == (int)i);
        if (ImGui::Selectable(label, isSelected))
        {
            selectedObject_ = SelectedObjectType::Door;
            selectedDoorIndex_ = static_cast<int>(i);
        }
    }

    if (ImGui::Button("+ Add Door"))
    {
        DoorMove newDoor;
        newDoor.SetPosition(gameContext.GetPlayer().GetPosition());
        doors.push_back(newDoor);
        selectedObject_ = SelectedObjectType::Door;
        selectedDoorIndex_ = static_cast<int>(doors.size()) - 1;
    }

    if (ImGui::Selectable("Goal Point", selectedObject_ == SelectedObjectType::GoalPoint))
    {
        selectedObject_ = SelectedObjectType::GoalPoint;
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

    auto& stps = gameContext.GetSecurityTeleportPoints();
    for (size_t i = 0; i < stps.size(); ++i)
    {
        char label[64];
        const char* typeStr = (stps[i].GetTeleportType() == SecurityTeleportType::Source) ? "Source (In)" : "Destination (Out)";
        snprintf(label, sizeof(label), "Security Teleport %d [%s]", (int)i, typeStr);

        bool isSelected = (selectedObject_ == SelectedObjectType::SecurityTeleportPoint && selectedSecurityTeleportIndex_ == (int)i);
        if (ImGui::Selectable(label, isSelected))
        {
            selectedObject_ = SelectedObjectType::SecurityTeleportPoint;
            selectedSecurityTeleportIndex_ = static_cast<int>(i);
        }
    }

    if (ImGui::Button("+ Add Security Teleport Point"))
    {
        SecurityTeleportPoint newStp;
        newStp.SetPosition(gameContext.GetPlayer().GetPosition());
        stps.push_back(newStp);
        selectedObject_ = SelectedObjectType::SecurityTeleportPoint;
        selectedSecurityTeleportIndex_ = static_cast<int>(stps.size()) - 1;
    }

    ImGui::End();
}

void DebugUI::DrawInspector(GameContext& gameContext)
{
    ImGui::Begin("Inspector");

    if (ImGui::Button("Save Scene (JSON)"))
    {
        sceneSaveMessage_ = SaveSystem::SaveScene(gameContext)
            ? u8"scene.json に保存しました"
            : u8"保存できませんでした。出力ログを確認してください。";
    }
    if (sceneSaveMessage_) ImGui::TextWrapped("%s", sceneSaveMessage_);

    ImGui::Separator();

    ImGui::Text("Gizmo Mode");
    const bool uprightPlacement = selectedObject_ == SelectedObjectType::EnemyManager ||
        selectedObject_ == SelectedObjectType::Items;
    ImGuizmo::OPERATION currentOp = gizmoDrawer_.GetOperation();

    if (ImGui::RadioButton("Translate (Ctrl+W)", currentOp == ImGuizmo::TRANSLATE ||
        (uprightPlacement && currentOp == ImGuizmo::SCALE))) {
        gizmoDrawer_.SetOperation(ImGuizmo::TRANSLATE);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate (Ctrl+E)", currentOp == ImGuizmo::ROTATE)) {
        gizmoDrawer_.SetOperation(ImGuizmo::ROTATE);
    }
    if (!uprightPlacement) {
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale (Ctrl+R)", currentOp == ImGuizmo::SCALE))
            gizmoDrawer_.SetOperation(ImGuizmo::SCALE);
    }

    ImGui::Separator();

    switch (selectedObject_)
    {
    case SelectedObjectType::Player:
    {
        Player& player = gameContext.GetPlayer();

        Vector3 position = player.GetPosition();
        Vector3 rotation = player.GetRotation();
        Vector3 scale = player.GetScale();

        bool debugDraw = player.IsDebugDrawEnabled();
        bool hasGun = player.HasGun();

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

        ImGui::Separator();
        ImGui::Text("Debug Settings");

        if (ImGui::Checkbox("Draw Collider Debug", &debugDraw))
        {
            player.SetDebugDrawEnabled(debugDraw);
        }

        if (ImGui::Checkbox("Has Gun", &hasGun))
        {
            player.SetHasGun(hasGun);
        }

        break;
    }

    case SelectedObjectType::EnemyManager:
        DrawEnemyInspector(gameContext);
        break;

    case SelectedObjectType::Stage:
        ImGui::Text("Stage");
        break;

    case SelectedObjectType::PaladinModel:
        ImGui::Text("Paladin Model");
        break;

    case SelectedObjectType::Door:
    {
        auto& doors = gameContext.GetDoors();
        if (!doors.empty())
        {
            if (selectedDoorIndex_ >= static_cast<int>(doors.size())) selectedDoorIndex_ = 0;

            ImGui::Text("Doors Count: %d", (int)doors.size());
            if (ImGui::Button("Add Door"))
            {
                DoorMove newDoor;
                newDoor.SetPosition(gameContext.GetPlayer().GetPosition());
                doors.push_back(newDoor);
                selectedDoorIndex_ = static_cast<int>(doors.size()) - 1;
            }

            ImGui::Separator();

            DoorMove& door = doors[selectedDoorIndex_];
            Vector3 position = door.GetPosition();
            Vector3 rotation = door.GetRotation();
            Vector3 scale = door.GetScale();
            float speed = door.GetSlideSpeed();
            float distance = door.GetOpenDistance();

            ImGui::DragInt("Door Index", &selectedDoorIndex_, 1.0f, 0, (int)doors.size() - 1);
            if (selectedDoorIndex_ < 0) selectedDoorIndex_ = 0;
            if (selectedDoorIndex_ >= static_cast<int>(doors.size())) selectedDoorIndex_ = static_cast<int>(doors.size()) - 1;

            ImGui::Text("Door Transform");

            if (ImGui::DragFloat3("Position", &position.x, 0.1f)) door.SetPosition(position);
            if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) door.SetRotation(rotation);
            if (ImGui::DragFloat3("Scale", &scale.x, 0.05f, 0.1f, 10.0f)) door.SetScale(scale);

            ImGui::Separator();
            ImGui::Text("Door Controls & Logic");

            if (ImGui::SliderFloat("Slide Speed", &speed, 0.1f, 10.0f)) door.SetSlideSpeed(speed);
            if (ImGui::DragFloat("Open Distance", &distance, 0.1f)) door.SetOpenDistance(distance);

            int lockTypeVal = static_cast<int>(door.GetLockType());
            const char* lockTypeNames[] = {
                "None (Always Operable)",
                "LostItem Collected"
            };
            if (ImGui::Combo("Lock Condition", &lockTypeVal, lockTypeNames, IM_ARRAYSIZE(lockTypeNames)))
            {
                door.SetLockType(static_cast<DoorLockType>(lockTypeVal));
            }

            int openCondVal = static_cast<int>(door.GetOpenConditionType());
            const char* openCondNames[] = {
                "None (Normal Open)",
                "Test: Press M Key",
                "Security Teleport Entered",
                "Security PC Activated"
            };
            if (ImGui::Combo("Open Condition", &openCondVal, openCondNames, IM_ARRAYSIZE(openCondNames)))
            {
                door.SetOpenConditionType(static_cast<DoorOpenConditionType>(openCondVal));
            }

            if (ImGui::Button("Toggle Door"))
            {
                door.Toggle();
            }

            if (ImGui::Button("Delete Door"))
            {
                doors.erase(doors.begin() + selectedDoorIndex_);
                selectedDoorIndex_--;
                if (selectedDoorIndex_ < 0) selectedDoorIndex_ = 0;
            }
        }
        else
        {
            if (ImGui::Button("Add Door"))
            {
                DoorMove newDoor;
                newDoor.SetPosition(gameContext.GetPlayer().GetPosition());
                doors.push_back(newDoor);
                selectedDoorIndex_ = 0;
            }
        }
        break;
    }

    case SelectedObjectType::TeleportPoint:
    {
        auto& tps = gameContext.GetTeleportPoints();
        if (!tps.empty())
        {
            if (selectedTeleportIndex_ >= static_cast<int>(tps.size())) selectedTeleportIndex_ = 0;

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

            ImGui::DragInt("Point Index", &selectedTeleportIndex_, 1.0f, 0, (int)tps.size() - 1);
            if (selectedTeleportIndex_ < 0) selectedTeleportIndex_ = 0;
            if (selectedTeleportIndex_ >= static_cast<int>(tps.size())) selectedTeleportIndex_ = static_cast<int>(tps.size()) - 1;

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
    case SelectedObjectType::GoalPoint:
    {
        GoalPoint& goal = gameContext.GetGoalPoint();
        Vector3 pos = goal.GetPosition();
        Vector3 rot = goal.GetRotation();
        Vector3 scale = goal.GetScale();
        float radius = goal.GetRadius();

        ImGui::Text("Goal Point Transform");
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) goal.SetPosition(pos);
        if (ImGui::DragFloat3("Rotation", &rot.x, 1.0f)) goal.SetRotation(rot);
        if (ImGui::DragFloat3("Scale", &scale.x, 0.05f, 0.1f, 10.0f)) goal.SetScale(scale);

        ImGui::Separator();
        ImGui::Text("Goal Settings");
        if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 10.0f)) goal.SetRadius(radius);

        break;
    }
    case SelectedObjectType::Items:
    {
        DrawItemInspector(gameContext);
        break;
    }
    case SelectedObjectType::SecurityPC:
    {
        SecurityPC& pc = gameContext.GetSecurityPC();
        Vector3 position = pc.GetPosition();
        Vector3 rotation = pc.GetRotation();
        Vector3 scale = pc.GetScale();

        ImGui::Text("Security PC Transform");
        if (ImGui::DragFloat3("Position", &position.x, 0.1f)) pc.SetPosition(position);
        if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f)) pc.SetRotation(rotation);
        if (ImGui::DragFloat3("Scale", &scale.x, 0.05f, 0.1f, 10.0f)) pc.SetScale(scale);
        break;
    }

    case SelectedObjectType::EventDialoguePoint:
    {
        auto& eventPoints = gameContext.GetEventDialoguePoints();
        if (!eventPoints.empty())
        {
            if (selectedEventPointIndex_ >= static_cast<int>(eventPoints.size())) selectedEventPointIndex_ = 0;

            ImGui::Text("Event Points Count: %d", (int)eventPoints.size());
            if (ImGui::Button("Add Point"))
            {
                EventDialoguePoint newEp;
                newEp.SetPosition(gameContext.GetPlayer().GetPosition());
                eventPoints.push_back(newEp);
                selectedEventPointIndex_ = static_cast<int>(eventPoints.size()) - 1;
            }

            ImGui::Separator();

            EventDialoguePoint& ep = eventPoints[selectedEventPointIndex_];
            Vector3 pos = ep.GetPosition();
            Vector3 rot = ep.GetRotation();
            Vector3 scale = ep.GetScale();
            float radius = ep.GetRadius();

            ImGui::DragInt("Point Index", &selectedEventPointIndex_, 1.0f, 0, (int)eventPoints.size() - 1);
            if (selectedEventPointIndex_ < 0) selectedEventPointIndex_ = 0;
            if (selectedEventPointIndex_ >= static_cast<int>(eventPoints.size()))
            {
                selectedEventPointIndex_ = static_cast<int>(eventPoints.size()) - 1;
            }

            ImGui::Text("Transform");
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) ep.SetPosition(pos);
            if (ImGui::DragFloat3("Rotation", &rot.x, 1.0f)) ep.SetRotation(rot);
            if (ImGui::DragFloat3("Scale", &scale.x, 0.05f, 0.1f, 10.0f)) ep.SetScale(scale);

            ImGui::Separator();
            ImGui::Text("Event Settings");

            char idBuffer[128];
            strncpy_s(idBuffer, ep.GetDialogueId().c_str(), sizeof(idBuffer));
            if (ImGui::InputText("Dialogue ID", idBuffer, sizeof(idBuffer)))
            {
                ep.SetDialogueId(std::string(idBuffer));
            }

            int condVal = static_cast<int>(ep.GetCondition());
            const char* condNames[] = {
                "None (Do Not Trigger)",
                "Normal (Always Active)",
                "LostItem Collected"
            };
            if (ImGui::Combo("Event Condition", &condVal, condNames, IM_ARRAYSIZE(condNames)))
            {
                ep.SetCondition(static_cast<EventDialogueCondition>(condVal));
            }

            if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 20.0f)) ep.SetRadius(radius);

            bool triggered = ep.IsTriggered();
            if (ImGui::Checkbox("Triggered", &triggered))
            {
                ep.SetTriggered(triggered);
            }

            if (ImGui::Button("Delete Event Point"))
            {
                eventPoints.erase(eventPoints.begin() + selectedEventPointIndex_);
                selectedEventPointIndex_--;
                if (selectedEventPointIndex_ < 0) selectedEventPointIndex_ = 0;
            }
        }
        else
        {
            if (ImGui::Button("Add Point"))
            {
                EventDialoguePoint newEp;
                eventPoints.push_back(newEp);
                selectedEventPointIndex_ = 0;
            }
        }
        break;
    }

    case SelectedObjectType::SecurityTeleportPoint:
    {
        auto& stps = gameContext.GetSecurityTeleportPoints();
        if (!stps.empty())
        {
            if (selectedSecurityTeleportIndex_ >= static_cast<int>(stps.size())) selectedSecurityTeleportIndex_ = 0;

            ImGui::Text("Security Teleport Points Count: %d", (int)stps.size());
            if (ImGui::Button("Add Point"))
            {
                SecurityTeleportPoint newStp;
                newStp.SetPosition(gameContext.GetPlayer().GetPosition());
                stps.push_back(newStp);
                selectedSecurityTeleportIndex_ = static_cast<int>(stps.size()) - 1;
            }

            ImGui::Separator();

            SecurityTeleportPoint& stp = stps[selectedSecurityTeleportIndex_];
            Vector3 pos = stp.GetPosition();
            Vector3 rot = stp.GetRotation();
            Vector3 scale = stp.GetScale();
            int pointNum = stp.GetPointNumber();
            int typeVal = static_cast<int>(stp.GetTeleportType());
            float radius = stp.GetRadius();

            ImGui::DragInt("Point Index", &selectedSecurityTeleportIndex_, 1.0f, 0, (int)stps.size() - 1);
            if (selectedSecurityTeleportIndex_ < 0) selectedSecurityTeleportIndex_ = 0;
            if (selectedSecurityTeleportIndex_ >= static_cast<int>(stps.size()))
            {
                selectedSecurityTeleportIndex_ = static_cast<int>(stps.size()) - 1;
            }

            ImGui::Text("Transform");
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) stp.SetPosition(pos);
            if (ImGui::DragFloat3("Rotation", &rot.x, 1.0f)) stp.SetRotation(rot);
            if (ImGui::DragFloat3("Scale", &scale.x, 0.05f, 0.1f, 10.0f)) stp.SetScale(scale);

            ImGui::Separator();
            ImGui::Text("Security Teleport Settings");
            if (ImGui::DragInt("Pair Number", &pointNum, 1.0f, 0, 99)) stp.SetPointNumber(pointNum);

            const char* types[] = { "Source (Origin - Hallway)", "Destination (Target - Security Room)" };
            if (ImGui::Combo("Type (One-Way)", &typeVal, types, IM_ARRAYSIZE(types)))
            {
                stp.SetTeleportType(static_cast<SecurityTeleportType>(typeVal));
            }

            int condVal = static_cast<int>(stp.GetCondition());
            const char* condNames[] = {
                "None (Do Not Trigger)",
                "Normal (Always Active)",
                "LostItem Collected"
            };
            if (ImGui::Combo("Condition", &condVal, condNames, IM_ARRAYSIZE(condNames)))
            {
                stp.SetCondition(static_cast<SecurityTeleportCondition>(condVal));
            }

            if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 10.0f)) stp.SetRadius(radius);

            bool triggered = stp.IsTriggered();
            if (ImGui::Checkbox("Triggered (Used)", &triggered))
            {
                stp.SetTriggered(triggered);
            }

            if (ImGui::Button("Delete Security Teleport Point"))
            {
                stps.erase(stps.begin() + selectedSecurityTeleportIndex_);
                selectedSecurityTeleportIndex_--;
                if (selectedSecurityTeleportIndex_ < 0) selectedSecurityTeleportIndex_ = 0;
            }
        }
        else
        {
            if (ImGui::Button("Add Point"))
            {
                SecurityTeleportPoint newStp;
                newStp.SetPosition(gameContext.GetPlayer().GetPosition());
                stps.push_back(newStp);
                selectedSecurityTeleportIndex_ = 0;
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

namespace {
    // Persisted enemy IDs are not zero-based UI indices. Keep each label with its ID.
    constexpr struct { demo::EnemyType type; const char* name; } EnemyChoices[] = {
        {demo::EnemyType::FemaleZombie, "ZOMBIE"},
        {demo::EnemyType::MonsterMale2, "MONSTER MALE 2"}
    };

    const char* EnemyTypeName(demo::EnemyType type) {
        for (const auto& choice : EnemyChoices)
            if (choice.type == type) return choice.name;
        return "UNKNOWN";
    }

    bool EnemyTypeCombo(const char* label, demo::EnemyType& type) {
        if (!ImGui::BeginCombo(label, EnemyTypeName(type))) return false;
        bool changed = false;
        for (const auto& choice : EnemyChoices) {
            const bool selected = choice.type == type;
            if (ImGui::Selectable(choice.name, selected)) {
                type = choice.type;
                changed = !selected;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
        return changed;
    }

    constexpr ImGuiSliderFlags ClampInput = ImGuiSliderFlags_AlwaysClamp;

    template<class Placement>
    bool HasSelection(int index, const std::vector<Placement>& placements) {
        return index >= 0 && static_cast<std::size_t>(index) < placements.size();
    }

    BoundingBox PlacementBounds(const demo::EnemySpawnData& placement) {
        const auto p = placement.GetPosition();
        const auto& c = placement.config;
        return { {p.x - c.bodyRadius, p.y, p.z - c.bodyRadius},
                {p.x + c.bodyRadius, p.y + c.bodyHeight, p.z + c.bodyRadius} };
    }

    const char* ItemName(ItemType type) {
        return GetItemDefinition(type).displayName;
    }

    bool ItemTypeCombo(const char* label, ItemType& type) {
        if (!ImGui::BeginCombo(label, ItemName(type))) return false;
        bool changed = false;
        for (const auto& option : GetItemDefinitions()) {
            if (!option.modelKey) continue;
            const bool selected = option.type == type;
            if (ImGui::Selectable(option.displayName, selected)) {
                type = option.type;
                changed = !selected;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
        return changed;
    }

    bool DrawPlacementGizmo(GizmoDrawer& gizmo, GameContext& context, Vector3& position, float& yaw) {
        GameObject transform;
        transform.SetPosition(position);
        transform.SetRotation({ 0, yaw * RAD2DEG, 0 });
        if (!gizmo.Draw(context, &transform, true)) return false;
        const auto p = transform.GetPosition();
        const auto angle = transform.GetRotation().y * DEG2RAD;
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(angle)) return false;
        if (gizmo.GetOperation() == ImGuizmo::ROTATE)
            yaw = std::remainder(angle, 2 * PI);
        else
            position = Vector3Clamp(p, { -100000, -100000, -100000 }, { 100000, 100000, 100000 });
        return true;
    }
}

void DebugUI::DrawEnemyHierarchy(GameContext& gameContext)
{
    if (!ImGui::CollapsingHeader(u8"Enemies / 敵配置", ImGuiTreeNodeFlags_DefaultOpen)) return;
    auto& spawnPoints = gameContext.GetEnemyManager().GetSpawnPoints();
    ImGui::PushID("EnemyPlacements");
    EnemyTypeCombo(u8"追加する種類", newEnemyType_);
    ImGui::BeginDisabled(spawnPoints.size() >= demo::MaxEnemyPlacements);
    if (ImGui::Button(u8"+ 敵を配置")) {
        pendingEnemy_ = demo::EnemySpawnData{};
        pendingEnemy_->config = demo::EnemyConfig::ForType(newEnemyType_);
        pendingEnemy_->condition = demo::EnemySpawnCondition::Normal;
        pendingEnemy_->despawnCondition = demo::EnemyDespawnCondition::None;
        selectedObject_ = SelectedObjectType::EnemyManager;
        placementMessage_ = nullptr;
    }
    ImGui::EndDisabled();
    if (pendingEnemy_) {
        ImGui::TextWrapped(u8"%s を置く床をクリック", EnemyTypeName(pendingEnemy_->config.type));
        if (ImGui::Button(u8"配置をキャンセル")) {
            pendingEnemy_.reset();
            placementMessage_ = nullptr;
        }
    }
    if (pendingEnemy_ && placementMessage_) ImGui::TextWrapped("%s", placementMessage_);

    ImGui::Text(u8"配置数 %zu", spawnPoints.size());
    for (int i = 0; i < static_cast<int>(spawnPoints.size()); ++i) {
        const bool selected = selectedObject_ == SelectedObjectType::EnemyManager && selectedEnemyIndex_ == i;
        const auto type = spawnPoints[i].config.type;
        if (ImGui::Selectable(TextFormat("%d  %s##enemy%d", i + 1, EnemyTypeName(type), i), selected)) {
            selectedEnemyIndex_ = i;
            selectedObject_ = SelectedObjectType::EnemyManager;
            pendingEnemy_.reset();
        }
    }
    ImGui::PopID();
}

void DebugUI::DrawEnemyInspector(GameContext& gameContext)
{
    auto& enemyManager = gameContext.GetEnemyManager();
    auto& spawnPoints = enemyManager.GetSpawnPoints();

    ImGui::TextUnformatted(u8"敵の配置設定");
    if (ImGui::Button(u8"この配置でプレイ")) {
        enemyManager.Clear();
        for (auto& sp : spawnPoints) {
            sp.spawned = false;
            sp.spawnedEnemyId = 0;
            if (sp.condition == demo::EnemySpawnCondition::Normal) {
                sp.spawnedEnemyId = enemyManager.Spawn(sp.GetPosition(), sp.config, sp.yaw);
                sp.spawned = true;
            }
        }
        pendingEnemy_.reset();
        gameContext.GetCameraController().SetActiveCamera(CameraType::Player);
        return;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"配置した位置・条件・ステータスで敵を再生成し、プレイに戻ります。");

    if (!HasSelection(selectedEnemyIndex_, spawnPoints)) {
        ImGui::TextWrapped(u8"Hierarchyで敵を選ぶか、床に新しく配置してください。");
        return;
    }

    ImGui::PushID(selectedEnemyIndex_);
    auto& placement = spawnPoints[selectedEnemyIndex_];
    auto& config = placement.config;
    bool changed = EnemyTypeCombo(u8"種類", config.type);

    Vector3 position = placement.GetPosition();
    changed |= ImGui::DragFloat3(u8"位置", &position.x, .05f, -100000, 100000, "%.2f", ClampInput);
    if (changed) placement.SetPosition(position);

    float degrees = placement.yaw * RAD2DEG;
    if (ImGui::DragFloat(u8"向き", &degrees, 1, -180, 180, "%.0f deg", ClampInput)) {
        placement.yaw = degrees * DEG2RAD;
        changed = true;
    }
    if (ImGui::Button(u8"近くの床に合わせる")) {
        placement.SetPosition(gameContext.GetStage().FindGroundPosition(placement.GetPosition()));
        changed = true;
    }

    int spawnCondition = static_cast<int>(placement.condition);
    const char* spawnConditions[] = {
        "None (Do Not Spawn)",
        "Normal (Always Spawn)",
        "LostItem Collected"
    };
    if (ImGui::Combo(u8"スポーン条件", &spawnCondition, spawnConditions, IM_ARRAYSIZE(spawnConditions))) {
        placement.condition = static_cast<demo::EnemySpawnCondition>(spawnCondition);
        placement.spawned = false;
        placement.spawnedEnemyId = 0;
        changed = true;
    }

    int despawnCondition = static_cast<int>(placement.despawnCondition);
    const char* despawnConditions[] = {
        "Normal (Do Not Despawn)",
        "Despawn On Security Teleport"
    };
    if (ImGui::Combo(u8"デスポーン条件", &despawnCondition, despawnConditions, IM_ARRAYSIZE(despawnConditions))) {
        placement.despawnCondition = static_cast<demo::EnemyDespawnCondition>(despawnCondition);
        changed = true;
    }

    ImGui::Text("Status: %s", placement.spawned ? "Spawned" : "Waiting");
    if (ImGui::Button("Reset Spawn Status")) {
        placement.spawned = false;
        placement.spawnedEnemyId = 0;
        changed = true;
    }

    ImGui::Separator();
    changed |= ImGui::DragInt(u8"最大HP", &config.maxHp, 1, 1, 100000, "%d", ClampInput);
    changed |= ImGui::DragInt(u8"攻撃力", &config.attackDamage, 1, 0, 100000, "%d", ClampInput);
    changed |= ImGui::DragFloat(u8"移動速度", &config.moveSpeed, .05f, 0, 100, "%.2f", ClampInput);
    changed |= ImGui::DragFloat(u8"索敵距離", &config.detectionRange, .1f, 0, 9999, "%.2f", ClampInput);
    changed |= ImGui::DragFloat(u8"攻撃距離", &config.attackRange, .05f, .05f, 100, "%.2f", ClampInput);
    changed |= ImGui::DragFloat(u8"アニメーション速度", &config.animationSpeed, .05f, .1f, 4, "%.2fx", ClampInput);

    if (ImGui::CollapsingHeader(u8"詳細設定")) {
        changed |= ImGui::DragFloat(u8"高さ", &config.bodyHeight, .05f, .01f, 100, "%.2f", ClampInput);
        changed |= ImGui::DragFloat(u8"当たり判定の半径", &config.bodyRadius, .05f, .01f, 100, "%.2f", ClampInput);
        changed |= ImGui::DragFloat(u8"追跡をやめる距離", &config.loseRange, .1f, 0, 10000, "%.2f", ClampInput);
        changed |= ImGui::DragFloat(u8"接近を止める距離", &config.stopDistance, .05f, 0, 99.9f, "%.2f", ClampInput);
        changed |= ImGui::DragFloat(u8"接近を再開する距離", &config.resumeDistance, .05f, 0, 100, "%.2f", ClampInput);
        changed |= ImGui::DragFloat(u8"旋回速度 (rad/s)", &config.turnSpeed, .05f, 0, 100, "%.2f", ClampInput);
        changed |= ImGui::DragFloat(u8"攻撃の命中位置 (0–1)", &config.attackHitFraction, .01f, .01f, .99f, "%.2f", ClampInput);
        changed |= ImGui::DragFloat(u8"次の攻撃までの秒数", &config.attackRecovery, .05f, 0, 10000, "%.2f", ClampInput);
        changed |= ImGui::DragFloat(u8"死体を残す秒数", &config.corpseDuration, .05f, 0, 10000, "%.2f", ClampInput);
        if (ImGui::Button(u8"この種類の初期設定に戻す")) {
            config = demo::EnemyConfig::ForType(config.type);
            changed = true;
        }
    }

    if (changed) {
        config.Validate();
        sceneSaveMessage_ = nullptr;
    }

    ImGui::Separator();
    ImGui::BeginDisabled(spawnPoints.size() >= demo::MaxEnemyPlacements);
    if (ImGui::Button(u8"複製して配置")) {
        pendingEnemy_ = placement;
        placementMessage_ = nullptr;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(u8"削除")) ImGui::OpenPopup("Delete Enemy Placement");
    if (ImGui::BeginPopupModal("Delete Enemy Placement", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(u8"敵 %d の配置を削除しますか？", selectedEnemyIndex_ + 1);
        if (ImGui::Button(u8"削除する")) {
            spawnPoints.erase(spawnPoints.begin() + selectedEnemyIndex_);
            selectedEnemyIndex_ = -1;
            pendingEnemy_.reset();
            sceneSaveMessage_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"キャンセル")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void DebugUI::DrawItemHierarchy(GameContext& gameContext)
{
    if (!ImGui::CollapsingHeader(u8"Items / アイテム・備品", ImGuiTreeNodeFlags_DefaultOpen)) return;
    const auto& placements = gameContext.GetInventory().GetPlacements();
    ImGui::PushID("ItemPlacements");
    ItemTypeCombo(u8"追加する種類", newItemType_);
    ImGui::BeginDisabled(placements.size() >= MaxItemPlacements);
    if (ImGui::Button(u8"+ アイテムを配置")) {
        pendingItem_.emplace(newItemType_, Vector3{});
        selectedObject_ = SelectedObjectType::Items;
        placementMessage_ = nullptr;
    }
    ImGui::EndDisabled();
    if (pendingItem_) {
        ImGui::TextWrapped(u8"%s を置く床や机の上をクリック", ItemName(pendingItem_->GetType()));
        if (ImGui::Button(u8"配置をキャンセル")) pendingItem_.reset();
        if (placementMessage_) ImGui::TextWrapped("%s", placementMessage_);
    }
    ImGui::Text(u8"配置数 %zu", placements.size());
    for (int i = 0; i < static_cast<int>(placements.size()); ++i) {
        const bool selected = selectedObject_ == SelectedObjectType::Items && selectedItemIndex_ == i;
        if (ImGui::Selectable(TextFormat("%d  %s##item%d", i + 1, ItemName(placements[i].GetType()), i), selected)) {
            selectedObject_ = SelectedObjectType::Items;
            selectedItemIndex_ = i;
            pendingItem_.reset();
        }
    }
    ImGui::PopID();
}

void DebugUI::DrawItemInspector(GameContext& gameContext)
{
    auto& inventory = gameContext.GetInventory();
    auto& placements = inventory.GetPlacements();
    ImGui::TextUnformatted(u8"アイテムと備品の配置");
    if (ImGui::Button(u8"アイテム配置でプレイ")) {
        inventory.ApplyPlacements();
        pendingItem_.reset();
        gameContext.GetCameraController().SetActiveCamera(CameraType::Player);
        return;
    }
    if (!HasSelection(selectedItemIndex_, placements)) {
        ImGui::TextWrapped(u8"Hierarchyでアイテムを選ぶか、新しく配置してください。");
        return;
    }

    ImGui::PushID(selectedItemIndex_);
    auto& item = placements[selectedItemIndex_];
    auto type = item.GetType();
    if (ItemTypeCombo(u8"種類", type)) {
        item.SetType(type);
        sceneSaveMessage_ = nullptr;
    }
    ImGui::TextDisabled(item.IsPickup() ? u8"取得アイテム" : u8"装飾用の備品");
    auto position = item.GetPosition();
    if (ImGui::DragFloat3(u8"位置", &position.x, .05f, -100000, 100000, "%.2f", ClampInput)) {
        item.SetPosition(position);
        sceneSaveMessage_ = nullptr;
    }
    float degrees = item.GetYaw() * RAD2DEG;
    if (ImGui::DragFloat(u8"向き", &degrees, 1, -180, 180, "%.0f deg", ClampInput)) {
        item.SetYaw(degrees * DEG2RAD);
        sceneSaveMessage_ = nullptr;
    }
    float scale = item.GetScale();
    if (ImGui::DragFloat(u8"サイズ倍率", &scale, .02f, .05f, 20, "%.2fx", ClampInput)) {
        item.SetScale(scale);
        sceneSaveMessage_ = nullptr;
    }
    if (ImGui::Button(u8"近くの床に合わせる")) {
        item.SetPosition(gameContext.GetStage().FindGroundPosition(position));
        sceneSaveMessage_ = nullptr;
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"プレイヤーの足元へ")) {
        item.SetPosition(gameContext.GetStage().FindGroundPosition(gameContext.GetPlayer().GetPosition()));
        sceneSaveMessage_ = nullptr;
    }

    ImGui::Separator();
    ImGui::BeginDisabled(placements.size() >= MaxItemPlacements);
    if (ImGui::Button(u8"複製して配置")) {
        pendingItem_ = item;
        placementMessage_ = nullptr;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(u8"削除")) ImGui::OpenPopup("Delete Item Placement");
    if (ImGui::BeginPopupModal("Delete Item Placement", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(u8"%s の配置を削除しますか？", ItemName(item.GetType()));
        if (ImGui::Button(u8"削除する")) {
            placements.erase(placements.begin() + selectedItemIndex_);
            selectedItemIndex_ = -1;
            pendingItem_.reset();
            sceneSaveMessage_ = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"キャンセル")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void DebugUI::HandlePlacementViewport(GameContext& gameContext)
{
    auto& spawnPoints = gameContext.GetEnemyManager().GetSpawnPoints();
    auto& items = gameContext.GetInventory().GetPlacements();
    const bool placing = pendingEnemy_.has_value() || pendingItem_.has_value();
    if (!placing && selectedObject_ == SelectedObjectType::EnemyManager &&
        HasSelection(selectedEnemyIndex_, spawnPoints)) {
        auto& placement = spawnPoints[selectedEnemyIndex_];
        Vector3 pos = placement.GetPosition();
        float yaw = -placement.yaw;
        if (DrawPlacementGizmo(gizmoDrawer_, gameContext, pos, yaw)) {
            placement.SetPosition(pos);
            placement.yaw = -yaw;
            sceneSaveMessage_ = nullptr;
        }
    }
    if (!placing && selectedObject_ == SelectedObjectType::Items && HasSelection(selectedItemIndex_, items)) {
        auto& item = items[selectedItemIndex_];
        auto position = item.GetPosition();
        auto yaw = item.GetYaw();
        if (DrawPlacementGizmo(gizmoDrawer_, gameContext, position, yaw)) {
            item.SetPosition(position);
            item.SetYaw(yaw);
            sceneSaveMessage_ = nullptr;
        }
    }

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
        ImGui::GetIO().WantCaptureMouse || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
        (!placing && (ImGuizmo::IsOver() || ImGuizmo::IsUsing()))) return;

    const auto& camera = gameContext.GetCameraController().GetActiveRaylibCamera();
    const Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    auto surface = gameContext.GetStage().Raycast(ray, 300.0f);
    float distance = surface.hit ? surface.distance : 300.0f;
    int hitItem = -1;
    if (!pendingEnemy_) {
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            if (pendingItem_ && items[i].IsPickup()) continue;
            const auto hit = items[i].RaycastPlacement(ray);
            if (hit.hit && hit.distance >= 0 && hit.distance < distance) {
                distance = hit.distance;
                surface = hit;
                hitItem = i;
            }
        }
    }

    if (placing) {
        if (!surface.hit || surface.normal.y <= .5f || ray.direction.y >= 0) {
            placementMessage_ = pendingEnemy_ ? u8"床の上をクリックしてください。" : u8"床や備品の上面をクリックしてください。";
            return;
        }
        if (pendingEnemy_ && spawnPoints.size() < demo::MaxEnemyPlacements) {
            pendingEnemy_->SetPosition(surface.point);
            spawnPoints.push_back(*pendingEnemy_);
            selectedEnemyIndex_ = static_cast<int>(spawnPoints.size()) - 1;
        }
        else if (pendingItem_ && items.size() < MaxItemPlacements) {
            pendingItem_->SetPosition(surface.point);
            items.push_back(*pendingItem_);
            selectedItemIndex_ = static_cast<int>(items.size()) - 1;
        }
        else {
            placementMessage_ = u8"配置数の上限に達しました。";
            return;
        }
        pendingEnemy_.reset();
        pendingItem_.reset();
        placementMessage_ = nullptr;
        sceneSaveMessage_ = nullptr;
        return;
    }

    if (hitItem >= 0) {
        selectedItemIndex_ = hitItem;
        selectedObject_ = SelectedObjectType::Items;
    }
    for (int i = 0; i < static_cast<int>(spawnPoints.size()); ++i) {
        const auto hit = GetRayCollisionBox(ray, PlacementBounds(spawnPoints[i]));
        if (hit.hit && hit.distance >= 0 && hit.distance < distance) {
            distance = hit.distance;
            selectedEnemyIndex_ = i;
            selectedObject_ = SelectedObjectType::EnemyManager;
        }
    }
}