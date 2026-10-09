#include "Stage.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace {

// Conservative slab test, including rays starting inside a box and flat meshes.
bool RayReachesBounds(Ray ray, BoundingBox bounds, float maxDistance) {
    constexpr float boundsPadding = 0.0001f;
    const float origins[] = { ray.position.x, ray.position.y, ray.position.z };
    const float directions[] = { ray.direction.x, ray.direction.y, ray.direction.z };
    const float minimums[] = { bounds.min.x, bounds.min.y, bounds.min.z };
    const float maximums[] = { bounds.max.x, bounds.max.y, bounds.max.z };
    float nearDistance = 0.0f;
    float farDistance = maxDistance;
    for (int axis = 0; axis < 3; ++axis) {
        const float minimum = minimums[axis] - boundsPadding;
        const float maximum = maximums[axis] + boundsPadding;
        if (directions[axis] == 0.0f) {
            if (origins[axis] < minimum || origins[axis] > maximum) return false;
            continue;
        }
        const float a = (minimum - origins[axis]) / directions[axis];
        const float b = (maximum - origins[axis]) / directions[axis];
        nearDistance = (std::max)(nearDistance, (std::min)(a, b));
        farDistance = (std::min)(farDistance, (std::max)(a, b));
        if (nearDistance > farDistance) return false;
    }
    return true;
}

// Match DrawModel: model transform, then uniform scale, then stage translation.
Matrix StageTransform(const Model& model, Vector3 position, float scale) {
    return MatrixMultiply(model.transform, MatrixMultiply(MatrixScale(scale, scale, scale),
        MatrixTranslate(position.x, position.y, position.z)));
}

RayCollision RaycastGroundMesh(Ray ray, const Mesh& mesh, Matrix transform) {
    RayCollision closest = {};
    if (!mesh.vertices) return closest;
    for (int triangle = 0; triangle < mesh.triangleCount; ++triangle) {
        Vector3 points[3];
        for (int corner = 0; corner < 3; ++corner) {
            const int index = mesh.indices ? mesh.indices[triangle * 3 + corner] : triangle * 3 + corner;
            points[corner] = Vector3Transform({ mesh.vertices[index * 3], mesh.vertices[index * 3 + 1],
                mesh.vertices[index * 3 + 2] }, transform);
        }
        const RayCollision hit = GetRayCollisionTriangle(ray, points[0], points[1], points[2]);
        // Filter each triangle, so a ceiling in the same mesh cannot hide the floor.
        if (hit.hit && hit.normal.y > 0.5f && (!closest.hit || hit.distance < closest.distance)) closest = hit;
    }
    return closest;
}

} // namespace

void Stage::Init()
{
    Reset();
    CacheCollisionBounds(RM().GetModel(ResourceKeys::Model_Stage1));
}

void Stage::Reset()
{
    position_ = { 0.0f, 0.0f, 0.0f };
    scale_ = 1.0f;
    cachedMeshes_ = nullptr;
    meshBounds_.clear();
}

void Stage::Draw() const
{
    Model stageModel = ResourceManager::GetInstance().GetModel(ResourceKeys::Model_Stage1);
    DrawModel(stageModel, position_, scale_, WHITE);
}

void Stage::End()
{
    cachedMeshes_ = nullptr;
    meshBounds_.clear();
}

void Stage::CacheCollisionBounds(const Model& model) const
{
    if (cachedMeshes_ == model.meshes && meshBounds_.size() == static_cast<std::size_t>(model.meshCount)) return;
    meshBounds_.clear();
    if (model.meshes && model.meshCount > 0) {
        meshBounds_.reserve(model.meshCount);
        for (int i = 0; i < model.meshCount; ++i) {
            meshBounds_.push_back(GetMeshBoundingBox(model.meshes[i]));
        }
    }
    cachedMeshes_ = model.meshes;
}

RayCollision Stage::Raycast(Ray ray) const
{
    return Raycast(ray, 999999.0f);
}

RayCollision Stage::Raycast(Ray ray, float maxDistance) const
{
    return RaycastSurfaces(ray, maxDistance, false);
}

RayCollision Stage::RaycastGround(Ray ray, float maxDistance) const
{
    return RaycastSurfaces(ray, maxDistance, true);
}

Vector3 Stage::FindGroundPosition(Vector3 preferred) const
{
    constexpr float margin = 0.01f;
    const RayCollision below = RaycastGround({ Vector3Add(preferred, { 0, margin, 0 }), { 0, -1, 0 } });
    const RayCollision above = RaycastGround({ Vector3Subtract(preferred, { 0, margin, 0 }), { 0, 1, 0 } });
    // Keep the requested X/Z and select the closest walkable floor to its Y.
    if (below.hit && (!above.hit || std::fabs(below.point.y - preferred.y) <= std::fabs(above.point.y - preferred.y))) {
        preferred.y = below.point.y;
    } else if (above.hit) {
        preferred.y = above.point.y;
    } else {
        return preferred;
    }
    // Resolve nearby overlapping floor layers just as the character ground probe does.
    constexpr float probeHeight = 0.25f;
    const RayCollision surface = RaycastGround({ Vector3Add(preferred, { 0, probeHeight, 0 }), { 0, -1, 0 } },
        probeHeight + margin);
    if (surface.hit) preferred.y = surface.point.y;
    return preferred;
}

RayCollision Stage::RaycastSurfaces(Ray ray, float maxDistance, bool groundOnly) const
{
    RayCollision closestHit = {};
    closestHit.distance = maxDistance;
    if (std::isnan(maxDistance) || maxDistance < 0.0f) return closestHit;
    const Model stageModel = RM().GetModel(ResourceKeys::Model_Stage1);
    if (!stageModel.meshes || stageModel.meshCount <= 0) return closestHit;
    CacheCollisionBounds(stageModel);

    const float directionLength = Vector3Length(ray.direction);
    if (!std::isfinite(directionLength) || directionLength <= 0.0f) return closestHit;
    ray.direction = Vector3Scale(ray.direction, 1.0f / directionLength);
    const Matrix transform = StageTransform(stageModel, position_, scale_);
    const float determinant = MatrixDeterminant(transform);
    if (!std::isfinite(determinant) || determinant == 0.0f) return closestHit;
    const Matrix inverse = MatrixInvert(transform);
    // Do not normalize the local direction: its ray parameter must remain in world metres.
    const Ray localRay = { Vector3Transform(ray.position, inverse),
        { inverse.m0 * ray.direction.x + inverse.m4 * ray.direction.y + inverse.m8 * ray.direction.z,
          inverse.m1 * ray.direction.x + inverse.m5 * ray.direction.y + inverse.m9 * ray.direction.z,
          inverse.m2 * ray.direction.x + inverse.m6 * ray.direction.y + inverse.m10 * ray.direction.z } };
    for (int i = 0; i < stageModel.meshCount; ++i) {
        if (!RayReachesBounds(localRay, meshBounds_[i], closestHit.distance)) continue;
        const RayCollision hit = groundOnly ? RaycastGroundMesh(ray, stageModel.meshes[i], transform) :
            GetRayCollisionMesh(ray, stageModel.meshes[i], transform);
        if (hit.hit && hit.distance <= maxDistance && (!closestHit.hit || hit.distance < closestHit.distance)) {
            closestHit = hit;
        }
    }
    return closestHit;
}
