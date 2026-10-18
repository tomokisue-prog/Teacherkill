#include "Item.h"
#include "raymath.h"

Item::Item(ItemType type, Vector3 position)
    : type_(type), position_(position)
{}

const char* Item::GetName(ItemType type)
{
    switch (type)
    {
    case ItemType::Medkit:  return "MEDKIT";
    case ItemType::Ammo:    return "AMMO";
    case ItemType::KeyCard: return "KEY CARD";
    case ItemType::Battery: return "BATTERY"; 
    case ItemType::Key:     return "KEY";     
    default:                return "EMPTY";
    }
}

void Item::Draw(const Model* model, float scale) const
{
    if (collected_ || type_ == ItemType::None || model == nullptr)
        return;

    if (model->meshCount <= 0 || model->meshes == nullptr)
        return;

    DrawModel(*model, position_, scale, WHITE);
}

bool Item::IsAimedAt(const Camera3D& camera,
    float pickupRange,
    float aimRadius,
    float& forwardDistance) const
{
    if (collected_ || type_ == ItemType::None)
        return false;

    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    if (Vector3Length(forward) < 0.0001f)
        return false;

    forward = Vector3Normalize(forward);

    const Vector3 toItem = Vector3Subtract(position_, camera.position);
    forwardDistance = Vector3DotProduct(toItem, forward);

    if (forwardDistance <= 0.0f || forwardDistance > pickupRange)
        return false;

    const Vector3 perpendicular = Vector3Subtract(
        toItem, Vector3Scale(forward, forwardDistance));

    return Vector3Length(perpendicular) <= aimRadius;
}
