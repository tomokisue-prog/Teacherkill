#pragma once
#ifndef NDEBUG
#include <string>
class GameContext;
struct GameSettings;

// Escメニューから開く開発用パネル。ゲームの進行や配置データは保存しない。
class DebugMenu {
public:
    static void LoadFonts(); // rlImGuiSetupのフォントコールバック。
    void Initialize(GameContext& context);
    void Open();
    void Close();
    bool IsOpen() const;
    bool Draw(GameContext& context, GameSettings& settings); // 通常の表示設定を変更したらtrue。
    void DrawOverlays(const GameContext& context) const;
    bool Load(GameContext& context, const std::string& path = "Data/Save/debug_graphics.json");
    bool Save(const GameContext& context, const std::string& path = "Data/Save/debug_graphics.json") const;
private:
    bool open_ = false;
    bool firstDraw_ = false;
    bool initialized_ = false;
    bool showStats_ = false;
    bool showColliders_ = false;
    bool showTriggers_ = false;
    bool showEnemyRanges_ = false;
    bool showShotRays_ = false;
    int positionSlot_ = 0;
    bool snapToFloor_ = true;
    void DrawGameplay(GameContext& context);
    void DrawCombatOverlays(const GameContext& context) const;
    std::string status_;
};
#endif
