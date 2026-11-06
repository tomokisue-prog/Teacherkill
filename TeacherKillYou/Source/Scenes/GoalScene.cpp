#include "GoalScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

namespace
{
    // Artwork should share the original 1280 x 720 coordinates.
    constexpr const char* CoverPath = "Data/Image/UI/Win_Cover.png";
    constexpr const char* CurtainPaths[] = {
        "Data/Image/UI/Win_Curtain_1.png", "Data/Image/UI/Win_Curtain_2.png",
        "Data/Image/UI/Win_Curtain_3.png"
    };
    constexpr const char* LeavesPath = "Data/Image/UI/Win_Leaves.png";

    constexpr float FadeInSeconds = 1.0f;
    constexpr float CurtainSwayPixels = 42.0f; // Movement of the free edge, in artwork pixels.
    constexpr float CurtainWindSpeed = 1.35f;   // Larger = faster breeze.
    constexpr float CurtainLiftPixels = 7.0f;   // Small up/down flutter of the free edge.
    constexpr int FallingLeafCount = 16;
    constexpr float LeafFallSeconds = 9.0f;    // Smaller = faster falling leaves.

    float Smooth01(float value)
    {
        const float t = (std::max)(0.0f, (std::min)(1.0f, value));
        return t * t * (3.0f - 2.0f * t);
    }

    Texture2D LoadWinTexture(const char* path)
    {
        Texture2D texture{};
        if (FileExists(path)) texture = LoadTexture(path);
        if (texture.id) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        else TraceLog(LOG_WARNING, "WIN: Could not load %s", path);
        return texture;
    }

    // Fill the window without stretching. Every layer shares this rectangle.
    Rectangle ArtworkArea()
    {
        const float w = static_cast<float>((std::max)(GetScreenWidth(), 1));
        const float h = static_cast<float>((std::max)(GetScreenHeight(), 1));
        const float scale = (std::max)(w / 1280.0f, h / 720.0f);
        const float width = 1280.0f * scale;
        const float height = 720.0f * scale;
        return {(w - width) * 0.5f, (h - height) * 0.5f, width, height};
    }

    Rectangle TitleButton()
    {
        const float w = static_cast<float>((std::max)(GetScreenWidth(), 1));
        const float h = static_cast<float>((std::max)(GetScreenHeight(), 1));
        const float scale = (std::min)(w / 1280.0f, h / 720.0f);
        const float width = 245.0f * scale;
        const float height = 55.0f * scale;
        // Adjust this position when the designer's text image arrives.
        return {(w - width) * 0.5f, h - height - 34.0f * scale, width, height};
    }

    void DrawCurtain(const Texture2D& texture, Rectangle area, float time)
    {
        if (!texture.id || texture.width <= 0 || texture.height <= 0) return;
        constexpr int strips = 48;
        const float scale = area.width / 1280.0f;

        // Adjacent strips share their actual vertices. This creates a continuous
        // flowing shape rather than separately shifted rectangles with seams.
        rlCheckRenderBatchLimit(strips * 2 * 4);
        rlSetTexture(texture.id);
        rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        for (int side = 0; side < 2; ++side)
        {
            // The transparent artwork has a curtain in each outer quarter.
            const float u0 = side == 0 ? 0.0f : 0.75f;
            const float u1 = side == 0 ? 0.25f : 1.0f;
            const float left = area.x + area.width * u0;
            const float right = area.x + area.width * u1;
            const auto motion = [time, side, scale](float heightRatio) -> Vector2
            {
                // The top and outside edge stay fixed; the free edge moves
                // more at the bottom, with two slow waves for a natural gust.
                const float bend = heightRatio * heightRatio * (3.0f - 2.0f * heightRatio);
                const float wind = 0.78f * std::sin(time * CurtainWindSpeed - heightRatio * 2.6f + side * 0.35f)
                    + 0.22f * std::sin(time * 0.63f + heightRatio * 4.0f + side * 0.8f);
                const float sideways = wind * CurtainSwayPixels * bend * scale * (side == 0 ? 1.0f : -1.0f);
                const float lift = std::sin(time * 1.05f - heightRatio * 3.0f + side * 0.8f)
                    * CurtainLiftPixels * bend * scale;
                return {sideways, lift};
            };
            for (int i = 0; i < strips; ++i)
            {
                const float top = static_cast<float>(i) / strips;
                const float bottom = static_cast<float>(i + 1) / strips;
                const Vector2 atTop = motion(top);
                const Vector2 atBottom = motion(bottom);
                const float topY = area.y + area.height * top;
                const float bottomY = area.y + area.height * bottom;
                const float leftTopX = left + (side == 1 ? atTop.x : 0.0f);
                const float rightTopX = right + (side == 0 ? atTop.x : 0.0f);
                const float leftBottomX = left + (side == 1 ? atBottom.x : 0.0f);
                const float rightBottomX = right + (side == 0 ? atBottom.x : 0.0f);

                rlTexCoord2f(u0, top);
                rlVertex2f(leftTopX, topY + (side == 1 ? atTop.y : 0.0f));
                rlTexCoord2f(u0, bottom);
                rlVertex2f(leftBottomX, bottomY + (side == 1 ? atBottom.y : 0.0f));
                rlTexCoord2f(u1, bottom);
                rlVertex2f(rightBottomX, bottomY + (side == 0 ? atBottom.y : 0.0f));
                rlTexCoord2f(u1, top);
                rlVertex2f(rightTopX, topY + (side == 0 ? atTop.y : 0.0f));
            }
        }
        rlEnd();
        rlSetTexture(0);
    }

    // Individual leaves cut from the supplied transparent 1280 x 720 layer.
    constexpr Rectangle LeafSprites[] = {
        {248, 196, 35, 40}, {734, 654, 33, 44}, {514, 350, 36, 29},
        {665, 653, 25, 36}, {816, 411, 34, 49}, {159, 435, 25, 37},
        {944, 181, 42, 30}, {384, 398, 25, 34}
    };

    void DrawLeaves(const Texture2D& texture, Rectangle area, float time)
    {
        if (!texture.id || texture.width != 1280 || texture.height != 720) return;
        const int spriteCount = static_cast<int>(sizeof(LeafSprites) / sizeof(LeafSprites[0]));
        const float scale = area.width / 1280.0f;
        for (int i = 0; i < FallingLeafCount; ++i)
        {
            const Rectangle source = LeafSprites[i % spriteCount];
            const float size = (0.55f + 0.09f * (i % 4)) * scale;
            const float width = source.width * size;
            const float height = source.height * size;
            const float duration = LeafFallSeconds + 0.65f * (i % 5);
            const float progress = std::fmod(time / duration + i * 0.618034f, 1.0f);
            const float lane = 0.08f + std::fmod(i * 0.371f, 0.84f);
            const float x = area.x + area.width * lane +
                std::sin(time * 0.9f + i * 1.7f) * 26.0f * scale;
            const float y = area.y - height + progress * (area.height + height * 2.0f);
            const float rotation = std::sin(time * 1.1f + i * 2.0f) * 32.0f;
            const Rectangle dest{x, y, width, height};
            DrawTexturePro(texture, source, dest, {width * 0.5f, height * 0.5f},
                           rotation, Fade(WHITE, 0.78f));
        }
    }
}

void GoalScene::Init()
{
    End();
    finished = false;
    nextScene = nullptr;
    elapsed_ = 0.0f;
    cover_ = LoadWinTexture(CoverPath);
    for (int i = 0; i < 3; ++i) curtains_[i] = LoadWinTexture(CurtainPaths[i]);
    leaves_ = LoadWinTexture(LeavesPath);
    EnableCursor();
}

void GoalScene::Update(float deltaTime)
{
    elapsed_ += (std::max)(0.0f, (std::min)(deltaTime, 0.1f));
    if (elapsed_ < FadeInSeconds) return;

    const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(GetMousePosition(), TitleButton());
    if (clicked || IsKeyPressed(KEY_ENTER))
    {
        AM().PlaySe(SeId::Confirm);
        SetNextScene(SM().GetScene(SceneID::Title));
    }
}

void GoalScene::Render() const
{
    ClearBackground({20, 27, 18, 255});
    const Rectangle area = ArtworkArea();
    if (cover_.id && cover_.width > 0 && cover_.height > 0)
    {
        const Rectangle source{0, 0, static_cast<float>(cover_.width), static_cast<float>(cover_.height)};
        DrawTexturePro(cover_, source, area, {0, 0}, 0.0f, WHITE);
    }

    // Animate one continuous piece of artwork, avoiding a visible fade between
    // drawn frames. The other supplied frames remain usable as fallbacks.
    int curtainIndex = curtains_[1].id ? 1 : (curtains_[0].id ? 0 : (curtains_[2].id ? 2 : -1));
    if (curtainIndex >= 0) DrawCurtain(curtains_[curtainIndex], area, elapsed_);
    DrawLeaves(leaves_, area, elapsed_);

    const Rectangle button = TitleButton();
    const bool hovered = elapsed_ >= FadeInSeconds && CheckCollisionPointRec(GetMousePosition(), button);
    const Color outline = hovered ? Color{244, 230, 171, 230} : Color{189, 194, 151, 185};
    const Color fill = hovered ? Color{42, 58, 34, 225} : Color{23, 32, 25, 205};
    DrawRectangleRounded(button, 0.22f, 8, outline);
    const float inset = (std::max)(1.0f, button.height * 0.035f);
    const Rectangle inner{button.x + inset, button.y + inset,
                          button.width - inset * 2.0f, button.height - inset * 2.0f};
    DrawRectangleRounded(inner, 0.20f, 8, fill);
    const int fontSize = (std::max)(10, static_cast<int>(button.height * 0.36f));
    const char* text = "BACK TO TITLE";
    DrawText(text, static_cast<int>(button.x + (button.width - MeasureText(text, fontSize)) * 0.5f),
             static_cast<int>(button.y + (button.height - fontSize) * 0.5f),
             fontSize, Color{247, 244, 220, 255});

    const float fade = 1.0f - Smooth01(elapsed_ / FadeInSeconds);
    if (fade > 0.0f) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, fade));
}

void GoalScene::End()
{
    if (cover_.id) UnloadTexture(cover_);
    for (Texture2D& curtain : curtains_)
    {
        if (curtain.id) UnloadTexture(curtain);
        curtain = {};
    }
    if (leaves_.id) UnloadTexture(leaves_);
    cover_ = {};
    leaves_ = {};
    elapsed_ = 0.0f;
}
