#include "Item.h"
#include "raymath.h"
#include "ResourceManager.h"
#include "HorrorLighting.h"
#include <algorithm>

namespace {
    struct ItemModel {
        const char* key;
        float size;
    };

    ItemModel ModelFor(ItemType type) {
        switch (type) {
        case ItemType::Medkit: return { ResourceKeys::Model_Medkit, .65f };
        case ItemType::Ammo: return { ResourceKeys::Model_Ammo, .55f };
        case ItemType::KeyCard: return { ResourceKeys::Model_KeyCard, .47f };
        case ItemType::LostItem: return { ResourceKeys::Model_LostItem, .40f };
        case ItemType::Gun: return { ResourceKeys::Model_Gun, .52f };
        case ItemType::AuthenticationKey: return { ResourceKeys::Model_Authentication_Key, .47f };
        case ItemType::EmergencyStairsKey: return { ResourceKeys::Model_Authentication_Key, .47f };
        case ItemType::Books: return { ResourceKeys::Model_Books, .58f };
        case ItemType::Chair1: return { ResourceKeys::Model_Chair1, 1.55f };
        case ItemType::Chair2: return { ResourceKeys::Model_Chair2, 1.4f };
        case ItemType::Computer: return { ResourceKeys::Model_Computer, .90f };
        case ItemType::Locker: return { ResourceKeys::Model_Locker, 3.0f };
        case ItemType::Whiteboard: return { ResourceKeys::Model_Whiteboard, 3.1f };
        case ItemType::TrashBin: return { ResourceKeys::Model_TrashBin, .85f };
        case ItemType::Table: return { ResourceKeys::Model_Table, 1.8f };
        case ItemType::HorrorTable: return { ResourceKeys::Model_HorrorTable, 2.6f };
        case ItemType::SlideDoorProp: return { ResourceKeys::Model_SlideDoorProp, 3.6f };
        case ItemType::Crowbar: return { ResourceKeys::Model_Crowbar, 1.0f };
        default: return {};
        }
    }

    float ModelScale(ItemModel asset, Vector3 size, float multiplier) {
        const float longest = (std::max)({ size.x, size.y, size.z });
        return longest > .001f ? asset.size / longest * multiplier : multiplier;
    }
}

Item::Item(ItemType type, Vector3 position)
    : type_(type), position_(position)
{
}

const char* Item::GetName(ItemType type)
{
    switch (type)
    {
    case ItemType::Medkit:  return "MEDKIT";
    case ItemType::Ammo:    return "AMMO";
    case ItemType::KeyCard: return "KEY CARD";
    case ItemType::Battery: return "BATTERY";
    case ItemType::Key:     return "KEY";
    case ItemType::LostItem: return "LOST ITEM";
    case ItemType::Gun:     return "GUN";
    case ItemType::AuthenticationKey: return "AUTHENTICATION KEY";
    case ItemType::EmergencyStairsKey: return "SERVICE ELEVATOR KEY";
    case ItemType::Crowbar: return "CROWBAR";
    case ItemType::Books: return "BOOKS";
    case ItemType::Chair1: return "CHAIR 1";
    case ItemType::Chair2: return "CHAIR 2";
    case ItemType::Computer: return "COMPUTER";
    case ItemType::Locker: return "LOCKER";
    case ItemType::Whiteboard: return "WHITEBOARD";
    case ItemType::TrashBin: return "TRASH BIN";
    case ItemType::Table: return "TABLE";
    case ItemType::HorrorTable: return "HORROR TABLE";
    case ItemType::SlideDoorProp: return "SLIDE DOOR (PROP)";
    default:                return "EMPTY";
    }
}

void Item::Draw() const
{
    if (collected_) return;
    const auto asset = ModelFor(type_);
    if (!asset.key) return;
    Model model = RM().GetModel(asset.key);
    if (model.meshCount <= 0 || !model.meshes) return;

    const auto bounds = RM().GetModelBounds(asset.key);
    const float baseScale = ModelScale(asset, Vector3Subtract(bounds.max, bounds.min), scale_);

    const Matrix pivot = MatrixTranslate(
        -(bounds.min.x + bounds.max.x) * 0.5f,
        -bounds.min.y,
        -(bounds.min.z + bounds.max.z) * 0.5f
    );

    const Matrix matScale = MatrixScale(baseScale, baseScale, baseScale);
    const Matrix matRotX = MatrixRotateX(rotation_.x * DEG2RAD);
    const Matrix matRotY = MatrixRotateY(rotation_.y * DEG2RAD);
    const Matrix matRotZ = MatrixRotateZ(rotation_.z * DEG2RAD);
    const Matrix matRot = MatrixMultiply(MatrixMultiply(matRotX, matRotY), matRotZ);
    const Matrix matTrans = MatrixTranslate(position_.x, position_.y, position_.z);

    const Matrix fullTransform = MatrixMultiply(
        MatrixMultiply(model.transform, pivot),
        MatrixMultiply(MatrixMultiply(matScale, matRot), matTrans)
    );

    for (int i = 0; i < model.meshCount; ++i)
    {
        DrawHorrorMesh(model.meshes[i], model.materials[model.meshMaterial[i]], fullTransform);
    }
}

RayCollision Item::RaycastPlacement(Ray ray) const
{
    const auto asset = ModelFor(type_);
    Vector3 size{ .35f, .35f, .35f };
    if (asset.key) {
        const auto bounds = RM().GetModelBounds(asset.key);
        const auto dimensions = Vector3Subtract(bounds.max, bounds.min);
        size = Vector3Scale(dimensions, ModelScale(asset, dimensions, scale_));
    }
    if (IsPickup()) size = Vector3Max(size, { .30f, .25f, .30f });

    const Matrix matRotX = MatrixRotateX(rotation_.x * DEG2RAD);
    const Matrix matRotY = MatrixRotateY(rotation_.y * DEG2RAD);
    const Matrix matRotZ = MatrixRotateZ(rotation_.z * DEG2RAD);
    const Matrix matRot = MatrixMultiply(MatrixMultiply(matRotX, matRotY), matRotZ);
    const Matrix inverse = MatrixInvert(matRot);

    const Ray local{
        Vector3Transform(Vector3Subtract(ray.position, position_), inverse),
        Vector3Transform(ray.direction, inverse)
    };

    auto hit = GetRayCollisionBox(local, {
        { -size.x * 0.5f, 0.0f, -size.z * 0.5f },
        {  size.x * 0.5f, size.y,  size.z * 0.5f }
        });

    if (hit.hit) {
        hit.point = Vector3Add(ray.position, Vector3Scale(ray.direction, hit.distance));
        hit.normal = Vector3Transform(hit.normal, matRot);
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