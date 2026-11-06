#include "TitleScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "ResourceManager.h"
#include "CheckpointSave.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr const char* CoverPath = "Data/Image/UI/Title_Cover.png";
    constexpr const char* CanopyPath = "Data/Image/UI/Title_Canopy.png";
    constexpr const char* LeavesPath = "Data/Image/UI/Title_Leaves.png";
    constexpr const char* LightningPath = "Data/Image/UI/Title_Lightning.png";

    // These values control the strength and speed of the title-screen breeze.
    constexpr float CanopySwayPixels = 18.0f; // Horizontal motion in the artwork's pixels.
    constexpr float CanopyVerticalPixels = 13.0f; // Diagonal, up-and-down motion.
    constexpr float CanopyTiltDegrees = 0.7f; // Subtle rotation around the upper-right corner.
    constexpr float WindSpeed = 0.8f;
    constexpr int FallingLeafCount = 16;
    constexpr float LeafFallSeconds = 7.5f; // Smaller values make leaves fall faster.
    constexpr float LeafDriftPixels = 32.0f; // Side-to-side drift at 1920 x 1080.
    constexpr float LightningIntervalSeconds = 6.0f;
    constexpr float LightningEffectSeconds = 0.60f;
    constexpr float IntroWindMix = 0.55f; // Lower the wind under the intro soundtrack.

    struct TitleLayout {
        float scale;
        Rectangle panel;
        Rectangle start;
        Rectangle checkpoint;
        Rectangle exit;
    };

    TitleLayout GetTitleLayout() {
        const float w = static_cast<float>((std::max)(1, GetScreenWidth()));
        const float h = static_cast<float>((std::max)(1, GetScreenHeight()));
        const float s = (std::max)(0.25f, (std::min)(w / 1920.0f, h / 1080.0f));
        const Rectangle panel{w * 0.055f, h * 0.23f, 670.0f * s, 515.0f * s};
        const float x = panel.x + 35.0f * s;
        const float width = panel.width - 70.0f * s;
        return {s, panel,
                {x, panel.y + 185.0f * s, width, 68.0f * s},
                {x, panel.y + 270.0f * s, width, 68.0f * s},
                {x, panel.y + 355.0f * s, width, 68.0f * s}};
    }

    Texture2D LoadTitleTexture(const char* path) {
        Texture2D texture{};
        if (FileExists(path)) {
            texture = LoadTexture(path);
            if (texture.id != 0) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        }
        if (texture.id == 0) TraceLog(LOG_WARNING, "TITLE: Could not load %s", path);
        return texture;
    }

    // Fill the screen without stretching. The cover and canopy share the
    // same screen rectangle so their original artwork positions stay aligned.
    Rectangle CoverArea(const Texture2D& texture) {
        const float w = static_cast<float>(GetScreenWidth());
        const float h = static_cast<float>(GetScreenHeight());
        const float scale = (std::max)(w / texture.width, h / texture.height);
        const float width = texture.width * scale;
        const float height = texture.height * scale;
        return {(w - width) * 0.5f, (h - height) * 0.5f, width, height};
    }

    void DrawWindLayer(const Texture2D& texture, Rectangle area, float x, float y,
                       float rotation, float sourceHeightRatio, float opacity) {
        if (texture.id == 0 || texture.width <= 0 || texture.height <= 0) return;
        // Only the top half is the canopy. This also excludes the stray mark
        // at the bottom right of the supplied image.
        const Rectangle source{0, 0, static_cast<float>(texture.width), texture.height * sourceHeightRatio};
        // Rotate around the upper-right corner so the canopy stays anchored.
        const Rectangle dest{area.x + area.width + x, area.y + y,
                             area.width, area.height * sourceHeightRatio};
        DrawTexturePro(texture, source, dest, {area.width, 0}, rotation, Fade(WHITE, opacity));
    }

    float Smooth01(float value) {
        const float t = (std::max)(0.0f, (std::min)(1.0f, value));
        return t * t * (3.0f - 2.0f * t);
    }

    // Draw the optional lightning on top of the scene. When the image matches
    // the cover dimensions, keep the designer's artwork aligned with it.
    void DrawLightningImage(const Texture2D& image, const Texture2D& cover, float opacity) {
        if (image.id == 0 || image.width <= 0 || image.height <= 0 || opacity <= 0.0f) return;
        Rectangle area{};
        if (cover.id != 0 && image.width == cover.width && image.height == cover.height) {
            area = CoverArea(cover);
        } else {
            const float screenWidth = static_cast<float>(GetScreenWidth());
            const float screenHeight = static_cast<float>(GetScreenHeight());
            const float scale = (std::min)(screenWidth / image.width, screenHeight / image.height);
            const float width = image.width * scale;
            const float height = image.height * scale;
            area = {(screenWidth - width) * 0.5f, (screenHeight - height) * 0.5f, width, height};
        }
        const Rectangle source{0, 0, static_cast<float>(image.width), static_cast<float>(image.height)};
        DrawTexturePro(image, source, area, {0, 0}, 0.0f, Fade(WHITE, opacity));
    }

    void DrawLightningEffect(const Texture2D& image, const Texture2D& cover, float time) {
        if (LightningIntervalSeconds <= 0.0f || LightningEffectSeconds <= 0.0f ||
            time < LightningIntervalSeconds) return; // First strike after about 3 seconds.
        const float phase = std::fmod(time - LightningIntervalSeconds, LightningIntervalSeconds);
        if (phase >= LightningEffectSeconds) return;

        // A quick flash, then a short blackout, and finally a gentle recovery.
        // The effects work even when the optional lightning PNG is missing.
        const float flashSeconds = LightningEffectSeconds * (7.0f / 30.0f);
        const float imageHoldSeconds = LightningEffectSeconds * 0.4f;
        const float flash = phase < flashSeconds ? 0.45f * (1.0f - phase / flashSeconds) : 0.0f;
        const float darkness = phase >= flashSeconds
            ? 0.82f * (1.0f - Smooth01((phase - flashSeconds) / (LightningEffectSeconds - flashSeconds))) : 0.0f;
        const float imageOpacity = phase < imageHoldSeconds ? 1.0f :
            1.0f - Smooth01((phase - imageHoldSeconds) / (LightningEffectSeconds - imageHoldSeconds));
        if (darkness > 0.0f)
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, darkness));
        DrawLightningImage(image, cover, imageOpacity);
        if (flash > 0.0f)
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(WHITE, flash));
    }

    // Rectangles in the supplied 2048 x 1423 transparent leaf image. Each
    // sprite falls independently instead of moving the whole layer together.
    constexpr Rectangle LeafSprites[] = {
        {1223, 62, 131, 85}, {1433, 376, 69, 105}, {1351, 509, 100, 87},
        {453, 618, 125, 95}, {712, 867, 97, 62}, {1686, 856, 97, 100},
        {1886, 876, 97, 105}, {62, 1148, 116, 95}, {755, 1179, 105, 96},
        {897, 1138, 78, 104}, {1473, 1227, 115, 128}
    };

    void DrawFallingLeaves(const Texture2D& texture, float time) {
        if (texture.id == 0 || texture.width != 2048 || texture.height != 1423) return;
        const float screenWidth = static_cast<float>(GetScreenWidth());
        const float screenHeight = static_cast<float>(GetScreenHeight());
        const float uiScale = (std::min)(screenWidth / 1920.0f, screenHeight / 1080.0f);
        const int spriteCount = static_cast<int>(sizeof(LeafSprites) / sizeof(LeafSprites[0]));
        for (int i = 0; i < FallingLeafCount; ++i) {
            const Rectangle source = LeafSprites[i % spriteCount];
            const float size = uiScale * (0.8f + 0.12f * (i % 4));
            const float width = source.width * size;
            const float height = source.height * size;
            const float duration = LeafFallSeconds + static_cast<float>(i % 5) * 0.65f;
            const float initialPhase = std::fmod(i * 0.618034f + 0.13f, 1.0f);
            const float phase = std::fmod(time / duration + initialPhase, 1.0f);
            const float horizontal = std::fmod(i * 0.754878f + 0.08f, 1.0f);
            const float x = screenWidth * horizontal +
                std::sin(time * (0.7f + 0.05f * (i % 3)) + i * 1.37f) * LeafDriftPixels * uiScale;
            // Start and wrap fully beyond the screen edge so the loop is invisible.
            const float y = -height + phase * (screenHeight + 2.0f * height);
            const float angle = std::sin(time * (0.65f + 0.06f * (i % 4)) + i * 0.91f) * 16.0f;
            const Rectangle dest{x, y, width, height};
            DrawTexturePro(texture, source, dest, {width * 0.5f, height * 0.5f}, angle, Fade(WHITE, 0.9f));
        }
    }

    void TextLeft(const char* jp, const char* en, float x, float y, float size, Color color) {
        const auto& font = JPFont();
        if (font.IsLoaded()) {
            font.DrawText(jp, static_cast<int>(x + 2), static_cast<int>(y + 2), size, {0, 0, 0, 180});
            font.DrawText(jp, static_cast<int>(x), static_cast<int>(y), size, color);
        } else {
            const int px = (std::max)(10, static_cast<int>(size));
            DrawText(en, static_cast<int>(x + 2), static_cast<int>(y + 2), px, {0, 0, 0, 180});
            DrawText(en, static_cast<int>(x), static_cast<int>(y), px, color);
        }
    }

    void TextCentered(const char* jp, const char* en, Rectangle area, float size, Color color) {
        const auto& font = JPFont();
        if (font.IsLoaded()) {
            const Vector2 textSize = font.MeasureText(jp, size);
            font.DrawText(jp, static_cast<int>(area.x + (area.width - textSize.x) * 0.5f),
                          static_cast<int>(area.y + (area.height - textSize.y) * 0.5f), size, color);
        } else {
            const int px = (std::max)(10, static_cast<int>(size));
            DrawText(en, static_cast<int>(area.x + (area.width - MeasureText(en, px)) * 0.5f),
                     static_cast<int>(area.y + (area.height - px) * 0.5f), px, color);
        }
    }

    void DrawTitleButton(Rectangle area, const char* jp, const char* en, float size, bool enabled) {
        const bool hovered = enabled && CheckCollisionPointRec(GetMousePosition(), area);
        const Color fill = !enabled ? Color{12, 17, 16, 165} :
                           hovered ? Color{50, 87, 65, 235} : Color{15, 27, 22, 215};
        const Color border = !enabled ? Color{72, 84, 76, 170} :
                             hovered ? Color{186, 222, 183, 255} : Color{125, 159, 133, 210};
        const Color text = enabled ? Color{230, 238, 226, 255} : Color{119, 133, 123, 220};
        DrawRectangleRec(area, fill);
        DrawRectangleLinesEx(area, (std::max)(1.0f, size * 0.055f), border);
        TextCentered(jp, en, area, size, text);
    }
}

void TitleScene::Init()
{
    intro_.Stop();
    finished = false;
    nextScene = nullptr;
    windTime_ = 0.0f;
    hoveredButton_ = -1;
    cover_ = LoadTitleTexture(CoverPath);
    canopy_ = LoadTitleTexture(CanopyPath);
    leaves_ = LoadTitleTexture(LeavesPath);
    // A missing optional image is normal; no warning and no placeholder needed.
    if (FileExists(LightningPath)) lightning_ = LoadTitleTexture(LightningPath);
    CheckpointData checkpoint;
    checkpointAvailable_ = CheckpointSave::Load(checkpoint);
    AM().PlayWind();
    EnableCursor();
}

void TitleScene::Update(float deltaTime)
{
    if (intro_.IsPlaying()) {
        if (!intro_.Update()) {
            AM().StopWind();
            auto* game = static_cast<GameScene*>(SM().GetScene(SceneID::Game));
            game->RequestIntroFadeIn();
            SetNextScene(game);
        } else {
            // Fade both sound layers with the same video fade-out.
            AM().SetWindMix(IntroWindMix * (1.0f - intro_.GetFadeOutAlpha()));
        }
        return;
    }
    const float previousWindTime = windTime_;
    windTime_ += (std::max)(0.0f, (std::min)(deltaTime, 0.1f));
    if (LightningIntervalSeconds > 0.0f &&
        static_cast<int>(windTime_ / LightningIntervalSeconds) >
        static_cast<int>(previousWindTime / LightningIntervalSeconds)) {
        AM().PlaySe(SeId::Thunder, 0.7f);
    }
    const TitleLayout layout = GetTitleLayout();
    const Vector2 mouse = GetMousePosition();
    const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool startClicked = click && CheckCollisionPointRec(mouse, layout.start);
    const bool continueClicked = click && checkpointAvailable_ && CheckCollisionPointRec(mouse, layout.checkpoint);
    const bool exitClicked = click && CheckCollisionPointRec(mouse, layout.exit);
    const int hovered = CheckCollisionPointRec(mouse, layout.start) ? 0 :
        CheckCollisionPointRec(mouse, layout.checkpoint) ? 1 :
        CheckCollisionPointRec(mouse, layout.exit) ? 2 : -1;
    if (hovered >= 0 && hovered != hoveredButton_) AM().PlayUiSe(SeId::Hover, 0.7f);
    hoveredButton_ = hovered;
    if (click && hovered == 1 && !checkpointAvailable_) AM().PlayUiSe(SeId::Error);

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || startClicked) {
        AM().PlaySe(SeId::Confirm);
        AM().StopSe(SeId::Thunder);
        AM().StopWind();
        // Keep the existing loading screen before the opening video.
        if (!SM().PrepareGameResources()) {
            finished = true;
            nextScene = nullptr;
            return;
        }
        if (!intro_.Start("Data/Video/IntroFrames.dat", "Data/Audio/Game Intro.mp3", AM().GetBgmVolume())) {
            // Missing/corrupt video: still enter the game with a smooth reveal.
            auto* game = static_cast<GameScene*>(SM().GetScene(SceneID::Game));
            game->RequestIntroFadeIn();
            SetNextScene(game);
        } else {
            AM().PlayWind();
            AM().SetWindMix(IntroWindMix);
        }
    }
    else if (checkpointAvailable_ && (IsKeyPressed(KEY_C) || continueClicked)) {
        AM().PlaySe(SeId::Confirm);
        static_cast<GameScene*>(SM().GetScene(SceneID::Game))->RequestCheckpointContinue();
        SetNextScene(SM().GetScene(SceneID::Game));
    }
    else if (IsKeyPressed(KEY_ESCAPE) || exitClicked) {
        AM().PlayUiSe(SeId::Confirm);
        finished = true;
        nextScene = nullptr;
    }
}

void TitleScene::Render() const
{
    if (intro_.IsPlaying()) {
        intro_.Draw();
        return;
    }

    ClearBackground({3, 5, 8, 255});
    if (cover_.id != 0 && cover_.width > 0 && cover_.height > 0) {
        const Rectangle area = CoverArea(cover_);
        const Rectangle source{0, 0, static_cast<float>(cover_.width), static_cast<float>(cover_.height)};
        DrawTexturePro(cover_, source, area, {0, 0}, 0.0f, WHITE);
        const float pixelScale = area.width / cover_.width;
        const float wave = windTime_ * WindSpeed;
        const float gust = std::sin(wave);
        const float x = (gust * CanopySwayPixels + std::sin(wave * 0.43f) * 4.0f) * pixelScale;
        const float y = (-gust * CanopyVerticalPixels + std::sin(wave * 0.61f) * 3.0f) * pixelScale;
        const float tilt = std::sin(wave * 0.83f) * CanopyTiltDegrees;
        DrawWindLayer(canopy_, area, x, y, tilt, 0.5f, 0.8f);
        DrawFallingLeaves(leaves_, windTime_);
    }

    const TitleLayout layout = GetTitleLayout();
    const float s = layout.scale;
    DrawRectangleRec(layout.panel, {3, 9, 7, 190});
    DrawRectangleLinesEx(layout.panel, (std::max)(1.0f, 2.0f * s), {117, 156, 126, 145});
    TextLeft(u8"学校脱出 3D", "ESCAPE SCHOOL 3D", layout.panel.x + 35 * s,
             layout.panel.y + 33 * s, 58 * s, {226, 236, 222, 255});
    DrawText("ESCAPE SCHOOL 3D", static_cast<int>(layout.panel.x + 39 * s),
             static_cast<int>(layout.panel.y + 110 * s), (std::max)(10, static_cast<int>(22 * s)),
             {159, 183, 163, 255});
    DrawTitleButton(layout.start, u8"最初から開始  [SPACE]", "NEW GAME  [SPACE]", 27 * s, true);
    DrawTitleButton(layout.checkpoint, checkpointAvailable_ ? u8"続きから  [C]" : u8"セーブデータなし",
                    checkpointAvailable_ ? "CONTINUE  [C]" : "NO CHECKPOINT", 25 * s, checkpointAvailable_);
    DrawTitleButton(layout.exit, u8"終了  [ESC]", "EXIT  [ESC]", 25 * s, true);
    TextLeft(u8"マウスでも選択できます", "CLICK A BUTTON OR USE THE KEYS", layout.panel.x + 38 * s,
             layout.panel.y + 460 * s, 20 * s, {158, 176, 163, 255});
    DrawLightningEffect(lightning_, cover_, windTime_);
}

void TitleScene::End()
{
    intro_.Stop();
    if (cover_.id != 0) UnloadTexture(cover_);
    if (canopy_.id != 0) UnloadTexture(canopy_);
    if (leaves_.id != 0) UnloadTexture(leaves_);
    if (lightning_.id != 0) UnloadTexture(lightning_);
    cover_ = {};
    canopy_ = {};
    leaves_ = {};
    lightning_ = {};
    AM().StopSe(SeId::Thunder);
    AM().StopWind();
}
