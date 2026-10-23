#include "GameFade.h"

GameFade::GameFade() : state_(FadeState::Idle), alpha_(0.0f), speed_(2.0f), isMidpointTriggered_(false) {}

void GameFade::StartFade(float speed)
{
    if (state_ != FadeState::Idle) return;
    speed_ = speed;
    state_ = FadeState::FadeOut;
    alpha_ = 0.0f;
    isMidpointTriggered_ = false;
}

void GameFade::Update(float deltaTime)
{
    if (state_ == FadeState::Idle) return;

    if (state_ == FadeState::FadeOut)
    {
        alpha_ += speed_ * deltaTime;
        if (alpha_ >= 1.0f)
        {
            alpha_ = 1.0f;
            isMidpointTriggered_ = true; // ê^Ç¡à√Ç…Ç»Ç¡ÇΩçáê}
            state_ = FadeState::Holding;
        }
    }
    else if (state_ == FadeState::Holding)
    {
        state_ = FadeState::FadeIn;
    }
    else if (state_ == FadeState::FadeIn)
    {
        alpha_ -= speed_ * deltaTime;
        if (alpha_ <= 0.0f)
        {
            alpha_ = 0.0f;
            state_ = FadeState::Idle;
        }
    }
}

void GameFade::Draw() const
{
    if (alpha_ <= 0.0f) return;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, alpha_));
}

void GameFade::Reset()
{
    state_ = FadeState::Idle;
    alpha_ = 0.0f;
    isMidpointTriggered_ = false;
}