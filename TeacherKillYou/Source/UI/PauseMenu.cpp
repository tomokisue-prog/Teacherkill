#include "PauseMenu.h"
#include "GameSettings.h"
#include "ResourceManager.h"
#include "raymath.h"
#include <algorithm>

namespace {
    constexpr int SettingCount = 6;
    float Scale() { return Clamp((std::min)(GetScreenWidth() / 1920.0f, GetScreenHeight() / 1080.0f), .45f, 1.5f); }
    Rectangle Button(int row) {
        const float s = Scale();
        return {(GetScreenWidth() - 600 * s) / 2, GetScreenHeight() * .3f + row * 76 * s, 600 * s, 60 * s};
    }
    Rectangle Slider(int row) {
        const float s = Scale();
        return {(GetScreenWidth() - 500 * s) / 2, GetScreenHeight() * .31f + (row * 90 + 38) * s, 500 * s, 18 * s};
    }
    void Text(const char* japanese, const char* english, float y, float size, Color color) {
        const auto& font = JPFont();
        if (font.IsLoaded())
            font.DrawText(japanese, int((GetScreenWidth() - font.MeasureText(japanese, size).x) / 2), int(y), size, color);
        else {
            const int pixels = (std::max)(10, int(size));
            DrawText(english, (GetScreenWidth() - MeasureText(english, pixels)) / 2, int(y), pixels, color);
        }
    }
    void DrawButton(Rectangle r, const char* jp, const char* en, bool selected, bool enabled) {
        DrawRectangleRec(r, selected && enabled ? Color{37, 64, 56, 250} : Color{15, 21, 24, 250});
        DrawRectangleLinesEx(r, (std::max)(1.0f, 2 * Scale()), enabled && selected ? Color{138, 191, 163, 255} : Color{57, 68, 68, 255});
        Text(jp, en, r.y + 14 * Scale(), 29 * Scale(), enabled ? RAYWHITE : Color{91, 99, 100, 255});
    }
    float& Setting(GameSettings& settings, int row) {
        switch (row) {
        case 0: return settings.bgmVolume;
        case 1: return settings.ambienceVolume;
        case 2: return settings.seVolume;
        case 3: return settings.mouseSensitivity;
        case 4: return settings.brightness;
        default: return settings.greenStrength;
        }
    }
    Vector2 SettingRange(int row) {
        if (row == 3) return {.1f, 3.0f};
        if (row == 4) return {GameSettings::MinBrightness, GameSettings::MaxBrightness};
        return {0, 1};
    }
}

void PauseMenu::Open()
{
    settingsPage_ = false;
    selectedOption_ = 0;
    draggingSlider_ = -1;
    elapsed_ = 0;
    message_.clear();
    fallbackMessage_.clear();
}

void PauseMenu::SetMessage(const std::string& japanese, const std::string& english)
{
    message_ = japanese;
    fallbackMessage_ = english;
}

PauseAction PauseMenu::Update(float dt, GameSettings& settings, bool canLoad, bool canSave)
{
    elapsed_ += (std::max)(0.0f, dt);
    if (elapsed_ < .15f) return PauseAction::None;
    const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool mouseMoved = Vector2LengthSqr(GetMouseDelta()) > 0 || click;
    const auto mouse = GetMousePosition();
    const int optionCount = settingsPage_ ? SettingCount + 1 : 5;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selectedOption_ = (selectedOption_ + optionCount - 1) % optionCount;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selectedOption_ = (selectedOption_ + 1) % optionCount;

    if (settingsPage_) {
        if (IsKeyPressed(KEY_ESCAPE)) { settingsPage_ = false; selectedOption_ = 1; return PauseAction::SaveSettings; }
        // 新しいクリックではドラッグ対象を選び直し、前のスライダーを動かさない。
        if (click) draggingSlider_ = -1;
        bool changed = false;
        for (int i = 0; i < SettingCount; ++i) {
            auto bar = Slider(i);
            auto hit = Rectangle{bar.x - 12 * Scale(), bar.y - 12 * Scale(), bar.width + 24 * Scale(), bar.height + 24 * Scale()};
            if (mouseMoved && CheckCollisionPointRec(mouse, hit)) selectedOption_ = i;
            if (click && CheckCollisionPointRec(mouse, hit)) draggingSlider_ = i;
            if (draggingSlider_ == i && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                const Vector2 range = SettingRange(i);
                float& value = Setting(settings, i);
                const float next = Lerp(range.x, range.y, Clamp((mouse.x - bar.x) / bar.width, 0, 1));
                changed |= value != next;
                value = next;
            }
        }
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) draggingSlider_ = -1;
        if (selectedOption_ < SettingCount && (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT))) {
            float& value = Setting(settings, selectedOption_);
            const float step = selectedOption_ == 3 ? .1f : .05f;
            const Vector2 range = SettingRange(selectedOption_);
            value = Clamp(value + (IsKeyPressed(KEY_RIGHT) ? step : -step), range.x, range.y);
            changed = true;
        }
        const auto back = Button(7);
        const bool overBack = CheckCollisionPointRec(mouse, back);
        if (mouseMoved && overBack) selectedOption_ = SettingCount;
        if ((click && overBack) || (selectedOption_ == SettingCount && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))) {
            settingsPage_ = false; selectedOption_ = 1; return PauseAction::SaveSettings;
        }
        return changed ? PauseAction::SettingsChanged : PauseAction::None;
    }

    if (IsKeyPressed(KEY_ESCAPE)) return PauseAction::Resume;
    bool clickedOption = false;
    for (int i = 0; i < optionCount; ++i) {
        if (CheckCollisionPointRec(mouse, Button(i))) {
            if (mouseMoved) selectedOption_ = i;
            clickedOption = click;
        }
    }
    if (!(clickedOption || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) return PauseAction::None;
    switch (selectedOption_) {
    case 0: return PauseAction::Resume;
    case 1: settingsPage_ = true; selectedOption_ = 0; return PauseAction::None;
    case 2: return canSave ? PauseAction::SaveCheckpoint : PauseAction::None;
    case 3: return canLoad ? PauseAction::LoadCheckpoint : PauseAction::None;
    case 4: return PauseAction::Title;
    }
    return PauseAction::None;
}

void PauseMenu::Draw(const GameSettings& settings, bool canLoad, bool canSave) const
{
    const float s = Scale();
    // 設定中は背景を確認できる明るさにし、文字の後ろだけパネルを重ねる。
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {3, 9, 11, static_cast<unsigned char>(settingsPage_ ? 24 : 225)});
    if (settingsPage_) {
        const float top = GetScreenHeight() * .28f;
        DrawRectangleRec({(GetScreenWidth() - 760 * s) / 2, top, 760 * s, Button(7).y + Button(7).height + 24 * s - top}, {3, 9, 11, 210});
    }
    Text(settingsPage_ ? u8"設定" : u8"ポーズ", settingsPage_ ? "SETTINGS" : "PAUSED", GetScreenHeight() * .16f, 64 * s, RAYWHITE);
    if (settingsPage_) {
        const char* jp[] = {u8"BGM音量", u8"環境音量", u8"効果音量", u8"マウス感度", u8"画面の明るさ", u8"緑の強さ"};
        const char* en[] = {"BGM VOLUME", "AMBIENCE VOLUME", "SOUND EFFECTS", "MOUSE SENSITIVITY", "DISPLAY BRIGHTNESS", "GREEN STRENGTH"};
        const float values[] = {settings.bgmVolume, settings.ambienceVolume, settings.seVolume, settings.mouseSensitivity, settings.brightness, settings.greenStrength};
        Text(u8"背景を見ながら調整できます。明るさ・緑の強さは100%が標準です。", "Preview the background. Brightness and green default to 100%.", GetScreenHeight() * .24f, 20 * s, {192, 212, 198, 255});
        for (int i = 0; i < SettingCount; ++i) {
            const auto bar = Slider(i);
            const Vector2 range = SettingRange(i);
            const float fraction = Clamp((values[i] - range.x) / (range.y - range.x), 0, 1);
            Text(jp[i], en[i], bar.y - 36 * s, 25 * s, i == selectedOption_ ? Color{173, 226, 194, 255} : RAYWHITE);
            DrawRectangleRec(bar, {42, 49, 51, 255});
            DrawRectangleRec({bar.x, bar.y, bar.width * fraction, bar.height}, {96, 153, 126, 255});
            DrawCircleV({bar.x + bar.width * fraction, bar.y + bar.height / 2}, 13 * s, RAYWHITE);
            const char* number = i == 3 ? TextFormat("%.2fx", values[i]) : TextFormat("%d%%", int(values[i] * 100 + .5f));
            DrawText(number, int(bar.x + bar.width + 24 * s), int(bar.y - 3 * s), int(22 * s), RAYWHITE);
        }
        DrawButton(Button(7), u8"保存して戻る", "SAVE AND BACK", selectedOption_ == SettingCount, true);
    }
    else {
        const char* jp[] = {u8"ゲームに戻る  [Esc]", u8"画面・音量・マウス設定", u8"チェックポイントを保存", u8"チェックポイントから再開", u8"タイトルへ戻る"};
        const char* en[] = {"RESUME  [Esc]", "DISPLAY / AUDIO / MOUSE", "SAVE CHECKPOINT", "LOAD CHECKPOINT", "RETURN TO TITLE"};
        for (int i = 0; i < 5; ++i)
            DrawButton(Button(i), jp[i], en[i], selectedOption_ == i, i == 2 ? canSave : i == 3 ? canLoad : true);
        Text(u8"保存すると、前のチェックポイントを更新します。", "Saving replaces the previous checkpoint.", Button(4).y + 87 * s, 20 * s, {159, 176, 172, 255});
    }
    if (!message_.empty()) Text(message_.c_str(), fallbackMessage_.c_str(), GetScreenHeight() * .91f, 22 * s, {192, 212, 184, 255});
}
