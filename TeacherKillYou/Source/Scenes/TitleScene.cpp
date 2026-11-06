#include "TitleScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "CheckpointSave.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr const char* CoverPath = "Data/Image/UI/Title_Cover.png";
    constexpr const char* CanopyPath = "Data/Image/UI/Title_Canopy.png";
    constexpr const char* LeavesPath = "Data/Image/UI/Title_Leaves.png";
    constexpr const char* LogoPath = "Data/Image/UI/The_Last_Class_Title.png";
    constexpr const char* MenuPath = "Data/Image/UI/The_Last_Classl.png";
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
    constexpr float TitleWindMix = 0.22f;

    enum MenuButton { StartButton, SaveButton, ExitButton, ButtonCount };

    // Trim the transparent margins and split the menu at the gaps in the artwork.
    constexpr Rectangle LogoSource{180.0f, 37.0f, 1157.0f, 954.0f};
    constexpr Rectangle ButtonSources[ButtonCount] = {
        {262.0f, 225.0f, 1011.0f, 203.0f},
        {262.0f, 428.0f, 1011.0f, 181.0f},
        {262.0f, 609.0f, 1011.0f, 196.0f}
    };
    constexpr float MenuTop = ButtonSources[StartButton].y;
    constexpr float MenuHeight = ButtonSources[ExitButton].y + ButtonSources[ExitButton].height - MenuTop;

    struct TitleLayout {
        Rectangle logo;
        Rectangle buttons[ButtonCount];
    };

    TitleLayout GetTitleLayout() {
        const float w = static_cast<float>((std::max)(1, GetScreenWidth()));
        const float h = static_cast<float>((std::max)(1, GetScreenHeight()));
        const float logoScale = (std::min)(w * 0.49f / LogoSource.width, h * 0.59f / LogoSource.height);
        const float menuScale = (std::min)(w * 0.44f / ButtonSources[StartButton].width, h * 0.30f / MenuHeight);
        const float menuWidth = ButtonSources[StartButton].width * menuScale;
        const float menuX = (w - menuWidth) * 0.5f;
        const float menuY = h * 0.95f - MenuHeight * menuScale;

        TitleLayout layout{};
        layout.logo = {w * 0.045f, h * 0.055f, LogoSource.width * logoScale, LogoSource.height * logoScale};
        for (int i = 0; i < ButtonCount; ++i) {
            const Rectangle source = ButtonSources[i];
            layout.buttons[i] = {menuX, menuY + (source.y - MenuTop) * menuScale,
                                 source.width * menuScale, source.height * menuScale};
        }
        return layout;
    }

    void StopTitleAudio() {
        AM().StopSe(SeId::Thunder);
        AM().StopWind();
        AM().StopBgm();
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
            time < LightningIntervalSeconds) return;
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
        if (flash > 0.0f)
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(WHITE, flash));
        DrawLightningImage(image, cover, imageOpacity);
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

    void DrawFallbackLabel(const char* label, Rectangle area, float heightRatio, Color tint) {
        int size = (std::max)(1, static_cast<int>(area.height * heightRatio));
        const int textWidth = MeasureText(label, size);
        if (textWidth > area.width)
            size = (std::max)(1, static_cast<int>(size * area.width / textWidth));
        DrawText(label, static_cast<int>(area.x + (area.width - MeasureText(label, size)) * 0.5f),
                 static_cast<int>(area.y + (area.height - size) * 0.5f), size, tint);
    }

    void DrawMenuButton(Texture2D menu, int button, Rectangle dest, bool enabled, bool hovered) {
        if (menu.id == 0) {
            constexpr const char* labels[ButtonCount] = {"START", "SAVE", "EXIT GAME"};
            const char* label = enabled ? labels[button] : "NO SAVE DATA";
            const Color tint = !enabled ? GRAY : hovered ? WHITE : RAYWHITE;
            DrawFallbackLabel(label, dest, 0.36f, tint);
            return;
        }

        const Rectangle source = ButtonSources[button];
        const Color tint = enabled ? WHITE : Color{135, 135, 135, 255};
        DrawTexturePro(menu, source, dest, {0, 0}, 0.0f, tint);
        if (enabled && hovered) {
            // The texture's alpha keeps the light on the painted button.
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(menu, source, dest, {0, 0}, 0.0f, Fade(WHITE, 0.36f));
            EndBlendMode();
        }
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
    logo_ = LoadTitleTexture(LogoPath);
    menu_ = LoadTitleTexture(MenuPath);
    // A missing optional image is normal; no warning and no placeholder needed.
    if (FileExists(LightningPath)) lightning_ = LoadTitleTexture(LightningPath);
    CheckpointData checkpoint;
    checkpointAvailable_ = CheckpointSave::Load(checkpoint);
    AM().PlayBgm(BgmId::Title);
    AM().PlayWind();
    AM().SetWindMix(TitleWindMix);
    EnableCursor();
}

void TitleScene::Update(float deltaTime)
{
    if (intro_.IsPlaying()) {
        if (!intro_.Update()) {
            auto* game = static_cast<GameScene*>(SM().GetScene(SceneID::Game));
            game->RequestIntroFadeIn();
            SetNextScene(game);
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
    int hovered = -1;
    for (int i = 0; i < ButtonCount; ++i) {
        if (CheckCollisionPointRec(mouse, layout.buttons[i])) {
            hovered = i;
            break;
        }
    }
    if (hovered >= 0 && hovered != hoveredButton_ && (hovered != SaveButton || checkpointAvailable_))
        AM().PlayUiSe(SeId::Hover, 0.7f);
    hoveredButton_ = hovered;
    if (click && hovered == SaveButton && !checkpointAvailable_) AM().PlayUiSe(SeId::Error);

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || (click && hovered == StartButton)) {
        AM().PlaySe(SeId::Confirm);
        StopTitleAudio();
        // Keep the existing loading screen before the opening video.
        if (!SM().PrepareGameResources()) {
            finished = true;
            nextScene = nullptr;
            return;
        }
        if (!intro_.Start("Data/Video/IntroFrames.dat", "Data/Video/IntroAudio.ogg", AM().GetBgmVolume())) {
            // Missing/corrupt video: still enter the game with a smooth reveal.
            auto* game = static_cast<GameScene*>(SM().GetScene(SceneID::Game));
            game->RequestIntroFadeIn();
            SetNextScene(game);
        }
    }
    else if (checkpointAvailable_ && (IsKeyPressed(KEY_C) || (click && hovered == SaveButton))) {
        AM().PlaySe(SeId::Confirm);
        StopTitleAudio();
        static_cast<GameScene*>(SM().GetScene(SceneID::Game))->RequestCheckpointContinue();
        SetNextScene(SM().GetScene(SceneID::Game));
    }
    else if (IsKeyPressed(KEY_ESCAPE) || (click && hovered == ExitButton)) {
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
    if (logo_.id != 0)
        DrawTexturePro(logo_, LogoSource, layout.logo, {0, 0}, 0.0f, WHITE);
    else
        DrawFallbackLabel("THE LAST CLASS", layout.logo, 0.22f, RAYWHITE);

    for (int i = 0; i < ButtonCount; ++i)
        DrawMenuButton(menu_, i, layout.buttons[i], i != SaveButton || checkpointAvailable_, hoveredButton_ == i);

    DrawLightningEffect(lightning_, cover_, windTime_);
}

void TitleScene::End()
{
    intro_.Stop();
    if (cover_.id != 0) UnloadTexture(cover_);
    if (canopy_.id != 0) UnloadTexture(canopy_);
    if (leaves_.id != 0) UnloadTexture(leaves_);
    if (logo_.id != 0) UnloadTexture(logo_);
    if (menu_.id != 0) UnloadTexture(menu_);
    if (lightning_.id != 0) UnloadTexture(lightning_);
    cover_ = {};
    canopy_ = {};
    leaves_ = {};
    logo_ = {};
    menu_ = {};
    lightning_ = {};
    StopTitleAudio();
}
