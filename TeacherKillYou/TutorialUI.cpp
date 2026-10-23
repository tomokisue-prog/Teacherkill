
#include "TutorialUI.h"
#include "ResourceManager.h"
#include "JapaneseFont.h"
#include <algorithm>

// =======================================
// Initialize
// =======================================
void TutorialUI::Init()
{
    End();
    Reset();

    const char* path =
        "Data/Image/UI/WASD_Tutorial_UI.png";

    if (!FileExists(path))
    {
        TraceLog(LOG_WARNING,
            "Tutorial PNG not found: %s", path);
        return;
    }

    wasdTexture_ = LoadTexture(path);

    if (wasdTexture_.id != 0)
    {
        SetTextureFilter(
            wasdTexture_,
            TEXTURE_FILTER_BILINEAR
        );
    }
}

// =======================================
// Reset
// =======================================
void TutorialUI::Reset()
{
    movementCompleted_ = false;
    movementAlpha_ = 1.0f;

	
	missionTitle_ = u8"教室に向かおう";
	missionDetail_ = u8"忘れ物を探そう";
}

// =======================================
// Update
// =======================================
void TutorialUI::Update(float deltaTime)
{
    // Detect player movement input
    if (!movementCompleted_)
    {
        if (IsKeyDown(KEY_W) ||
            IsKeyDown(KEY_A) ||
            IsKeyDown(KEY_S) ||
            IsKeyDown(KEY_D))
        {
            movementCompleted_ = true;
        }
    }

    // Fade out after movement
    if (movementCompleted_)
    {
        movementAlpha_ = std::max(
            0.0f,
            movementAlpha_ -
            std::max(deltaTime, 0.0f) * 2.0f
        );
    }
}

// =======================================
// Update mission text
// =======================================
void TutorialUI::SetMission(
    const std::string& title,
    const std::string& detail)
{
    missionTitle_ = title;
    missionDetail_ = detail;
}

// =======================================
// Draw WASD Tutorial
// =======================================
void TutorialUI::DrawMovementTutorial() const
{
    if (movementAlpha_ <= 0.0f)
        return;

    // Fallback text if PNG is missing
    if (wasdTexture_.id == 0)
    {
        const char* text = "W A S D - MOVE";

        int centerX = GetScreenWidth() / 2;
        int centerY = GetScreenHeight() / 2;

        DrawText(
            text,
            centerX - MeasureText(text, 24) / 2,
            centerY,
            24,
            Fade(WHITE, movementAlpha_)
        );
        return;
    }

    // Tutorial image size
    const float width = std::min(
        380.0f,
        GetScreenWidth() * 0.70f
    );

    const float height = width *
        (float)wasdTexture_.height /
        (float)wasdTexture_.width;

    // Center on screen
    const float x =
        (GetScreenWidth() - width) * 0.5f;

    const float y =
        (GetScreenHeight() - height) * 0.5f;

    DrawTexturePro(
        wasdTexture_,
        {
            0.0f,
            0.0f,
            (float)wasdTexture_.width,
            (float)wasdTexture_.height
        },
        { x, y, width, height },
        { 0.0f, 0.0f },
        0.0f,
        Fade(WHITE, movementAlpha_)
    );
}

// =======================================
// Draw Mission HUD
// =======================================
void TutorialUI::DrawMissionHUD() const
{
    const int width = 420;
    const int height = 136;

    const int x =
        GetScreenWidth() - width - 25;

    const int y = 30;

    const auto& jpFont = JPFont();
    if (jpFont.IsLoaded())
    {

        // Dark background
        DrawRectangle(
            x, y, width, height,
            Color{ 18, 24, 29, 215 }
        );

        // Border
        DrawRectangleLinesEx(
            {
                (float)x,
                (float)y,
                (float)width,
                (float)height
            },
            1.0f,
            Color{ 100, 115, 120, 255 }
        );

        // Small accent on left
        DrawRectangle(
            x, y, 4, height,
            Color{ 117, 183, 185, 255 }
        );

        // Mission header
        jpFont.DrawText(u8"クエスト", x + 16, y + 11, 18, { 220, 220, 220, 255 });

        // Mission title

        jpFont.DrawText(
            missionTitle_.c_str(),
            x + 16,
            y + 39,
            18,
            {255, 255, 255, 255}
        );

        // Mission description
        jpFont.DrawText(
            missionDetail_.c_str(),
            x + 16,
            y + 68,
            14,
            {200, 200, 200, 255}
        );
    }
}

// =======================================
// Draw all UI
// =======================================
void TutorialUI::Draw() const
{
    DrawMissionHUD();
    DrawMovementTutorial();
}

// =======================================
// Release textures
// =======================================
void TutorialUI::End()
{
    if (wasdTexture_.id != 0)
    {
        UnloadTexture(wasdTexture_);
        wasdTexture_ = {};
    }
}
