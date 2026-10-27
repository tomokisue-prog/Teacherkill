#pragma once

#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// HUD for the supplied HP_Bar.png. This class owns its texture; the Player
// initializes it once and calls End() when the gameplay scene ends.
class PlayerStatusHud
{
public:
    bool Init(const char* imagePath)
    {
        End();
        if (imagePath == nullptr || !FileExists(imagePath))
        {
            TraceLog(LOG_WARNING, "PLAYER HUD: Image not found: %s", imagePath ? imagePath : "(null)");
            return false;
        }

        Image image = LoadImage(imagePath);
        if (image.data == nullptr || image.width <= 0 || image.height <= 0)
        {
            if (image.data != nullptr) UnloadImage(image);
            TraceLog(LOG_WARNING, "PLAYER HUD: Could not load %s", imagePath);
            return false;
        }

        // UpdateTexture below receives Color pixels, so use the same format.
        ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        if (image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
        {
            UnloadImage(image);
            TraceLog(LOG_WARNING, "PLAYER HUD: Could not convert image to RGBA: %s", imagePath);
            return false;
        }

        Color* pixels = LoadImageColors(image);
        if (pixels == nullptr)
        {
            UnloadImage(image);
            TraceLog(LOG_WARNING, "PLAYER HUD: Could not read image pixels: %s", imagePath);
            return false;
        }

        const std::size_t count = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
        originalPixels_.assign(pixels, pixels + count);
        UnloadImageColors(pixels);

        BuildArcMasks(image.width, image.height);
        texture_ = LoadTextureFromImage(image);
        UnloadImage(image);
        if (texture_.id == 0)
        {
            TraceLog(LOG_WARNING, "PLAYER HUD: Could not create texture: %s", imagePath);
            End();
            return false;
        }

        SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
        return true;
    }

    void End()
    {
        if (texture_.id != 0) UnloadTexture(texture_);
        texture_ = {};
        std::vector<Color>().swap(originalPixels_);
        std::vector<Color>().swap(originalPatch_);
        std::vector<Color>().swap(displayPatch_);
        std::vector<ArcPixel>().swap(healthArc_);
        std::vector<ArcPixel>().swap(staminaArc_);
        patchX_ = patchY_ = patchWidth_ = patchHeight_ = 0;
        lastHp_ = lastMaxHp_ = lastStaminaPercent_ = -1;
    }

    bool IsReady() const { return texture_.id != 0; }

    void Draw(int hp, int maxHp, int ammo, int reserve,
              float stamina, float maxStamina, bool reloading)
    {
        if (!IsReady()) return;

        RefreshArcs(hp, maxHp, stamina, maxStamina);

        // Keep this HUD to the left of the existing four inventory slots.
        float uiScale = Clamp((std::min)(GetScreenWidth() / 1920.0f,
                                         GetScreenHeight() / 1080.0f), 0.45f, 1.5f);
        const float inventoryStart = (GetScreenWidth() - 344.0f) * 0.5f;
        if (inventoryStart > 8.0f)
            uiScale = (std::min)(uiScale, (inventoryStart - 8.0f) / 501.3f);
        uiScale = (std::max)(0.15f, uiScale);

        const float width = 370.0f * uiScale; // CHANGE: overall HUD size
        const float height = width * texture_.height / texture_.width;
        const float x = 16.0f * uiScale; // CHANGE: distance from left edge
        const float y = GetScreenHeight() - height - 16.0f * uiScale; // CHANGE: bottom margin
        const Rectangle imageRect{ x, y, width, height };
        const Rectangle source{ 0.0f, 0.0f, static_cast<float>(texture_.width), static_cast<float>(texture_.height) };
        DrawTexturePro(texture_, source, imageRect, { 0.0f, 0.0f }, 0.0f, WHITE);

        // Small dark backing keeps the values readable over the level.
        const Rectangle values{ x + width * 0.69f, y + height * 0.46f,
                                230.0f * uiScale, 136.0f * uiScale };
        DrawRectangleRec(values, Color{ 7, 9, 10, 200 });
        const int textX = static_cast<int>(values.x + 8.0f * uiScale);
        const float textY = values.y + 5.0f * uiScale;
        const int hpSize = (std::max)(8, static_cast<int>(25.0f * uiScale));
        const int ammoSize = (std::max)(8, static_cast<int>(23.0f * uiScale));
        const int staminaSize = (std::max)(8, static_cast<int>(20.0f * uiScale));
        const int reloadSize = (std::max)(8, static_cast<int>(18.0f * uiScale));
        const Color hpColor = hp <= 25 ? RED : hp <= 50 ? ORANGE : RAYWHITE;
        const Color staminaColor = stamina <= 0.0f ? ORANGE : SKYBLUE;

        DrawShadowText(TextFormat("HP %d / %d", hp, maxHp), textX,
                       static_cast<int>(textY), hpSize, hpColor, uiScale);
        DrawShadowText(TextFormat("AMMO %d / %d", ammo, reserve), textX,
                       static_cast<int>(textY + 31.0f * uiScale), ammoSize,
                       ammo > 0 ? RAYWHITE : RED, uiScale);
        const int staminaValue = static_cast<int>(std::round(Clamp(stamina, 0.0f, (std::max)(0.0f, maxStamina))));
        DrawShadowText(TextFormat("STAMINA %d", staminaValue), textX,
                       static_cast<int>(textY + 59.0f * uiScale), staminaSize, staminaColor, uiScale);
        if (reloading)
            DrawShadowText("RELOADING", textX, static_cast<int>(textY + 85.0f * uiScale),
                           reloadSize, YELLOW, uiScale);
    }

private:
    struct ArcPixel
    {
        std::size_t index;
        float progress;
    };

    static void DrawShadowText(const char* text, int x, int y, int size, Color color, float scale)
    {
        const int offset = (std::max)(1, static_cast<int>(2.0f * scale));
        DrawText(text, x + offset, y + offset, size, Color{ 12, 12, 12, 230 });
        DrawText(text, x, y, size, color);
    }

    void BuildArcMasks(int width, int height)
    {
        // Coordinates below match the supplied 606 x 586 image. They scale
        // with the source size, so no extra PNG layers are needed.
        constexpr float artWidth = 606.0f;
        constexpr float artHeight = 586.0f;
        constexpr float centerX = 222.0f;
        constexpr float centerY = 314.0f;
        constexpr float startAngle = -93.0f;
        constexpr float sweepAngle = 148.0f;
        constexpr float radiansToDegrees = 57.2957795f;

        healthArc_.clear();
        staminaArc_.clear();
        int minX = width, minY = height, maxX = -1, maxY = -1;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x);
                const Color color = originalPixels_[index];
                if (color.a == 0) continue;

                const float dx = x * artWidth / width - centerX;
                const float dy = y * artHeight / height - centerY;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (distance < 117.0f || distance > 198.0f) continue;
                const float angle = std::atan2(dy, dx) * radiansToDegrees;
                if (angle < startAngle || angle > startAngle + sweepAngle) continue;
                const float progress = Clamp((angle - startAngle) / sweepAngle, 0.0f, 1.0f);

                const bool red = color.r > 35 && color.r > color.g * 1.55f && color.r > color.b * 1.5f;
                const bool blue = color.b > 85 && color.b > color.r + 25 && color.b > color.g + 7;
                const bool healthPixel = red && distance >= 155.0f && distance <= 198.0f;
                const bool staminaPixel = blue && distance >= 117.0f && distance <= 150.0f;
                if (healthPixel)
                    healthArc_.push_back({ index, progress });
                else if (staminaPixel)
                    staminaArc_.push_back({ index, progress });
                if (healthPixel || staminaPixel)
                {
                    minX = (std::min)(minX, x); maxX = (std::max)(maxX, x);
                    minY = (std::min)(minY, y); maxY = (std::max)(maxY, y);
                }
            }
        }

        if (maxX < minX || maxY < minY)
        {
            TraceLog(LOG_WARNING, "PLAYER HUD: No matching arc pixels found in the image.");
            std::vector<Color>().swap(originalPixels_);
            return;
        }

        // Update only the rectangle containing the arcs during sprinting.
        patchX_ = minX; patchY_ = minY;
        patchWidth_ = maxX - minX + 1; patchHeight_ = maxY - minY + 1;
        originalPatch_.resize(static_cast<std::size_t>(patchWidth_) * static_cast<std::size_t>(patchHeight_));
        for (int row = 0; row < patchHeight_; ++row)
        {
            const std::size_t src = static_cast<std::size_t>(patchY_ + row) * static_cast<std::size_t>(width) + static_cast<std::size_t>(patchX_);
            const std::size_t dst = static_cast<std::size_t>(row) * static_cast<std::size_t>(patchWidth_);
            std::copy_n(originalPixels_.data() + src, patchWidth_, originalPatch_.data() + dst);
        }
        const auto toPatchIndex = [width, this](ArcPixel& pixel)
        {
            const int x = static_cast<int>(pixel.index % static_cast<std::size_t>(width));
            const int y = static_cast<int>(pixel.index / static_cast<std::size_t>(width));
            pixel.index = static_cast<std::size_t>(y - patchY_) * static_cast<std::size_t>(patchWidth_)
                        + static_cast<std::size_t>(x - patchX_);
        };
        for (ArcPixel& pixel : healthArc_) toPatchIndex(pixel);
        for (ArcPixel& pixel : staminaArc_) toPatchIndex(pixel);
        displayPatch_ = originalPatch_;
        std::vector<Color>().swap(originalPixels_);
    }

    static void DimPixel(Color& color)
    {
        // Keep the shape visible as a dark track when its value is empty.
        color.r = static_cast<unsigned char>(color.r * 0.22f);
        color.g = static_cast<unsigned char>(color.g * 0.22f);
        color.b = static_cast<unsigned char>(color.b * 0.22f);
    }

    void RefreshArcs(int hp, int maxHp, float stamina, float maxStamina)
    {
        const float staminaRatio = maxStamina > 0.0f ? Clamp(stamina / maxStamina, 0.0f, 1.0f) : 0.0f;
        const int staminaPercent = static_cast<int>(std::round(staminaRatio * 100.0f));
        if (hp == lastHp_ && maxHp == lastMaxHp_ && staminaPercent == lastStaminaPercent_)
            return;

        const float hpRatio = maxHp > 0 ? Clamp(static_cast<float>(hp) / maxHp, 0.0f, 1.0f) : 0.0f;
        const float visibleStamina = staminaPercent / 100.0f;
        if (originalPatch_.empty()) return;
        displayPatch_ = originalPatch_;
        if (hpRatio < 1.0f)
            for (const ArcPixel& pixel : healthArc_)
                if (pixel.progress >= hpRatio) DimPixel(displayPatch_[pixel.index]);
        if (visibleStamina < 1.0f)
            for (const ArcPixel& pixel : staminaArc_)
                if (pixel.progress >= visibleStamina) DimPixel(displayPatch_[pixel.index]);

        UpdateTextureRec(texture_, Rectangle{ static_cast<float>(patchX_), static_cast<float>(patchY_),
                                              static_cast<float>(patchWidth_), static_cast<float>(patchHeight_) },
                         displayPatch_.data());
        lastHp_ = hp;
        lastMaxHp_ = maxHp;
        lastStaminaPercent_ = staminaPercent;
    }

    Texture2D texture_{};
    std::vector<Color> originalPixels_;
    std::vector<Color> originalPatch_;
    std::vector<Color> displayPatch_;
    std::vector<ArcPixel> healthArc_;
    std::vector<ArcPixel> staminaArc_;
    int patchX_ = 0;
    int patchY_ = 0;
    int patchWidth_ = 0;
    int patchHeight_ = 0;
    int lastHp_ = -1;
    int lastMaxHp_ = -1;
    int lastStaminaPercent_ = -1;
};
