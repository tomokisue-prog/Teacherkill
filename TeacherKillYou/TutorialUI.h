
#pragma once

#include "raylib.h"
#include <string>

class TutorialUI
{
public:
    void Init();
    void Reset();
    void Update(float deltaTime);
    void Draw() const;
    void End();

    void SetMission(
        const std::string& title,
        const std::string& detail
    );

private:
    // WASD tutorial image
    Texture2D wasdTexture_{};

    // Movement tutorial
    bool movementCompleted_ = false;
    float movementAlpha_ = 1.0f;

    // Current mission
    std::string missionTitle_ =
        u8"教室に向かおう";

    std::string missionDetail_ =
        u8"忘れ物を探そう";

    void DrawMovementTutorial() const;
    void DrawMissionHUD() const;
};
