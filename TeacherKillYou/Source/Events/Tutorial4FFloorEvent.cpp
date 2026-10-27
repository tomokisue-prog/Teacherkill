#include "Tutorial4FFloorEvent.h"
#include "GameContext.h"
#include "ResourceManager.h"
#include "raymath.h"

void Tutorial4FFloorEvent::Init()
{
    isFinished_ = false;
    hasMoved_ = false;
    currentPhase_ = Phase::OpeningDialogue;
    fadeTimer_ = 0.0f;
    markerAnimTimer_ = 0.0f;
}

void Tutorial4FFloorEvent::Update(float deltaTime)
{
    switch (currentPhase_)
    {
    case Phase::OpeningDialogue:
        currentPhase_ = Phase::WaitWASD;
        fadeTimer_ = 0.0f;
        break;

    case Phase::WaitWASD:
        fadeTimer_ += deltaTime;
        if (fadeTimer_ > fadeDuration_)
        {
            fadeTimer_ = fadeDuration_;
        }

        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_A) ||
            IsKeyPressed(KEY_S) || IsKeyPressed(KEY_D))
        {
            hasMoved_ = true;
            currentPhase_ = Phase::FadeOut;
            fadeTimer_ = 0.0f;
        }
        break;

    case Phase::FadeOut:
        fadeTimer_ += deltaTime;
        if (fadeTimer_ >= fadeDuration_)
        {
            currentPhase_ = Phase::MissionActive;
            fadeTimer_ = 0.0f;
        }
        break;

    case Phase::MissionActive:
        markerAnimTimer_ += deltaTime;

        break;
    }
}

void Tutorial4FFloorEvent::Draw() const
{
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    if (currentPhase_ == Phase::WaitWASD || currentPhase_ == Phase::FadeOut)
    {
        int centerX = screenWidth / 2;
        int centerY = screenHeight / 2 - 30;

        const Texture2D* wasdTexPtr = RM().GetTexture(ResourceKeys::WASD_UI);
        if (wasdTexPtr != nullptr && wasdTexPtr->id != 0)
        {
            const Texture2D& wasdTex = *wasdTexPtr;
            int drawX = centerX - (wasdTex.width / 2);
            int drawY = centerY - (wasdTex.height / 2);

            float alpha = 0.0f;
            if (currentPhase_ == Phase::WaitWASD)
            {
                alpha = Clamp(fadeTimer_ / fadeDuration_, 0.0f, 1.0f);
            }
            else if (currentPhase_ == Phase::FadeOut)
            {
                alpha = Clamp(1.0f - (fadeTimer_ / fadeDuration_), 0.0f, 1.0f);
            }

            Color tintColor = Fade(WHITE, alpha);
            DrawTexture(wasdTex, drawX, drawY, tintColor);
        }
    }

}