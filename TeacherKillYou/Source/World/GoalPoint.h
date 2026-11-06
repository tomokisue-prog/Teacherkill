#pragma once
#include "GameObject.h"

class GoalPoint : public GameObject
{
public:
    GoalPoint() : GameObject(GameObjectType::Stage, "GoalPoint") {}
    ~GoalPoint() override = default;

    void Init();
    void Reset();
    void Draw() const;

    float GetRadius() const { return radius_; }
    void SetRadius(float r) { radius_ = r; }

private:
    float radius_{ 1.5f }; // ÚG”»’è‚Ì”¼Œa
};