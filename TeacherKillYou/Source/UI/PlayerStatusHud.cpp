#include "PlayerStatusHud.h"
#include <algorithm>
#include <cmath>
#include "raymath.h"
#include <cstdio>

bool PlayerStatusHud::Init(const char* imagePath)
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

bool PlayerStatusHud::InitAmmoIcon(const char* imagePath)
{
    if (bulletTexture_.id) UnloadTexture(bulletTexture_);
    bulletTexture_ = {};
    bulletSource_ = {};
    if (!imagePath || !FileExists(imagePath)) return false;
    Image image = LoadImage(imagePath);
    if (!image.data || image.width <= 0 || image.height <= 0)
    {
        if (image.data) UnloadImage(image);
        return false;
    }
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
    {
        UnloadImage(image);
        return false;
    }
    Color* pixels = static_cast<Color*>(image.data);
    const std::size_t count = static_cast<std::size_t>(image.width) * image.height;
    for (std::size_t i = 0; i < count; ++i)
    {
        const int darkest = (std::min)(static_cast<int>(pixels[i].r),
            (std::min)(static_cast<int>(pixels[i].g), static_cast<int>(pixels[i].b)));
        const float alpha = Clamp((255.0f - darkest) / 28.0f, 0.0f, 1.0f);
        pixels[i].a = static_cast<unsigned char>(alpha * 255.0f + 0.5f);
    }
    bulletSource_ = GetImageAlphaBorder(image, 0.10f);
    if (bulletSource_.width > 0 && bulletSource_.height > 0)
        bulletTexture_ = LoadTextureFromImage(image);
    UnloadImage(image);
    if (bulletTexture_.id) SetTextureFilter(bulletTexture_, TEXTURE_FILTER_BILINEAR);
    else TraceLog(LOG_WARNING, "PLAYER HUD: Could not load ammo icon: %s", imagePath);
    return bulletTexture_.id != 0;
}

void PlayerStatusHud::End()
{
    if (texture_.id != 0) UnloadTexture(texture_);
    if (bulletTexture_.id != 0) UnloadTexture(bulletTexture_);
    bulletTexture_ = {};
    bulletSource_ = {};
    texture_ = {};
    std::vector<Color>().swap(originalPixels_);
    std::vector<Color>().swap(originalPatch_);
    std::vector<Color>().swap(displayPatch_);
    std::vector<ArcPixel>().swap(healthArc_);
    std::vector<ArcPixel>().swap(staminaArc_);
    patchX_ = patchY_ = patchWidth_ = patchHeight_ = 0;
    lastHp_ = lastMaxHp_ = lastStaminaPercent_ = -1;
}

Rectangle PlayerStatusHud::GetPanelRect() const
{
    if (!IsReady() || texture_.width <= 0) return {};
    float scale = Clamp((std::min)(GetScreenWidth() / 1920.0f,
                                   GetScreenHeight() / 1080.0f), 0.45f, 1.5f);
    const float inventoryStart = (GetScreenWidth() - 344.0f) * 0.5f;
    if (inventoryStart > 8.0f)
        scale = (std::min)(scale, (inventoryStart - 8.0f) / 501.3f);
    scale = (std::max)(0.15f, scale);
    const float width = 370.0f * scale;
    const float height = width * texture_.height / texture_.width;
    return {16.0f * scale, GetScreenHeight() - height - 16.0f * scale, width, height};
}

void PlayerStatusHud::DrawAmmoRow(int ammo, int reserve, Rectangle healthPanel, bool isMainArtwork) const
{
    const float screenW = static_cast<float>((std::max)(GetScreenWidth(), 1));
    const float screenH = static_cast<float>((std::max)(GetScreenHeight(), 1));
    const float scale = Clamp(healthPanel.width / 370.0f, 0.15f, 1.5f);
    constexpr int magazineSize = 8;
    const float bulletHeight = 22.0f * scale; // CHANGE: bullet picture size.
    const float aspect = bulletTexture_.id && bulletSource_.height > 0.0f
        ? bulletSource_.width / bulletSource_.height : 0.42f;
    const float bulletWidth = bulletHeight * aspect;
    const float spacing = 14.0f * scale; // CHANGE: space between bullets.
    const int fontSize = (std::max)(9, static_cast<int>(16.0f * scale));
    const int safeReserve = (std::max)(0, reserve);
    const int magazines = safeReserve / magazineSize;
    const int looseRounds = safeReserve % magazineSize;
    char multiplier[48];
    if (looseRounds > 0)
        std::snprintf(multiplier, sizeof(multiplier), "x%d+%d", magazines, looseRounds);
    else
        std::snprintf(multiplier, sizeof(multiplier), "x%d", magazines);

    // The HP_Bar.png design has empty space below its circle at y=482/586.
    // Reserve eight fixed slots so the multiplier does not move while firing.
    const float slotsWidth = 7.0f * spacing + bulletWidth;
    const float textGap = 7.0f * scale;
    const float baseTextWidth = static_cast<float>(MeasureText("x4", fontSize));
    const float centerX = isMainArtwork
        ? healthPanel.x + healthPanel.width * (222.0f / 606.0f)
        : healthPanel.x + healthPanel.width * 0.5f;
    float startX = centerX - (slotsWidth + textGap + baseTextWidth) * 0.5f;
    startX = Clamp(startX, 0.0f, (std::max)(0.0f, screenW - slotsWidth - textGap - baseTextWidth));
    float y = isMainArtwork
        ? healthPanel.y + healthPanel.height * (482.0f / 586.0f)
        : healthPanel.y + healthPanel.height + 6.0f * scale;
    y = Clamp(y, 0.0f, (std::max)(0.0f, screenH - bulletHeight - 2.0f));

    const int loaded = (std::max)(0, (std::min)(ammo, magazineSize));
    for (int i = 0; i < loaded; ++i)
    {
        const Rectangle dest{startX + i * spacing, y, bulletWidth, bulletHeight};
        if (bulletTexture_.id && bulletSource_.width > 0 && bulletSource_.height > 0)
            DrawTexturePro(bulletTexture_, bulletSource_, dest, {0, 0}, 0.0f, WHITE);
        else
            DrawRectangleRounded(dest, 0.5f, 5, Color{209, 163, 72, 255});
    }
    const float textX = startX + slotsWidth + textGap;
    const float textY = y + (bulletHeight - fontSize) * 0.5f;
    DrawText(multiplier, static_cast<int>(textX), static_cast<int>(textY),
             fontSize, Color{242, 237, 220, 255});
}

void PlayerStatusHud::Draw(int hp, int maxHp, float stamina, float maxStamina)
{
    if (!IsReady()) return;
    RefreshArcs(hp, maxHp, stamina, maxStamina);
    const Rectangle imageRect = GetPanelRect();
    const Rectangle source{0.0f, 0.0f, static_cast<float>(texture_.width), static_cast<float>(texture_.height)};
    DrawTexturePro(texture_, source, imageRect, {0.0f, 0.0f}, 0.0f, WHITE);
}

void PlayerStatusHud::BuildArcMasks(int width, int height)
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

void PlayerStatusHud::DimPixel(Color& color)
{
    // Keep the shape visible as a dark track when its value is empty.
    color.r = static_cast<unsigned char>(color.r * 0.22f);
    color.g = static_cast<unsigned char>(color.g * 0.22f);
    color.b = static_cast<unsigned char>(color.b * 0.22f);
}

void PlayerStatusHud::RefreshArcs(int hp, int maxHp, float stamina, float maxStamina)
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
