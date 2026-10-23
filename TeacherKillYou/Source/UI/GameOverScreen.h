#pragma once
enum class GameOverAction { None, Retry, Title, Checkpoint };

class GameOverScreen
{
public:
    void Reset();
    void SetCheckpointAvailable(bool available);
    GameOverAction Update(float deltaTime);
    void Draw() const;

private:
    bool checkpointAvailable_ = false;
    float elapsed_ = 0.0f;
    int selectedOption_ = 0;
};
