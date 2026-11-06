#include "Item.h"
#include "ItemCatalog.h"
#include "raymath.h"
#include "ResourceManager.h"
#include "HorrorLighting.h"
#include <algorithm>

namespace {
    float ModelScale(const ItemDefinition& asset, Vector3 size, float multiplier) {
        const float longest = (std::max)({size.x, size.y, size.z});
        return longest > .001f ? asset.worldSize / longest * multiplier : multiplier;
    }
}

Item::Item(ItemType type, Vector3 position)
    : type_(type), position_(position)
{
}

const char* Item::GetName(ItemType type)
{
    return GetItemDefinition(type).name;
}

bool Item::IsPickup() const
{
    return GetItemDefinition(type_).pickup;
}

void Item::Draw() const
{
    if (collected_) return;
    const auto asset = GetItemDefinition(type_);
    if (!asset.modelKey) return;
    Model model = RM().GetModel(asset.modelKey);
    if (model.meshCount <= 0 || !model.meshes) return;

    const auto bounds = RM().GetModelBounds(asset.modelKey);
    const float scale = ModelScale(asset, Vector3Subtract(bounds.max, bounds.min), scale_);
    // Use the bottom centre as the anchor so floor and tabletop clicks align with the model.
    model.transform = MatrixMultiply(model.transform, MatrixTranslate(
        -(bounds.min.x + bounds.max.x) * .5f, -bounds.min.y, -(bounds.min.z + bounds.max.z) * .5f));
    DrawHorrorModelEx(model, position_, {0, 1, 0}, yaw_ * RAD2DEG, {scale, scale, scale});
}

RayCollision Item::RaycastPlacement(Ray ray) const
{
    const auto asset = GetItemDefinition(type_);
    Vector3 size{.35f, .35f, .35f};
    if (asset.modelKey) {
        const auto bounds = RM().GetModelBounds(asset.modelKey);
        const auto dimensions = Vector3Subtract(bounds.max, bounds.min);
        size = Vector3Scale(dimensions, ModelScale(asset, dimensions, scale_));
    }
    if (IsPickup()) size = Vector3Max(size, {.30f, .25f, .30f});
    const auto inverse = MatrixRotateY(-yaw_);
    const Ray local{Vector3Transform(Vector3Subtract(ray.position, position_), inverse),
        Vector3Transform(ray.direction, inverse)};
    auto hit = GetRayCollisionBox(local, {{-size.x * .5f, 0, -size.z * .5f},
                                         { size.x * .5f, size.y, size.z * .5f}});
    if (hit.hit) {
        hit.point = Vector3Add(ray.position, Vector3Scale(ray.direction, hit.distance));
        hit.normal = Vector3Transform(hit.normal, MatrixRotateY(yaw_));
    }
    return hit;
}

bool Item::IsAimedAt(const Camera3D& camera, float pickupRange, float aimRadius, float& forwardDistance) const
{
    forwardDistance = 0.0f;
    if (collected_ || !IsPickup())
        return false;

    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    if (Vector3LengthSqr(forward) < 0.000001f)
        return false;

    forward = Vector3Normalize(forward);
    const Vector3 toItem = Vector3Subtract(position_, camera.position);

    if (Vector3Length(toItem) > pickupRange)
        return false;

    forwardDistance = Vector3DotProduct(toItem, forward);
    if (forwardDistance <= 0.0f)
        return false;

    const Vector3 closestPoint = Vector3Add(camera.position, Vector3Scale(forward, forwardDistance));

    return Vector3Distance(closestPoint, position_) <= aimRadius;
}