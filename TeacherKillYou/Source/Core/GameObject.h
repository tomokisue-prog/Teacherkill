#pragma once

#include "raylib.h"
#include <string>

enum class GameObjectType
{
    None,
    Player,
    Enemy,
    Stage,
    Item,
    Door
};

class GameObject
{
public:
    explicit GameObject(
        GameObjectType type = GameObjectType::None,
        const std::string& name = "GameObject")
        : type_(type), name_(name) {
    }

    virtual ~GameObject() = default;

    GameObjectType GetType() const { return type_; }
    const std::string& GetName() const { return name_; }

    Vector3 GetPosition() const { return position_; }
    Vector3 GetRotation() const { return rotation_; }
    Vector3 GetScale() const { return scale_; }

    void SetPosition(Vector3 position) { position_ = position; }
    void SetRotation(Vector3 rotation) { rotation_ = rotation; }
    void SetScale(Vector3 scale) { scale_ = scale; }

    bool IsActive() const { return isActive_; }
    void SetActive(bool active) { isActive_ = active; }

protected:
    GameObjectType type_{ GameObjectType::None };
    std::string name_{ "GameObject" };

    Vector3 position_{};
    Vector3 rotation_{};
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };

    bool isActive_{ true };
};