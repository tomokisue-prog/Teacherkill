#include "GameOverScreen.h"
#include "ResourceManager.h"
#include "raymath.h"
#include <algorithm>

namespace
{
    constexpr const char* GameOverImagePath = "Data/Image/UI/Gamelose.png";
    constexpr float GameOverFadeSeconds = 0.6f;

    // Button artwork positions in Mitsuki's 1888 x 833 GameOver.png.
    // The same rectangles are used for highlighting and mouse clicks.
    constexpr float ArtworkWidth = 1888.0f;
    constexpr float ArtworkHeight = 833.0f;
    constexpr Rectangle RetryArtwork{ 724.0f, 550.0f, 475.0f, 116.0f };
    constexpr Rectangle TitleArtwork{ 724.0f, 688.0f, 475.0f, 111.0f };

    struct GameOverLayout
    {
        float scale;
        Rectangle canvas;
        Rectangle imageArea;
        Rectangle retry;
        Rectangle title;
    };

    Rectangle ScreenCanvas()
    {
        const float screenWidth = static_cast<float>((std::max)(GetScreenWidth(), 1));
        const float screenHeight = static_cast<float>((std::max)(GetScreenHeight(), 1));
        const float scale = (std::min)(screenWidth / 1920.0f, screenHeight / 1080.0f);
        const float width = 1920.0f * scale;
        const float height = 1080.0f * scale;
        return { (screenWidth - width) * 0.5f, (screenHeight - height) * 0.5f, width, height };
    }

    Rectangle ImageArea(const Texture2D& image, Rectangle canvas)
    {
        if (image.id == 0 || image.width <= 0 || image.height <= 0)
            return canvas;

        const float scale = (std::max)(canvas.width / image.width, canvas.height / image.height);
        const float width = image.width * scale;
        const float height = image.height * scale;
        return { canvas.x + (canvas.width - width) * 0.5f,
                 canvas.y + (canvas.height - height) * 0.5f, width, height };
    }

    GameOverLayout MenuLayout(const Texture2D& image)
    {
        const Rectangle canvas = ScreenCanvas();
        const float scale = (std::max)(0.01f, (std::min)(canvas.width / 1920.0f, canvas.height / 1080.0f));

        if (image.id != 0 && image.width > 0 && image.height > 0)
        {
            const Rectangle viewport{ 0.0f, 0.0f, static_cast<float>(GetScreenWidth()),
                                      static_cast<float>(GetScreenHeight()) };
            const Rectangle area = ImageArea(image, viewport);
            const float sx = area.width / ArtworkWidth;
            const float sy = area.height / ArtworkHeight;
            const Rectangle retry{ area.x + RetryArtwork.x * sx, area.y + RetryArtwork.y * sy,
                                   RetryArtwork.width * sx, RetryArtwork.height * sy };
            const Rectangle title{ area.x + TitleArtwork.x * sx, area.y + TitleArtwork.y * sy,
                                   TitleArtwork.width * sx, TitleArtwork.height * sy };
            return { scale, canvas, area, retry, title };
        }

        // A readable two-button menu if the PNG cannot be loaded.
        const float width = (std::min)(420.0f * scale, canvas.width * 0.85f); // CHANGE: width
        const float height = 64.0f * scale; // CHANGE: height
        const float gap = 20.0f * scale; // CHANGE: space between buttons
        const float x = canvas.x + (canvas.width - width) * 0.5f;
        const float y = canvas.y + canvas.height * 0.62f;
        return { scale, canvas, canvas, { x, y, width, height }, { x, y + height + gap, width, height } };
    }

    void DrawCenteredText(const char* japanese, const char* fallback,
                          float centerX, float y, float size, Color color)
    {
        const auto& font = JPFont();
        if (font.IsLoaded())
        {
            const float width = font.MeasureText(japanese, size).x;
            font.DrawText(japanese, static_cast<int>(centerX - width * 0.5f),
                          static_cast<int>(y), size, color);
        }
        else
        {
            const int fontSize = (std::max)(10, static_cast<int>(size));
            DrawText(fallback, static_cast<int>(centerX - MeasureText(fallback, fontSize) * 0.5f),
                     static_cast<int>(y), fontSize, color);
        }
    }
}

void GameOverScreen::Init()
{
    End();

    if (FileExists(GameOverImagePath))
    {
        image_ = LoadTexture(GameOverImagePath);
        if (image_.id != 0) SetTextureFilter(image_, TEXTURE_FILTER_BILINEAR);
    }

    if (image_.id == 0)
        TraceLog(LOG_WARNING, "GAME OVER: Could not load %s; using the text fallback.", GameOverImagePath);
}

void GameOverScreen::End()
{
    if (image_.id != 0) UnloadTexture(image_);
    image_ = {};
    checkpointAvailable_ = false;
    Reset();
}

void GameOverScreen::Reset()
{
    elapsed_ = 0.0f;
    selectedOption_ = 0;
}

void GameOverScreen::SetCheckpointAvailable(bool available)
{
    checkpointAvailable_ = available;
}

GameOverAction GameOverScreen::Update(float deltaTime)
{
    elapsed_ += (std::max)(deltaTime, 0.0f);

    // Ignore the death-causing input while the screen fades in.
    if (elapsed_ < GameOverFadeSeconds) return GameOverAction::None;

    const GameOverLayout layout = MenuLayout(image_);
    const Vector2 mouse = GetMousePosition();
    const bool overRetry = CheckCollisionPointRec(mouse, layout.retry);
    const bool overTitle = CheckCollisionPointRec(mouse, layout.title);
    const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    // A stationary mouse must not override keyboard selection.
    if (Vector2LengthSqr(GetMouseDelta()) > 0.0f || clicked)
    {
        if (overRetry) selectedOption_ = 0;
        else if (overTitle) selectedOption_ = 1;
    }

    // A click always activates the button under the pointer, even if a key
    // was pressed during the same frame.
    if (clicked && overRetry) return GameOverAction::Retry;
    if (clicked && overTitle) return GameOverAction::Title;

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selectedOption_ = (selectedOption_ + 1) % 2;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selectedOption_ = (selectedOption_ + 1) % 2;

    // Keep the existing checkpoint feature as a shortcut, with two main buttons.
    if (checkpointAvailable_ && IsKeyPressed(KEY_C)) return GameOverAction::Checkpoint;
    if (IsKeyPressed(KEY_R)) return GameOverAction::Retry;
    if (IsKeyPressed(KEY_B)) return GameOverAction::Title;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        return selectedOption_ == 0 ? GameOverAction::Retry : GameOverAction::Title;

    return GameOverAction::None;
}

void GameOverScreen::Draw() const
{
    const GameOverLayout layout = MenuLayout(image_);
    const float fade = Clamp(elapsed_ / GameOverFadeSeconds, 0.0f, 1.0f);
    const float centerX = layout.canvas.x + layout.canvas.width * 0.5f;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f + 0.35f * fade));
    const bool hasImage = image_.id != 0 && image_.width > 0 && image_.height > 0;
    if (hasImage)
    {
        const Rectangle source{ 0.0f, 0.0f, static_cast<float>(image_.width), static_cast<float>(image_.height) };
        DrawTexturePro(image_, source, layout.imageArea, { 0.0f, 0.0f }, 0.0f, Fade(WHITE, fade));
        
        const Rectangle selected = selectedOption_ == 0 ? layout.retry : layout.title;
        const float lineWidth = (std::max)(1.0f, 2.0f * layout.imageArea.width / ArtworkWidth);
        DrawRectangleLinesEx(selected, lineWidth, Fade(Color{ 220, 54, 54, 220 }, fade));

        if (checkpointAvailable_)
        {
            const int size = (std::max)(10, static_cast<int>(18.0f * layout.scale));
            const int x = static_cast<int>(layout.imageArea.x + 14.0f * layout.scale);
            const float topMargin = layout.imageArea.y;
            const int y = topMargin > size + 8.0f
                ? static_cast<int>(topMargin - size - 6.0f)
                : static_cast<int>(topMargin + 10.0f * layout.scale);
            DrawText("[C] CONTINUE FROM CHECKPOINT", x, y, size, Fade(RAYWHITE, fade));
        }
        return;
    }

    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                           { 30, 0, 4, 0 }, Fade(Color{ 45, 3, 10, 100 }, fade));
    DrawCenteredText("GAME OVER", "GAME OVER", centerX,
                     layout.canvas.y + layout.canvas.height * 0.27f,
                     88.0f * layout.scale, Fade(Color{ 205, 67, 76, 255 }, fade));
    DrawCenteredText(u8"ここで力尽きました。", "You were defeated.", centerX,
                     layout.canvas.y + layout.canvas.height * 0.39f,
                     26.0f * layout.scale, Fade(Color{ 185, 181, 183, 255 }, fade));

    const Rectangle buttons[2] = { layout.retry, layout.title };
    const char* labels[2] = { u8"リトライ  [R]", u8"タイトルへ戻る  [B]" };
    const char* fallbacks[2] = { "RETRY  [R]", "RETURN TO TITLE  [B]" };
    for (int i = 0; i < 2; ++i)
    {
        const bool selected = i == selectedOption_;
        const Color fill = selected ? Color{ 76, 22, 29, 235 } : Color{ 14, 17, 20, 235 };
        const Color border = selected ? Color{ 180, 68, 79, 255 } : Color{ 68, 71, 76, 255 };
        const Color textColor = selected ? RAYWHITE : Color{ 176, 179, 183, 255 };
        DrawRectangleRec(buttons[i], Fade(fill, fade));
        DrawRectangleLinesEx(buttons[i], (std::max)(1.0f, 2.0f * layout.scale), Fade(border, fade));
        DrawCenteredText(labels[i], fallbacks[i], centerX,
                         buttons[i].y + (buttons[i].height - 28.0f * layout.scale) * 0.5f,
                         28.0f * layout.scale, Fade(textColor, fade));
    }

    const float hintY = layout.title.y + layout.title.height + 28.0f * layout.scale;
    DrawCenteredText(u8"上下キーで選択・Enterで決定 / マウス操作も可能",
                     "UP/DOWN: SELECT   ENTER: CONFIRM   MOUSE: CLICK",
                     centerX, hintY, 19.0f * layout.scale, Fade(Color{ 141, 143, 149, 255 }, fade));
    if (checkpointAvailable_)
        DrawCenteredText(u8"[C] チェックポイントから再開", "[C] CONTINUE FROM CHECKPOINT",
                         centerX, hintY + 30.0f * layout.scale, 19.0f * layout.scale,
                         Fade(Color{ 164, 169, 166, 255 }, fade));
}
