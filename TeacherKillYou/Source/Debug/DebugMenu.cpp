#include "DebugMenu.h"
#ifndef NDEBUG
#include "GameContext.h"
#include "GameSettings.h"
#include "ResourceManager.h"
#include "SaveData.h"
#include "nlohmann/json.hpp"
#include "imgui.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace {
    float Number(const nlohmann::json& data, const char* name, float fallback) {
        const auto found = data.find(name);
        if (found == data.end() || !found->is_number()) return fallback;
        const double value = found->get<double>();
        return std::isfinite(value) && std::abs(value) < 1.e20 ? float(value) : fallback;
    }
    bool Flag(const nlohmann::json& data, const char* name, bool fallback) {
        const auto found = data.find(name);
        return found != data.end() && found->is_boolean() ? found->get<bool>() : fallback;
    }
    bool Percent(const char* label, float& value, float low, float high) {
        float percent = value * 100;
        if (!ImGui::SliderFloat(label, &percent, low * 100, high * 100, "%.0f%%")) return false;
        value = percent / 100;
        return true;
    }
    const char* AttackReason(demo::DebugAttackBlock reason) {
        switch(reason) {
        case demo::DebugAttackBlock::Ready: return u8"距離・向き・遮蔽の条件OK";
        case demo::DebugAttackBlock::Dead: return u8"敵が死亡している";
        case demo::DebugAttackBlock::NoTarget: return u8"有効な標的がいない";
        case demo::DebugAttackBlock::Height: return u8"高さが合わない";
        case demo::DebugAttackBlock::Range: return u8"攻撃範囲の外";
        case demo::DebugAttackBlock::Facing: return u8"プレイヤーの方を向いていない";
        case demo::DebugAttackBlock::Wall: return u8"ステージの壁などで遮られている";
        case demo::DebugAttackBlock::Door: return u8"ドアで遮られている";
        }
        return "";
    }
    const char* ShotTarget(DebugShotTarget target) {
        switch(target) {
        case DebugShotTarget::Enemy: return u8"敵";
        case DebugShotTarget::Wall: return u8"壁";
        default: return u8"空振り";
        }
    }
    void Direction(Vector3 from,Vector3 direction,Color color) {
        direction.y=0;const float length=Vector3Length(direction);if(length<.0001f)return;
        const auto unit=Vector3Scale(direction,1/length),tip=Vector3Add(from,Vector3Scale(unit,1.2f));
        const auto back=Vector3Subtract(tip,Vector3Scale(unit,.25f)),side=Vector3Scale(Vector3CrossProduct(unit,{0,1,0}),.12f);
        DrawLine3D(from,tip,color);DrawLine3D(tip,Vector3Add(back,side),color);DrawLine3D(tip,Vector3Subtract(back,side),color);
    }
    const char* State(demo::EnemyState state) {
        switch (state) {
        case demo::EnemyState::Chase: return u8"追跡";
        case demo::EnemyState::Search: return u8"捜索";
        case demo::EnemyState::Attack: return u8"攻撃";
        case demo::EnemyState::Dead: return u8"死亡";
        default: return u8"待機";
        }
    }
}

void DebugMenu::LoadFonts() {
    auto& io = ImGui::GetIO();
    if (FileExists("Data/Font/NotoSansJP-Regular.ttf"))
        io.Fonts->AddFontFromFileTTF("Data/Font/NotoSansJP-Regular.ttf", 18, nullptr, io.Fonts->GetGlyphRangesJapanese());
    if (io.Fonts->Fonts.empty()) io.Fonts->AddFontDefault();
}

void DebugMenu::Initialize(GameContext& context) {
    if (initialized_) return;
    Load(context);
    initialized_ = true;
}

void DebugMenu::Open() { open_ = true; firstDraw_ = true; status_.clear(); }
void DebugMenu::Close() { open_ = false; }
bool DebugMenu::IsOpen() const { return open_; }

bool DebugMenu::Draw(GameContext& context, GameSettings& settings) {
    if (!open_) return false;
    const float width = (std::min)(680.0f, GetScreenWidth() - 24.0f);
    ImGui::SetNextWindowPos({GetScreenWidth() - width - 12, 24}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize({width, (std::min)(810.0f, GetScreenHeight() - 48.0f)}, ImGuiCond_Appearing);
    ImGui::SetNextWindowBgAlpha(.94f);
    bool displayChanged = false;
    if (ImGui::Begin(u8"デバッグ確認###GameDebugMenu", &open_, ImGuiWindowFlags_NoSavedSettings)) {
        // Escメニューで押したクリックを、新しいパネルの項目へ持ち越さない。
        ImGui::BeginDisabled(firstDraw_);
        ImGui::TextWrapped(u8"背景を見ながら比較できます。敵・移動・攻撃は一時停止中です。Escでメニューに戻ります。");
        if (context.GetCameraController().GetActiveType() == CameraType::System)
            ImGui::TextColored({1,.8f,.3f,1}, u8"System Cameraは明るい編集表示です。画面効果の比較はPlayer Cameraで行ってください。");
        auto effects = context.GetLighting().GetPostProcessSettings();
        if (ImGui::BeginTabBar("DebugTabs")) {
            if (ImGui::BeginTabItem(u8"ブラー・画面")) {
                ImGui::Checkbox(u8"ブラーを有効にする", &effects.blurEnabled);
                ImGui::Combo(u8"ブラーの範囲", &effects.blurMode, u8"ライトの外側\0画面の端\0全画面\0");
                Percent(u8"ブラーの強さ", effects.blurRatio, 0, 1);
                ImGui::SliderFloat(u8"ブラーの半径", &effects.blurRadius, 0, 16, "%.1f px");
                if (ImGui::Button(u8"ブラーOFF")) effects.blurEnabled = false;
                ImGui::SameLine();
                if (ImGui::Button(u8"弱めのブラー")) {
                    effects.blurEnabled = true; effects.blurMode = 1; effects.blurRatio = .25f; effects.blurRadius = 2;
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"従来のブラー")) {
                    effects.blurEnabled = true; effects.blurMode = 0; effects.blurRatio = .75f; effects.blurRadius = 6;
                }
                ImGui::Separator();
                ImGui::Checkbox(u8"グロー（明るい部分のにじみ）", &effects.bloomEnabled);
                ImGui::SliderFloat(u8"グローの強さ", &effects.bloomStrength, 0, .1f, "%.3f");
                ImGui::Checkbox(u8"フィルムノイズ", &effects.grainEnabled);
                ImGui::SliderFloat(u8"ノイズの強さ", &effects.grainStrength, 0, .05f, "%.3f");
                ImGui::Separator();
                displayChanged |= Percent(u8"画面の明るさ", settings.brightness, GameSettings::MinBrightness, GameSettings::MaxBrightness);
                displayChanged |= Percent(u8"緑の強さ", settings.greenStrength, 0, 1);
                bool flashlight = context.GetLighting().IsFlashlightOn();
                if (ImGui::Checkbox(u8"懐中電灯", &flashlight)) context.GetLighting().SetFlashlightOn(flashlight);
                ImGui::Combo(u8"比較する表示", &effects.preview, u8"完成表示\0画面効果をかける前\0ライトの照射マスク\0");
                ImGui::TextWrapped(u8"画面効果前の表示にも、モデルの照明は含まれます。白い照射マスク部分がライトの中心です。");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(u8"描画・診断")) {
                ImGui::Checkbox(u8"内部描画を1080pまでに制限する", &effects.limitTo1080p);
                Percent(u8"内部描画倍率", effects.renderScale, .5f, 1);
                ImGui::TextWrapped(u8"上限OFF・倍率100%%で画面の実ピクセル数を使います。高解像度では負荷が増えます。");
                const auto size = context.GetLighting().GetSceneRenderSize();
                ImGui::Text("Window: %d x %d / Framebuffer: %d x %d", GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight());
                ImGui::Text("Scene: %.0f x %.0f / FPS: %d / Frame: %.2f ms", size.x, size.y, GetFPS(), GetFrameTime() * 1000);
                const auto dpi = GetWindowScaleDPI();
                ImGui::Text("DPI: %.2f x %.2f / Building lights: %d", dpi.x, dpi.y, context.GetLighting().GetBuildingLightCount());
                ImGui::Checkbox(u8"FPS・内部解像度をプレイ画面にも表示", &showStats_);
                ImGui::Checkbox(u8"当たり判定のワイヤーを表示", &showColliders_);
                ImGui::Checkbox(u8"テレポート・ゴール範囲を表示", &showTriggers_);
                ImGui::Separator();
                const auto player = context.GetPlayer().CaptureCheckpoint();
                ImGui::Text("Player: HP %d / Stamina %.0f / Ammo %d + %d", player.hp, player.stamina, player.ammo, player.reserveAmmo);
                ImGui::Text("Position: %.2f, %.2f, %.2f / Body: %.2f x %.2f", player.position.x, player.position.y, player.position.z,
                    context.GetPlayer().GetHeight(), context.GetPlayer().GetRadius() * 2);
                ImGui::Text(u8"敵: 生存 %d / 全体 %d", context.GetEnemyManager().AliveCount(), int(context.GetEnemyManager().GetEnemies().size()));
                if (ImGui::BeginTable("Enemies", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("ID"); ImGui::TableSetupColumn(u8"種類"); ImGui::TableSetupColumn("HP"); ImGui::TableSetupColumn(u8"状態"); ImGui::TableHeadersRow();
                    for (const auto& enemy : context.GetEnemyManager().GetEnemies()) {
                        ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(enemy.GetId()));
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(enemy.GetType() == demo::EnemyType::Monster ? "MONSTER" : "ZOMBIE");
                        ImGui::TableNextColumn(); ImGui::Text("%d", enemy.GetHp()); ImGui::TableNextColumn(); ImGui::TextUnformatted(State(enemy.GetState()));
                    }
                    ImGui::EndTable();
                }
                for (const char* key : {ResourceKeys::Model_Enemy, ResourceKeys::Model_FemaleZombie}) {
                    const Model model = RM().GetModel(key);
                    ImGui::Text("%s: %d meshes / %d bones", key, model.meshCount, model.boneCount);
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(u8"戦闘・位置")) {
                DrawGameplay(context);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        effects.Validate();
        context.GetLighting().SetPostProcessSettings(effects);
        ImGui::Separator();
        if (ImGui::Button(u8"画面効果を初期値に戻す")) context.GetLighting().SetPostProcessSettings({});
        if (ImGui::Button(u8"Debug設定を保存")) status_ = Save(context) ? u8"Debug設定を保存しました。" : u8"Debug設定を保存できませんでした。";
        ImGui::SameLine();
        if (ImGui::Button(u8"保存済みを読み込む")) status_ = Load(context) ? u8"Debug設定を読み込みました。" : u8"読み込めるDebug設定がありません。";
        ImGui::TextWrapped(u8"Debug設定はこのPCだけに保存します。明るさ・緑は通常の設定としてゲームへ戻る時に保存します。");
        if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());
        if (ImGui::Button(u8"Escメニューに戻る")) Close();
        ImGui::EndDisabled();
    }
    ImGui::End();
    firstDraw_ = false;
    return displayChanged;
}

void DebugMenu::DrawOverlays(const GameContext& context) const {
    if (showColliders_ || showTriggers_ || showEnemyRanges_ || showShotRays_) {
        BeginMode3D(context.GetCameraController().GetActiveRaylibCamera());
        if (showColliders_) {
            const auto& player = context.GetPlayer(); const Vector3 p = player.GetPosition(); const float r = player.GetRadius();
            DrawBoundingBox({{p.x-r,p.y,p.z-r},{p.x+r,p.y+player.GetHeight(),p.z+r}}, GREEN);
            DrawBoundingBox(context.GetDoor().GetBoundingBox(), YELLOW);
            for (const auto& enemy : context.GetEnemyManager().GetEnemies()) DrawBoundingBox(enemy.GetBodyBounds(), enemy.IsAlive() ? RED : GRAY);
        }
        if (showTriggers_) {
            for (const auto& point : context.GetTeleportPoints()) point.Draw();
            context.GetGoalPoint().Draw();
        }
        DrawCombatOverlays(context);
        EndMode3D();
    }
    const bool invincible=context.GetPlayer().DebugIsInvincible(), infiniteAmmo=context.GetPlayer().DebugHasInfiniteAmmo();
    if (invincible || infiniteAmmo) {
        DrawRectangle(12,82,540,28,{0,0,0,190});
        DrawText(TextFormat("DEBUG  %s  %s",invincible?"INVINCIBLE":"",infiniteAmmo?"INFINITE AMMO":""),22,88,18,ORANGE);
    }
    if (showStats_) {
        const auto size = context.GetLighting().GetSceneRenderSize();
        DrawRectangle(12, 46, 540, 30, {0,0,0,190});
        DrawText(TextFormat("DEBUG  %d FPS | %.2f ms | Scene %.0fx%.0f", GetFPS(), GetFrameTime()*1000, size.x, size.y), 22, 52, 18, LIME);
    }
}

bool DebugMenu::Save(const GameContext& context, const std::string& path) const {
    const auto& e = context.GetLighting().GetPostProcessSettings();
    return SaveData::Write(path, {{"version",1},{"blurEnabled",e.blurEnabled},{"blurMode",e.blurMode},{"blurRatio",e.blurRatio},{"blurRadius",e.blurRadius},
        {"bloomEnabled",e.bloomEnabled},{"bloomStrength",e.bloomStrength},{"grainEnabled",e.grainEnabled},{"grainStrength",e.grainStrength},
        {"limitTo1080p",e.limitTo1080p},{"renderScale",e.renderScale},{"showStats",showStats_},{"showColliders",showColliders_},{"showTriggers",showTriggers_},
        {"showEnemyRanges",showEnemyRanges_},{"showShotRays",showShotRays_},{"testPositions",context.GetDebugGameplay().SavePositions()}});
}

bool DebugMenu::Load(GameContext& context, const std::string& path) {
    nlohmann::json data;
    if (!SaveData::Read(path, data) || !data.contains("version") || !data["version"].is_number_integer() || data["version"] != 1) return false;
    PostProcessSettings e;
    e.blurEnabled = Flag(data,"blurEnabled",e.blurEnabled); e.blurMode = int(std::clamp(Number(data,"blurMode",0),0.f,2.f));
    e.blurRatio = Number(data,"blurRatio",e.blurRatio); e.blurRadius = Number(data,"blurRadius",e.blurRadius);
    e.bloomEnabled = Flag(data,"bloomEnabled",e.bloomEnabled); e.bloomStrength = Number(data,"bloomStrength",e.bloomStrength);
    e.grainEnabled = Flag(data,"grainEnabled",e.grainEnabled); e.grainStrength = Number(data,"grainStrength",e.grainStrength);
    e.limitTo1080p = Flag(data,"limitTo1080p",e.limitTo1080p); e.renderScale = Number(data,"renderScale",e.renderScale);
    context.GetLighting().SetPostProcessSettings(e); // 比較用のマスク表示は起動時に戻す。
    showStats_ = Flag(data,"showStats",false); showColliders_ = Flag(data,"showColliders",false); showTriggers_ = Flag(data,"showTriggers",false);
    showEnemyRanges_=Flag(data,"showEnemyRanges",false);showShotRays_=Flag(data,"showShotRays",false);
    if(data.contains("testPositions"))context.GetDebugGameplay().LoadPositions(data["testPositions"]);
    return true;
}
void DebugMenu::DrawGameplay(GameContext& context) {
    auto& player=context.GetPlayer();auto& debug=context.GetDebugGameplay();
    bool invincible=player.DebugIsInvincible(),infiniteAmmo=player.DebugHasInfiniteAmmo();
    if(ImGui::Checkbox(u8"無敵",&invincible))player.DebugSetInvincible(invincible);
    ImGui::SameLine();
    if(ImGui::Checkbox(u8"無限弾",&infiniteAmmo))player.DebugSetInfiniteAmmo(infiniteAmmo);
    if(ImGui::Button(u8"HP・スタミナを全回復"))player.DebugRefillVitals();
    ImGui::SameLine();if(ImGui::Button(u8"マガジン・予備弾を補充"))player.DebugRefillAmmo();
    ImGui::TextWrapped(u8"無敵・無限弾は保存しません。新規開始・チェックポイント読み込み時はOFFに戻ります。");
    ImGui::Separator();
    ImGui::Checkbox(u8"敵の範囲・視線・移動方向を表示",&showEnemyRanges_);
    ImGui::TextWrapped(u8"水色:索敵 / 青:追跡をやめる距離 / 赤:攻撃。紫:向き / 水色の矢印:移動要求 / 緑または橙:実移動。矢印は方向を拡大表示します。");
    ImGui::TextWrapped(u8"壁・ドアの向こうでは発見しません。緑の視線は視認中、灰色は未視認。黄色の球は最後に見た位置です。見失うと最大3秒だけ捜索し、進めない状態が続くと停止します。");
    const auto target=demo::TargetInfo::FromPlayer(player);const auto door=context.GetDoor().GetBoundingBox();
    for(const auto& enemy:context.GetEnemyManager().GetEnemies()) {
        ImGui::PushID(int(enemy.GetId()));
        if(ImGui::TreeNode("Enemy", "%s #%llu - %s",enemy.GetType()==demo::EnemyType::Monster?"MONSTER":"ZOMBIE",static_cast<unsigned long long>(enemy.GetId()),State(enemy.GetState()))) {
            const auto info=enemy.GetDebugInfo(target,context.GetStage(),&door);
            ImGui::Text(u8"距離 %.2f / 索敵 %.2f / 攻撃 %.2f",info.distance,info.config.detectionRange,info.config.attackRange);
            ImGui::Text(u8"視認 %s / 捜索残り %.1f秒 / 行き詰まり %s",info.targetVisible?"YES":"NO",info.searchTime,info.movementBlocked?"YES":"NO");
            if(info.hasLastSeen)ImGui::Text(u8"最後に見た位置: %.2f, %.2f, %.2f",info.lastSeenPosition.x,info.lastSeenPosition.y,info.lastSeenPosition.z);
            ImGui::TextWrapped(u8"攻撃の位置条件: %s",AttackReason(info.reason));
            ImGui::Text(u8"威力 %d / クールダウン %.2f秒 / 被弾反応 %s",info.config.attackDamage,info.cooldown,info.hitReaction?"ON":"OFF");
            if(info.config.attackDamage==0)ImGui::TextUnformatted(u8"威力0のため攻撃が無効です。");
            ImGui::Text(u8"遮蔽物: ステージ %s / ドア %s / 移動制限 %s",info.wallBlocked?"YES":"NO",info.doorBlocked?"YES":"NO",info.movementLimited?"YES":"NO");
            if(enemy.GetState()==demo::EnemyState::Attack)ImGui::ProgressBar(info.attackProgress,{-1,0},u8"攻撃アニメーション");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::Checkbox(u8"射線・命中点を表示（直近8発）",&showShotRays_);
    ImGui::SameLine();if(ImGui::Button(u8"射撃履歴を消去"))debug.ClearShots();
    ImGui::TextWrapped(u8"判定に使ったカメラからの線です。赤:敵 / 橙:壁 / 灰:空振り。最新32発を記録します。");
    if(ImGui::BeginTable("Shots",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,{0,150})) {
        for(const char* title:{u8"発射",u8"命中",u8"敵ID",u8"実ダメージ",u8"残HP"})ImGui::TableSetupColumn(title);
        ImGui::TableHeadersRow();
        for(auto it=debug.GetShots().rbegin();it!=debug.GetShots().rend();++it) {
            const auto& shot=*it;ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Text("%llu",static_cast<unsigned long long>(shot.number));
            ImGui::TableNextColumn();ImGui::TextUnformatted(ShotTarget(shot.target));ImGui::TableNextColumn();
            if(shot.target==DebugShotTarget::Enemy)ImGui::Text("%llu",static_cast<unsigned long long>(shot.enemyId));else ImGui::TextUnformatted("-");
            ImGui::TableNextColumn();ImGui::Text("%d",shot.damage);ImGui::TableNextColumn();
            if(shot.target==DebugShotTarget::Enemy)ImGui::Text("%d%s",shot.hpAfter,shot.killed?" (DEAD)":"");else ImGui::TextUnformatted("-");
        }
        ImGui::EndTable();
    }
    if(!debug.GetShots().empty()) {const auto& shot=debug.GetShots().back();ImGui::Text(u8"最新 t=%.3f秒 / 命中・終点: %.2f, %.2f, %.2f",shot.time,shot.end.x,shot.end.y,shot.end.z);}
    ImGui::Separator();
    const char* slots[]={u8"位置1",u8"位置2",u8"位置3",u8"位置4",u8"位置5"};
    ImGui::Combo(u8"テスト位置",&positionSlot_,slots,DebugGameplay::PositionCount);
    ImGui::Checkbox(u8"移動先を床に合わせる",&snapToFloor_);
    if(ImGui::Button(u8"現在位置・向きを登録"))status_=debug.StorePosition(context,positionSlot_)?u8"テスト位置を登録しました。Debug設定の保存で残せます。":u8"位置を登録できませんでした。";
    const auto& position=debug.GetPositions()[positionSlot_];
    ImGui::BeginDisabled(!position.valid);ImGui::SameLine();
    if(ImGui::Button(u8"登録位置へ移動"))status_=debug.RecallPosition(context,positionSlot_,snapToFloor_)?u8"登録位置へ移動しました。":u8"ステージ配置が異なるか、移動できない状態です。";
    ImGui::SameLine();if(ImGui::Button(u8"登録を消去"))debug.ClearPosition(positionSlot_);ImGui::EndDisabled();
    if(position.valid)ImGui::Text("XYZ %.2f, %.2f, %.2f / yaw %.2f / pitch %.2f",position.position.x,position.position.y,position.position.z,position.yaw,position.pitch);
    ImGui::TextWrapped(u8"位置と向きだけを移動します。HP・所持品・敵の状態はそのままです。ステージの移動や拡縮後は再登録してください。");
}

void DebugMenu::DrawCombatOverlays(const GameContext& context) const {
    if(showEnemyRanges_) {
        const auto target=demo::TargetInfo::FromPlayer(context.GetPlayer());const auto door=context.GetDoor().GetBoundingBox();
        for(const auto& enemy:context.GetEnemyManager().GetEnemies()) {
            const auto info=enemy.GetDebugInfo(target,context.GetStage(),&door);const auto p=Vector3Add(enemy.GetPosition(),{0,.05f,0});
            DrawCircle3D(p,info.config.detectionRange,{1,0,0},90,SKYBLUE);DrawCircle3D(p,info.config.loseRange,{1,0,0},90,DARKBLUE);DrawCircle3D(p,info.config.attackRange,{1,0,0},90,RED);
            Direction(info.origin,{std::sin(enemy.GetYaw()),0,-std::cos(enemy.GetYaw())},PURPLE);
            Direction(p,info.desiredMove,SKYBLUE);Direction(Vector3Add(p,{0,.12f,0}),info.actualMove,info.movementLimited?ORANGE:LIME);
            if(enemy.IsAlive()) {
                const auto head=Vector3Add(target.position,{0,context.GetPlayer().GetHeight()*.9f,0});
                DrawLine3D(info.sightOrigin,head,info.targetVisible?LIME:GRAY);
                if(info.hasLastSeen) {
                    const auto last=Vector3Add(info.lastSeenPosition,{0,.15f,0});
                    DrawSphereWires(last,.15f,8,8,YELLOW);DrawLine3D(p,last,YELLOW);
                }
            }
            if(enemy.IsAlive()) {DrawLine3D(info.origin,info.aim,info.canHit?LIME:ORANGE);if(info.wallBlocked||info.doorBlocked)DrawSphereWires(info.obstruction,.08f,6,6,ORANGE);}
        }
    }
    if(showShotRays_) {
        int count=0;const auto& shots=context.GetDebugGameplay().GetShots();
        for(auto it=shots.rbegin();it!=shots.rend()&&count<8;++it,++count) {
            const auto color=it->target==DebugShotTarget::Enemy?RED:it->target==DebugShotTarget::Wall?ORANGE:GRAY;
            DrawLine3D(it->ray.position,it->end,color);
            if(it->target!=DebugShotTarget::Miss) {DrawSphereWires(it->end,.065f,6,6,color);DrawLine3D(it->end,Vector3Add(it->end,Vector3Scale(it->normal,.3f)),color);}
        }
    }
}
#endif
