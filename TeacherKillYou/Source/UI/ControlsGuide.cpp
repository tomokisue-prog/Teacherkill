#include "ControlsGuide.h"
#include "AudioManager.h"
#include "ResourceManager.h"
#include <algorithm>

namespace {
    struct Control {
        const char* key;
        const char* japanese;
        const char* english;
    };

    constexpr Control Controls[] = {
        {"W / A / S / D", u8"移動", "Move"},
        {"Mouse", u8"視点を動かす", "Look around"},
        {"Left Shift + WASD", u8"ダッシュ", "Sprint"},
        {"Space", u8"ジャンプ", "Jump"},
        {"E", u8"調べる・扉や設備を使う", "Inspect or use doors and fixtures"},
        {"F", u8"アイテムを拾う", "Pick up items"},
        {"Left Click", u8"射撃（銃を入手した後）", "Shoot (after picking up the gun)"},
        {"R", u8"リロード", "Reload"},
        {"1 / 2 / 3 / 4", u8"アイテムスロットを選ぶ", "Select an item slot"},
        {"Hold E", u8"選択中の回復アイテムを使う", "Use the selected medkit"},
        {"Space / Left Click", u8"会話を進める", "Advance dialogue"},
        {"Esc", u8"ポーズメニューを開く", "Open the pause menu"},
    };

    struct Layout {
        Rectangle panel;
        Rectangle back;
        float scale;
    };

    Layout GetLayout() {
        const float w = static_cast<float>(GetScreenWidth());
        const float h = static_cast<float>(GetScreenHeight());
        const float scale = (std::min)(w / 1600.0f, h / 900.0f);
        const float width = 820 * scale;
        const float height = 740 * scale;
        const Rectangle panel{(w - width) * .5f, (h - height) * .5f, width, height};
        const Rectangle back{(w - 220 * scale) * .5f, panel.y + 662 * scale, 220 * scale, 48 * scale};
        return {panel, back, scale};
    }

    void DrawCenteredText(const char* japanese, const char* english, float centerX, float y, float size) {
        const auto& font = JPFont();
        if (font.IsLoaded()) {
            const float width = font.MeasureText(japanese, size).x;
            font.DrawText(japanese, int(centerX - width * .5f), int(y), size, RAYWHITE);
        } else {
            const int pixels = (std::max)(1, int(size));
            DrawText(english, int(centerX - MeasureText(english, pixels) * .5f), int(y), pixels, RAYWHITE);
        }
    }
}

void ControlsGuide::Update()
{
    if (IsKeyPressed(KEY_ESCAPE) ||
        (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), GetLayout().back))) {
        AM().PlayUiSe(SeId::Confirm);
        Close();
    }
}

void ControlsGuide::Draw() const
{
    const auto [panel, back, s] = GetLayout();
    const float centerX = panel.x + panel.width * .5f;
    const Color border{107, 126, 116, 255};
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, 200});
    DrawRectangleRec(panel, {14, 21, 24, 255});
    DrawRectangleLinesEx(panel, (std::max)(1.0f, 2 * s), border);
    DrawCenteredText(u8"操作説明", "CONTROLS", centerX, panel.y + 27 * s, 40 * s);

    const auto& font = JPFont();
    const float rowX = panel.x + 34 * s;
    float rowY = panel.y + 96 * s;
    const float actionX = rowX + 293 * s;
    const int pixels = (std::max)(1, int(24 * s));
    for (const auto& control : Controls) {
        DrawRectangleRec({rowX, rowY, panel.width - 68 * s, 40 * s}, {23, 34, 36, 255});
        const int y = int(rowY + 7 * s);
        DrawText(control.key, int(rowX + 16 * s), y, pixels, {173, 226, 194, 255});
        if (font.IsLoaded())
            font.DrawText(control.japanese, int(actionX), y, 24 * s, RAYWHITE);
        else
            DrawText(control.english, int(actionX), y, pixels, RAYWHITE);
        rowY += 46 * s;
    }

    const bool hovered = CheckCollisionPointRec(GetMousePosition(), back);
    DrawRectangleRec(back, hovered ? Color{46, 73, 63, 255} : Color{25, 39, 39, 255});
    DrawRectangleLinesEx(back, (std::max)(1.0f, 2 * s), hovered ? Color{173, 226, 194, 255} : border);
    DrawCenteredText(u8"戻る  [Esc]", "BACK  [Esc]", centerX, back.y + 10 * s, 24 * s);
}
