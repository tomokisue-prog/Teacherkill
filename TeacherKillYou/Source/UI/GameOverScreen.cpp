#include "GameOverScreen.h"
#include "ResourceManager.h"
#include "raymath.h"
#include <algorithm>

namespace {
    constexpr float GameOverInputDelay = 0.35f;

    struct GameOverLayout {
        float scale;
        Rectangle retry;
        Rectangle title;
    };

    GameOverLayout MenuLayout() {
        const float scale = Clamp((std::min)(GetScreenWidth() / 1920.0f, GetScreenHeight() / 1080.0f), 0.45f, 1.5f);
        const float width = 420.0f * scale;
        const float height = 64.0f * scale;
        const float x = (GetScreenWidth() - width) * 0.5f;
        const float y = GetScreenHeight() * 0.52f;
        return {scale, {x, y, width, height}, {x, y + height + 20.0f * scale, width, height}};
    }

    void DrawCenteredText(const char* japanese, const char* fallback, float y, float size, Color color) {
        const auto& font = JPFont();
        if (font.IsLoaded()) {
            const float width = font.MeasureText(japanese, size).x;
            font.DrawText(japanese, static_cast<int>((GetScreenWidth() - width) * 0.5f), static_cast<int>(y), size, color);
        }
        else {
            const int fontSize = (std::max)(10, static_cast<int>(size));
            DrawText(fallback, (GetScreenWidth() - MeasureText(fallback, fontSize)) / 2, static_cast<int>(y), fontSize, color);
        }
    }
}

void GameOverScreen::Reset()
{
    elapsed_ = 0.0f;
    selectedOption_ = 0;
}

GameOverAction GameOverScreen::Update(float deltaTime)
{
    elapsed_ += (std::max)(deltaTime, 0.0f);
    // 致命傷と同じクリックやキー入力で、直ちに再開してしまうことを防ぐ。
    if (elapsed_ < GameOverInputDelay) return GameOverAction::None;

    const auto layout = MenuLayout();
    const Vector2 mouse = GetMousePosition();
    const bool overRetry = CheckCollisionPointRec(mouse, layout.retry);
    const bool overTitle = CheckCollisionPointRec(mouse, layout.title);
    const Rectangle checkpoint{layout.title.x, layout.title.y + layout.title.height + 20 * layout.scale, layout.title.width, layout.title.height};
    const bool overCheckpoint = checkpointAvailable_ && CheckCollisionPointRec(mouse, checkpoint);
    const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    // マウスを止めている間は、キーボードで選択した項目を上書きしない。
    if (Vector2LengthSqr(GetMouseDelta()) > 0.0f || clicked) {
        if (overRetry) selectedOption_ = 0;
        else if (overTitle) selectedOption_ = 1;
        else if (overCheckpoint) selectedOption_ = 2;
    }
    const int count = checkpointAvailable_ ? 3 : 2;
    if (selectedOption_ >= count) selectedOption_ = 0;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selectedOption_ = (selectedOption_ + count - 1) % count;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selectedOption_ = (selectedOption_ + 1) % count;
    if (checkpointAvailable_ && IsKeyPressed(KEY_C)) return GameOverAction::Checkpoint;

    if (IsKeyPressed(KEY_R))
        return GameOverAction::Retry;
    else if (IsKeyPressed(KEY_B))
        return GameOverAction::Title;
    else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || (clicked && (overRetry || overTitle || overCheckpoint)))
        return selectedOption_ == 0 ? GameOverAction::Retry : selectedOption_ == 1 ? GameOverAction::Title : GameOverAction::Checkpoint;
    return GameOverAction::None;
}

void GameOverScreen::Draw() const
{
    const auto layout = MenuLayout();
    const float fade = Clamp(elapsed_ / 0.6f, 0.0f, 1.0f);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.62f + 0.24f * fade));
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), {30, 0, 4, 0}, {45, 3, 10, 100});
    DrawCenteredText("GAME OVER", "GAME OVER", GetScreenHeight() * 0.27f, 88.0f * layout.scale, {205, 67, 76, 255});
    DrawCenteredText(u8"ここで力尽きました。", "You were defeated.", GetScreenHeight() * 0.39f, 26.0f * layout.scale, {185, 181, 183, 255});

    const Rectangle buttons[] = {layout.retry, layout.title, {layout.title.x, layout.title.y + layout.title.height + 20 * layout.scale, layout.title.width, layout.title.height}};
    const char* labels[] = {u8"リトライ  [R]", u8"タイトルへ戻る  [B]", u8"チェックポイントから再開  [C]"};
    const char* fallbacks[] = {"RETRY  [R]", "RETURN TO TITLE  [B]", "CONTINUE CHECKPOINT  [C]"};
    for (int i = 0; i < (checkpointAvailable_ ? 3 : 2); ++i) {
        const bool selected = i == selectedOption_;
        DrawRectangleRec(buttons[i], selected ? Color{76, 22, 29, 235} : Color{14, 17, 20, 235});
        DrawRectangleLinesEx(buttons[i], (std::max)(1.0f, 2.0f * layout.scale),
            selected ? Color{180, 68, 79, 255} : Color{68, 71, 76, 255});
        DrawCenteredText(labels[i], fallbacks[i], buttons[i].y + (buttons[i].height - 28.0f * layout.scale) * 0.5f,
            28.0f * layout.scale, selected ? RAYWHITE : Color{176, 179, 183, 255});
    }
    DrawCenteredText(u8"上下キーで選択・Enterで決定 / マウス操作も可能", "UP/DOWN: SELECT   ENTER: CONFIRM   MOUSE: CLICK",
        buttons[checkpointAvailable_ ? 2 : 1].y + layout.title.height + 32.0f * layout.scale, 19.0f * layout.scale, {141, 143, 149, 255});
}


void GameOverScreen::SetCheckpointAvailable(bool available) { checkpointAvailable_ = available; }
