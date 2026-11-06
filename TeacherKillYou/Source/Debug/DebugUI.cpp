#include "DebugUI.h"
#include "GameContext.h"
#include "imgui.h"
#include "SceneManager.h"
#include "SaveSystem.h"
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
            targetObj = &gameContext.GetDoor();
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

    DrawEnemyHierarchy(gameContext);
    DrawItemHierarchy(gameContext);

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

    ImGui::End();
}

void DebugUI::DrawInspector(GameContext& gameContext)
{
    // 1つのウィンドウ（"Inspector"）の先頭で Begin を1回だけ呼ぶ
    ImGui::Begin("Inspector");

    // --- シーブ・ロード用のボタンをここに配置 ---
    if (ImGui::Button("Save Scene (JSON)"))
    {
        sceneSaveMessage_ = SaveSystem::SaveScene(gameContext)
            ? u8"scene.json に保存しました"
            : u8"保存できませんでした。出力ログを確認してください。";
    }
    if (sceneSaveMessage_) ImGui::TextWrapped("%s", sceneSaveMessage_);

    ImGui::Separator();

    // ギズモ操作モード切替 UI

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
        DrawItemInspector(gameContext);
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
namespace {
    constexpr const char* EnemyTypes[] = {"ZOMBIE", "MONSTER MALE 2"};
    constexpr demo::EnemyType EnemyKinds[] = {demo::EnemyType::FemaleZombie, demo::EnemyType::MonsterMale2};
    static_assert(IM_ARRAYSIZE(EnemyTypes) == IM_ARRAYSIZE(EnemyKinds));
    int EnemyTypeIndex(demo::EnemyType type) { return type == demo::EnemyType::MonsterMale2 ? 1 : 0; }
    constexpr ImGuiSliderFlags ClampInput = ImGuiSliderFlags_AlwaysClamp;

    template<class Placement>
    bool HasSelection(int index, const std::vector<Placement>& placements) {
        return index >= 0 && static_cast<std::size_t>(index) < placements.size();
    }

    BoundingBox PlacementBounds(const demo::EnemyPlacement& placement) {
        const auto p = placement.position;
        const auto& c = placement.config;
        return {{p.x - c.bodyRadius, p.y, p.z - c.bodyRadius},
                {p.x + c.bodyRadius, p.y + c.bodyHeight, p.z + c.bodyRadius}};
    }

    constexpr struct { ItemType type; const char* name; } ItemChoices[] = {
        {ItemType::Medkit, u8"回復キット"},
        {ItemType::Ammo, u8"弾薬"},
        {ItemType::KeyCard, u8"キーカード"},
        {ItemType::LostItem, u8"携帯電話（忘れ物）"},
        {ItemType::Books, u8"本"},
        {ItemType::Chair1, u8"椅子 1"},
        {ItemType::Chair2, u8"椅子 2"},
        {ItemType::Computer, u8"パソコン"},
        {ItemType::Locker, u8"ロッカー"},
        {ItemType::Whiteboard, u8"ホワイトボード"},
        {ItemType::TrashBin, u8"ごみ箱"},
        {ItemType::Table, u8"机"},
        {ItemType::HorrorTable, u8"ホラー調の机"},
        {ItemType::SlideDoorProp, u8"スライドドア（装飾）"}
    };

    const char* ItemName(ItemType type) {
        for (const auto& option : ItemChoices)
            if (type == option.type) return option.name;
        return Item::GetName(type);
    }

    bool ItemTypeCombo(const char* label, ItemType& type) {
        if (!ImGui::BeginCombo(label, ItemName(type))) return false;
        bool changed = false;
        for (const auto& option : ItemChoices) {
            const bool selected = option.type == type;
            if (ImGui::Selectable(option.name, selected)) {
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
        transform.SetRotation({0, yaw * RAD2DEG, 0});
        if (!gizmo.Draw(context, &transform, true)) return false;
        const auto p = transform.GetPosition();
        const auto angle = transform.GetRotation().y * DEG2RAD;
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(angle)) return false;
        if (gizmo.GetOperation() == ImGuizmo::ROTATE)
            yaw = std::remainder(angle, 2 * PI);
        else
            position = Vector3Clamp(p, {-100000, -100000, -100000}, {100000, 100000, 100000});
        return true;
    }
}

void DebugUI::DrawEnemyHierarchy(GameContext& gameContext)
{
    if (!ImGui::CollapsingHeader(u8"Enemies / 敵配置", ImGuiTreeNodeFlags_DefaultOpen)) return;
    const auto& placements = gameContext.GetEnemyPlacements();
    ImGui::PushID("EnemyPlacements");
    ImGui::Combo(u8"追加する種類", &newEnemyType_, EnemyTypes, IM_ARRAYSIZE(EnemyTypes));
    ImGui::BeginDisabled(placements.size() >= demo::MaxEnemyPlacements);
    if (ImGui::Button(u8"+ 敵を配置")) {
        pendingEnemy_ = demo::EnemyPlacement{};
        pendingEnemy_->config = demo::EnemyConfig::ForType(EnemyKinds[newEnemyType_]);
        selectedObject_ = SelectedObjectType::EnemyManager;
        placementMessage_ = nullptr;
    }
    ImGui::EndDisabled();
    if (pendingEnemy_) {
        ImGui::TextWrapped(u8"%s を置く床をクリック", EnemyTypes[EnemyTypeIndex(pendingEnemy_->config.type)]);
        if (ImGui::Button(u8"配置をキャンセル")) {
            pendingEnemy_.reset();
            placementMessage_ = nullptr;
        }
    }
    if (pendingEnemy_ && placementMessage_) ImGui::TextWrapped("%s", placementMessage_);

    ImGui::Text(u8"配置数 %zu", placements.size());
    for (int i = 0; i < static_cast<int>(placements.size()); ++i) {
        const bool selected = selectedObject_ == SelectedObjectType::EnemyManager && selectedEnemyIndex_ == i;
        const auto type = placements[i].config.type;
        if (ImGui::Selectable(TextFormat("%d  %s##enemy%d", i + 1, EnemyTypes[EnemyTypeIndex(type)], i), selected)) {
            selectedEnemyIndex_ = i;
            selectedObject_ = SelectedObjectType::EnemyManager;
            pendingEnemy_.reset();
        }
    }
    ImGui::PopID();
}

void DebugUI::DrawEnemyInspector(GameContext& gameContext)
{
    auto& placements = gameContext.GetEnemyPlacements();
    ImGui::TextUnformatted(u8"敵の配置設定");
    if (ImGui::Button(u8"この配置でプレイ")) {
        gameContext.ApplyEnemyPlacements();
        pendingEnemy_.reset();
        gameContext.GetCameraController().SetActiveCamera(CameraType::Player);
        return;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(u8"配置した位置とHPで敵を再生成し、プレイに戻ります。");
    if (!HasSelection(selectedEnemyIndex_, placements)) {
        ImGui::TextWrapped(u8"Hierarchyで敵を選ぶか、床に新しく配置してください。");
        return;
    }

    ImGui::PushID(selectedEnemyIndex_);
    auto& placement = placements[selectedEnemyIndex_];
    auto& config = placement.config;
    int type = EnemyTypeIndex(config.type);
    bool changed = ImGui::Combo(u8"種類", &type, EnemyTypes, IM_ARRAYSIZE(EnemyTypes));
    config.type = EnemyKinds[type];
    changed |= ImGui::DragFloat3(u8"位置", &placement.position.x, .05f, -100000, 100000, "%.2f", ClampInput);
    float degrees = placement.yaw * RAD2DEG;
    if (ImGui::DragFloat(u8"向き", &degrees, 1, -180, 180, "%.0f deg", ClampInput)) {
        placement.yaw = degrees * DEG2RAD;
        changed = true;
    }
    if (ImGui::Button(u8"近くの床に合わせる")) {
        placement.position = gameContext.GetStage().FindGroundPosition(placement.position);
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
    ImGui::BeginDisabled(placements.size() >= demo::MaxEnemyPlacements);
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
            placements.erase(placements.begin() + selectedEnemyIndex_);
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
    auto& enemies = gameContext.GetEnemyPlacements();
    auto& items = gameContext.GetInventory().GetPlacements();
    const bool placing = pendingEnemy_.has_value() || pendingItem_.has_value();
    if (!placing && selectedObject_ == SelectedObjectType::EnemyManager &&
        HasSelection(selectedEnemyIndex_, enemies)) {
        auto& placement = enemies[selectedEnemyIndex_];
        float yaw = -placement.yaw;
        if (DrawPlacementGizmo(gizmoDrawer_, gameContext, placement.position, yaw)) {
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
        if (pendingEnemy_ && enemies.size() < demo::MaxEnemyPlacements) {
            pendingEnemy_->position = surface.point;
            enemies.push_back(*pendingEnemy_);
            selectedEnemyIndex_ = static_cast<int>(enemies.size()) - 1;
        } else if (pendingItem_ && items.size() < MaxItemPlacements) {
            pendingItem_->SetPosition(surface.point);
            items.push_back(*pendingItem_);
            selectedItemIndex_ = static_cast<int>(items.size()) - 1;
        } else {
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
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i) {
        const auto hit = GetRayCollisionBox(ray, PlacementBounds(enemies[i]));
        if (hit.hit && hit.distance >= 0 && hit.distance < distance) {
            distance = hit.distance;
            selectedEnemyIndex_ = i;
            selectedObject_ = SelectedObjectType::EnemyManager;
        }
    }
}
