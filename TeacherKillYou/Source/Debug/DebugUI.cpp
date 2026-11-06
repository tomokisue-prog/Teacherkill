#include "DebugUI.h"
#include "GameContext.h"
#include "imgui.h"
#include "SceneManager.h"
#include "SaveSystem.h"
#include "../Events/SecurityTeleportPoint.h"


void DebugUI::Draw(GameContext& gameContext)
{
    auto& cameraController =
        gameContext.GetCameraController();

    // 配置編集とギズモ操作は、システムカメラ使用時だけ有効にする。
    if (cameraController.GetActiveType() == CameraType::System)
    {
        ImGuizmo::BeginFrame();

        DrawHierarchy(gameContext);
        DrawInspector(gameContext);

        GameObject* targetObj = nullptr;

        if (selectedObject_ == SelectedObjectType::Player)
        {
            targetObj = &gameContext.GetPlayer();
        }
        else if (selectedObject_ == SelectedObjectType::Door)
        {
            // 複数ドアのうち、選択されているインデックスのドアをギズモターゲットにする
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
        // 追加：Enemy（スポーンポイント）が選択された場合のギズモターゲット設定
        else if (selectedObject_ == SelectedObjectType::Enemy)
        {
            auto& spawnPoints = gameContext.GetEnemyManager().GetSpawnPoints();
            if (selectedEnemyIndex_ >= 0 &&
                selectedEnemyIndex_ < static_cast<int>(spawnPoints.size()))
            {
                targetObj = &spawnPoints[selectedEnemyIndex_];
            }
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

        if (selectedObject_ == SelectedObjectType::Items)
        {
            auto& items =
                gameContext.GetInventory().GetWorldItems();

            if (selectedItemIndex_ >= 0 &&
                selectedItemIndex_ < static_cast<int>(items.size()))
            {
                Item& selectedItem = items[selectedItemIndex_];

                if (!selectedItem.IsCollected())
                {
                    gizmoDrawer_.DrawItem(
                        gameContext,
                        &selectedItem
                    );
                }
            }
        }
        else
        {
            // Enemy が選択されている場合もここで gizmoDrawer_.Draw(gameContext, targetObj) が動くよう条件から外す
            gizmoDrawer_.Draw(
                gameContext,
                targetObj
            );
        }
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

    // TeleportPoint と同様に、登録されている敵を個別にリスト表示する
    auto& spawnPoints = gameContext.GetEnemyManager().GetSpawnPoints();
    for (size_t i = 0; i < spawnPoints.size(); ++i)
    {
        char label[64];
        const char* typeStr = (spawnPoints[i].config.type == demo::EnemyType::FemaleZombie) ? "FemaleZombie" : "Monster";
        snprintf(label, sizeof(label), "Enemy %d [%s]", (int)i, typeStr);

        bool isSelected = (selectedObject_ == SelectedObjectType::Enemy && selectedEnemyIndex_ == (int)i);
        if (ImGui::Selectable(label, isSelected))
        {
            selectedObject_ = SelectedObjectType::Enemy;
            selectedEnemyIndex_ = static_cast<int>(i);
        }
    }

    // 敵を追加するボタン
    if (ImGui::Button("+ Add Enemy"))
    {
        demo::EnemySpawnData newSp;
		newSp.SetPosition(gameContext.GetPlayer().GetPosition());
        newSp.condition = demo::EnemySpawnCondition::None; // 最初はスポーンしない状態
        spawnPoints.push_back(newSp);
        selectedObject_ = SelectedObjectType::Enemy;
        selectedEnemyIndex_ = static_cast<int>(spawnPoints.size()) - 1;
    }

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

    // ドアを追加するボタン
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

    // --- World Items ---
    if (ImGui::TreeNode("Items"))
    {
        auto& items =
            gameContext.GetInventory().GetWorldItems();

        if (items.empty())
        {
            ImGui::TextDisabled("No world items");
        }

        for (int i = 0;
            i < static_cast<int>(items.size());
            ++i)
        {
            Item& item = items[i];

            ImGui::PushID(i);

            const bool selected =
                selectedObject_ == SelectedObjectType::Items &&
                selectedItemIndex_ == i;

            if (ImGui::Selectable(
                Item::GetName(item.GetType()),
                selected))
            {
                selectedObject_ = SelectedObjectType::Items;
                selectedItemIndex_ = i;
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
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
        SaveSystem::SaveScene(gameContext, "scene.json");
    }

    ImGui::Separator();

    // ギズモ操作モード切替 UI

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

    case SelectedObjectType::Enemy:
    {
        auto& enemyMgr = gameContext.GetEnemyManager();
        auto& spawnPoints = enemyMgr.GetSpawnPoints();

        if (!spawnPoints.empty())
        {
            if (selectedEnemyIndex_ >= static_cast<int>(spawnPoints.size()))
            {
                selectedEnemyIndex_ = static_cast<int>(spawnPoints.size()) - 1;
            }
            if (selectedEnemyIndex_ < 0) selectedEnemyIndex_ = 0;

            ImGui::Text("Enemies Count: %d", (int)spawnPoints.size());
            if (ImGui::Button("Add Enemy"))
            {
                demo::EnemySpawnData newSp;
                newSp.SetPosition(gameContext.GetPlayer().GetPosition());
                newSp.condition = demo::EnemySpawnCondition::None;
                spawnPoints.push_back(newSp);
                selectedEnemyIndex_ = static_cast<int>(spawnPoints.size()) - 1;
            }

            ImGui::Separator();

            demo::EnemySpawnData& sp = spawnPoints[selectedEnemyIndex_];
            Vector3 pos = sp.GetPosition();

            // インデックス変更用
            ImGui::DragInt("Enemy Index", &selectedEnemyIndex_, 1.0f, 0, (int)spawnPoints.size() - 1);
            if (selectedEnemyIndex_ < 0) selectedEnemyIndex_ = 0;
            if (selectedEnemyIndex_ >= static_cast<int>(spawnPoints.size()))
            {
                selectedEnemyIndex_ = static_cast<int>(spawnPoints.size()) - 1;
            }

            ImGui::Text("Transform");
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
            {
                sp.SetPosition(pos);
            }

            // 敵の種類選択やスポーン条件などのUIをここに記述...
            int typeVal = static_cast<int>(sp.config.type);
            const char* typeNames[] = { "Monster", "FemaleZombie" };
            if (ImGui::Combo("Enemy Type", &typeVal, typeNames, IM_ARRAYSIZE(typeNames))) {
                sp.config.type = static_cast<demo::EnemyType>(typeVal);
            }

            int condVal = static_cast<int>(sp.condition);
            const char* condNames[] = {
                "None (Do Not Spawn)",
                "Normal (Always Spawn)",
                "LostItem Collected"
            };
            if (ImGui::Combo("Spawn Condition", &condVal, condNames, IM_ARRAYSIZE(condNames))) {
                sp.condition = static_cast<demo::EnemySpawnCondition>(condVal);
            }

            int despawnVal = static_cast<int>(sp.despawnCondition);
            const char* despawnNames[] = {
                "Normal (Do Not Despawn)",
                "Despawn On Security Teleport"
            };
            if (ImGui::Combo("Despawn Type", &despawnVal, despawnNames, IM_ARRAYSIZE(despawnNames))) {
                sp.despawnCondition = static_cast<demo::EnemyDespawnCondition>(despawnVal);
            }

            ImGui::Text("Status: %s", sp.spawned ? "Spawned" : "Waiting");

            if (ImGui::Button("Reset Spawn Status")) {
                sp.spawned = false;
            }

            if (ImGui::Button("Delete Enemy")) {
                spawnPoints.erase(spawnPoints.begin() + selectedEnemyIndex_);
                selectedEnemyIndex_--;
                if (selectedEnemyIndex_ < 0) selectedEnemyIndex_ = 0;
            }
        }
        else
        {
            if (ImGui::Button("Add Enemy"))
            {
                demo::EnemySpawnData newSp;
                newSp.SetPosition(gameContext.GetPlayer().GetPosition());
                newSp.condition = demo::EnemySpawnCondition::None;
                spawnPoints.push_back(newSp);
                selectedEnemyIndex_ = 0;
            }
        }
        break;
    }

    case SelectedObjectType::Stage:
        ImGui::Text("Stage");
        break;

    case SelectedObjectType::PaladinModel:
        ImGui::Text("Paladin Model");
        break;

    case SelectedObjectType::Door:
    {
        // 複数ドアの管理とインスペクター表示
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
                "Security Teleport Entered"
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

            // インデックス変更用
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
        auto& items =
            gameContext.GetInventory().GetWorldItems();

        if (selectedItemIndex_ < 0 ||
            selectedItemIndex_ >= static_cast<int>(items.size()) ||
            items[selectedItemIndex_].IsCollected())
        {
            ImGui::Text("No item selected.");
            break;
        }

        Item& item = items[selectedItemIndex_];

        Vector3 position = item.GetPosition();
        Vector3 rotation = item.GetRotation();

        ImGui::Text("Item: %s",
            Item::GetName(item.GetType()));

        ImGui::Separator();
        ImGui::Text("Transform");

        if (ImGui::DragFloat3(
            "Position", &position.x, 0.05f))
        {
			item.SetPosition(position);
        }

        if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f))
        {
            item.SetRotation(rotation);
        }

        if (ImGui::Button("Move To Player"))
        {
            Vector3 pos =
                gameContext.GetPlayer().GetPosition();

            pos.y += 0.5f;
            item.SetPosition(pos);
        }

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

            // 有効化条件を選択するコンボボックス
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

            // ★ 追加：トリガー済みフラグ
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