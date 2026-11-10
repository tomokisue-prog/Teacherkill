#include "TitleScene.h"
#include "SceneManager.h"
#include "AudioManager.h"
#include "CheckpointSave.h"
#include "ResourceManager.h"
#include "rlgl.h"
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
    constexpr float FirstLightningSeconds = 4.0f;
    constexpr float LightningFrameSeconds = 0.25f;
    constexpr float ThunderDelaySeconds = 0.37f;
    constexpr float TitleWindMix = 0.22f;

    const char* LightningLightFragment = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 viewportSize;
uniform float flashStrength;
out vec4 finalColor;
void main() {
    vec4 surface = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec2 position = vec2(gl_FragCoord.x, viewportSize.y - gl_FragCoord.y) / viewportSize;
    vec2 distanceFromFlash = (position - vec2(0.79, 0.30)) / vec2(0.46, 0.70);
    float falloff = 1.0 - smoothstep(0.05, 1.0, length(distanceFromFlash));
    vec3 reflectedLight = surface.rgb * vec3(0.23, 0.34, 0.50) + vec3(0.045, 0.060, 0.092);
    finalColor = vec4(clamp(surface.rgb + reflectedLight * falloff * flashStrength, 0.0, 1.0), surface.a);
})";

    enum MenuButton { StartButton, SaveButton, ExitButton, ControlsButton, ButtonCount };

    // Trim the transparent margins and split the menu at the gaps in the artwork.
    constexpr Rectangle LogoSource{180.0f, 37.0f, 1157.0f, 954.0f};
    constexpr Rectangle ButtonSources[] = {
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
        for (int i = StartButton; i <= ExitButton; ++i) {
            const Rectangle source = ButtonSources[i];
            layout.buttons[i] = {menuX, menuY + (source.y - MenuTop) * menuScale,
                                 source.width * menuScale, source.height * menuScale};
        }
        const float uiScale = (std::min)(w / 1600.0f, h / 900.0f);
        layout.buttons[ControlsButton] = {w - 260 * uiScale, h - 90 * uiScale, 220 * uiScale, 50 * uiScale};
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

    struct LightningFrame {
        float light;
        float bolt;
    };

    LightningFrame GetLightningFrame(float age) {
        constexpr float lightLevels[] = {0.55f, 1.0f, 0.70f, 0.48f, 0.95f};
        constexpr float boltLevels[] = {0.0f, 1.0f, 0.82f, 0.0f, 1.0f};
        constexpr int frameCount = 5;
        if (age < 0.0f || age >= LightningFrameSeconds * frameCount) return {0.0f, 0.0f};
        const int frame = (std::min)(frameCount - 1, static_cast<int>(age / LightningFrameSeconds));
        const float localTime = age - frame * LightningFrameSeconds;
        const float transition = Smooth01(localTime / 0.04f);
        const float previousLight = frame > 0 ? lightLevels[frame - 1] : 0.0f;
        float light = previousLight + (lightLevels[frame] - previousLight) * transition;
        if (frame == frameCount - 1) light *= 1.0f - Smooth01((localTime - 0.20f) / 0.05f);

        float bolt = boltLevels[frame];
        if (bolt > 0.0f) {
            const float previousBolt = frame > 0 ? boltLevels[frame - 1] : 0.0f;
            bolt = previousBolt + (bolt - previousBolt) * transition;
            if (frame + 1 == frameCount || boltLevels[frame + 1] == 0.0f)
                bolt *= 1.0f - Smooth01((localTime - 0.21f) / 0.04f);
        }
        return {light, bolt};
    }

    // Place the bolt behind the moving canopy, near the upper-right corner.
    void DrawLightningImage(const Texture2D& image, float opacity) {
        if (image.id == 0 || image.width <= 0 || image.height <= 0 || opacity <= 0.0f) return;
        const float screenWidth = static_cast<float>(GetScreenWidth());
        const float screenHeight = static_cast<float>(GetScreenHeight());
        const float scale = (std::min)(screenWidth * 0.95f / image.width, screenHeight * 0.90f / image.height);
        const float width = image.width * scale;
        const float height = image.height * scale;
        const Rectangle area{screenWidth * 0.985f - width, screenHeight * 0.015f, width, height};
        const Rectangle source{0, 0, static_cast<float>(image.width), static_cast<float>(image.height)};
        DrawTexturePro(image, source, area, {0, 0}, 0.0f, Fade(WHITE, opacity));
        // Add a soft blue-white glow along the bolt.
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(image, source, area, {0, 0}, 0.0f, Fade(Color{175, 205, 255, 255}, opacity * 0.22f));
        EndBlendMode();
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
        if (button == ControlsButton) {
            DrawRectangleRec(dest, hovered ? Color{67, 32, 34, 245} : Color{20, 24, 24, 235});
            DrawRectangleLinesEx(dest, (std::max)(1.0f, dest.height * .04f),
                                 hovered ? Color{204, 150, 130, 255} : Color{125, 137, 125, 255});
            const auto& font = JPFont();
            if (font.IsLoaded()) {
                const char* label = u8"操作説明  [F1]";
                const float size = dest.height * .48f;
                const float width = font.MeasureText(label, size).x;
                font.DrawText(label, int(dest.x + (dest.width - width) * .5f),
                              int(dest.y + (dest.height - size) * .5f), size, RAYWHITE);
            } else {
                DrawFallbackLabel("CONTROLS  [F1]", dest, .48f, RAYWHITE);
            }
            return;
        }
        if (menu.id == 0) {
            constexpr const char* labels[] = {"START", "SAVE", "EXIT GAME"};
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
    lastLightningAt_ = -1.0f;
    nextLightningAt_ = FirstLightningSeconds;
    hoveredButton_ = -1;
    controlsGuide_.Close();
    cover_ = LoadTitleTexture(CoverPath);
    canopy_ = LoadTitleTexture(CanopyPath);
    leaves_ = LoadTitleTexture(LeavesPath);
    logo_ = LoadTitleTexture(LogoPath);
    menu_ = LoadTitleTexture(MenuPath);
    // A missing optional image is normal; no warning and no placeholder needed.
    if (FileExists(LightningPath)) lightning_ = LoadTitleTexture(LightningPath);
    lightningLight_ = LoadShaderFromMemory(nullptr, LightningLightFragment);
    if (lightningLight_.id == 0 || lightningLight_.id == rlGetShaderIdDefault()) {
        lightningLight_ = {};
        TraceLog(LOG_WARNING, "TITLE: Could not load lightning light shader.");
    } else {
        lightningStrengthLocation_ = GetShaderLocation(lightningLight_, "flashStrength");
        lightningViewportLocation_ = GetShaderLocation(lightningLight_, "viewportSize");
    }
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
        if (IsKeyPressed(KEY_ESCAPE) || !intro_.Update()) {
            intro_.Stop();
            auto* game = static_cast<GameScene*>(SM().GetScene(SceneID::Game));
            game->RequestIntroFadeIn();
            SetNextScene(game);
        }
        return;
    }
    const float previousWindTime = windTime_;
    windTime_ += (std::max)(0.0f, (std::min)(deltaTime, 0.1f));
    if (windTime_ >= nextLightningAt_) {
        lastLightningAt_ = windTime_;
        nextLightningAt_ = windTime_ + GetRandomValue(8000, 15000) / 1000.0f;
    }
    if (lastLightningAt_ >= 0.0f && previousWindTime < lastLightningAt_ + ThunderDelaySeconds &&
        windTime_ >= lastLightningAt_ + ThunderDelaySeconds) {
        AM().PlaySe(SeId::Thunder, 0.7f);
    }
    if (controlsGuide_.IsOpen()) {
        controlsGuide_.Update();
        return;
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

    if (IsKeyPressed(KEY_F1) || (click && hovered == ControlsButton)) {
        AM().PlayUiSe(SeId::Confirm);
        controlsGuide_.Open();
    }
    else if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || (click && hovered == StartButton)) {
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
        const int size = (std::max)(16, GetScreenHeight() / 42);
        const int margin = size;
        const char* hint = "[ESC]  SKIP";
        const int width = MeasureText(hint, size);
        const int x = GetScreenWidth() - width - margin * 2;
        const int y = GetScreenHeight() - size - margin * 2;
        DrawRectangle(x - margin / 2, y - margin / 2, width + margin, size + margin, Fade(BLACK, .76f));
        DrawText(hint, x, y, size, RAYWHITE);
        return;
    }

    ClearBackground({3, 5, 8, 255});
    if (cover_.id != 0 && cover_.width > 0 && cover_.height > 0) {
        const float age = lastLightningAt_ < 0.0f ? -1.0f : windTime_ - lastLightningAt_;
        const LightningFrame frame = GetLightningFrame(age);
        const float lightStrength = frame.light;
        const float boltOpacity = frame.bolt;
        const bool lightActive = lightningLight_.id != 0 && lightStrength > 0.0f &&
            lightningStrengthLocation_ >= 0 && lightningViewportLocation_ >= 0;
        if (lightActive) {
            const Vector2 viewport{static_cast<float>((std::max)(1, GetRenderWidth())),
                                   static_cast<float>((std::max)(1, GetRenderHeight()))};
            SetShaderValue(lightningLight_, lightningStrengthLocation_, &lightStrength, SHADER_UNIFORM_FLOAT);
            SetShaderValue(lightningLight_, lightningViewportLocation_, &viewport, SHADER_UNIFORM_VEC2);
            BeginShaderMode(lightningLight_);
        }
        const Rectangle area = CoverArea(cover_);
        const Rectangle source{0, 0, static_cast<float>(cover_.width), static_cast<float>(cover_.height)};
        DrawTexturePro(cover_, source, area, {0, 0}, 0.0f, WHITE);
        DrawLightningImage(lightning_, boltOpacity);
        const float pixelScale = area.width / cover_.width;
        const float wave = windTime_ * WindSpeed;
        const float gust = std::sin(wave);
        const float x = (gust * CanopySwayPixels + std::sin(wave * 0.43f) * 4.0f) * pixelScale;
        const float y = (-gust * CanopyVerticalPixels + std::sin(wave * 0.61f) * 3.0f) * pixelScale;
        const float tilt = std::sin(wave * 0.83f) * CanopyTiltDegrees;
        DrawWindLayer(canopy_, area, x, y, tilt, 0.5f, 0.8f);
        DrawFallingLeaves(leaves_, windTime_);
        if (lightActive) EndShaderMode();
    }

    const TitleLayout layout = GetTitleLayout();
    if (logo_.id != 0)
        DrawTexturePro(logo_, LogoSource, layout.logo, {0, 0}, 0.0f, WHITE);
    else
        DrawFallbackLabel("THE LAST CLASS", layout.logo, 0.22f, RAYWHITE);

    for (int i = 0; i < ButtonCount; ++i)
        DrawMenuButton(menu_, i, layout.buttons[i], i != SaveButton || checkpointAvailable_, hoveredButton_ == i);
    if (controlsGuide_.IsOpen()) controlsGuide_.Draw();
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
    if (lightningLight_.id != 0) UnloadShader(lightningLight_);
    cover_ = {};
    canopy_ = {};
    leaves_ = {};
    logo_ = {};
    menu_ = {};
    lightning_ = {};
    lightningLight_ = {};
    lightningStrengthLocation_ = -1;
    lightningViewportLocation_ = -1;
    StopTitleAudio();
}
