#pragma once

#include "raylib.h"

enum class GameOverAction { None, Retry, Title, Checkpoint };

class GameOverScreen
{
public:
    void Init();
    void End();
    void Reset();
    void SetCheckpointAvailable(bool available);
    GameOverAction Update(float deltaTime);;
    void Draw() const;

private:
    Texture2D image_{};
    bool checkpointAvailable_ = false;
    float elapsed_ = 0.0f;
    int selectedOption_ = 0;
};
