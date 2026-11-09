#include "HorrorLighting.h"
#include "Stage.h"
#include "WorldScale.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>

namespace {

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

    Matrix StageTransform(const Model& model, Vector3 position, Vector3 rotation, float scale) {
        Matrix mScale = MatrixScale(scale, scale, scale);
        Matrix mRotY = MatrixRotateY(rotation.y * DEG2RAD);
        Matrix mTrans = MatrixTranslate(position.x, position.y, position.z);
        return MatrixMultiply(model.transform, MatrixMultiply(MatrixMultiply(mScale, mRotY), mTrans));
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
            if (hit.hit && hit.normal.y > 0.5f && (!closest.hit || hit.distance < closest.distance)) closest = hit;
        }
        return closest;
    }

}

void Stage::Init()
{
    Reset();
    for (const auto& sec : sections_) {
        CacheCollisionBounds(sec.modelKey, RM().GetModel(sec.modelKey));
    }
}

void Stage::Reset()
{
    modelBoundsCache_.clear();
    sections_.clear();

    // ƒfƒtƒHƒ‹ƒg‚Å Stage 1, Stage 2, Stage 3 ‚Ì3‚Â‚ð“o˜^
    StageSection s1;
    s1.name = "Stage 1";
    s1.modelKey = ResourceKeys::Model_Stage1;
    s1.position = { 0.0f, 0.0f, 0.0f };
    s1.rotation = { 0.0f, 0.0f, 0.0f };
    s1.scale = WorldScale::StageScale;
    s1.active = true;
    sections_.push_back(s1);

    StageSection s2;
    s2.name = "Stage 2";
    s2.modelKey = ResourceKeys::Model_Stage2;
    s2.position = { 0.0f, 0.0f, 0.0f };
    s2.rotation = { 0.0f, 0.0f, 0.0f };
    s2.scale = WorldScale::StageScale;
    s2.active = true;
    sections_.push_back(s2);

    StageSection s3;
    s3.name = "Stage 3";
    s3.modelKey = ResourceKeys::Model_Stage3;
    s3.position = { 0.0f, 0.0f, 0.0f };
    s3.rotation = { 0.0f, 0.0f, 0.0f };
    s3.scale = WorldScale::StageScale;
    s3.active = true;
    sections_.push_back(s3);
}

void Stage::Draw() const
{
    for (const auto& sec : sections_) {
        if (!sec.active) continue;
        Model model = ResourceManager::GetInstance().GetModel(sec.modelKey);
        if (model.meshCount > 0) {
            DrawHorrorModelEx(model, sec.position, { 0.0f, 1.0f, 0.0f }, sec.rotation.y,
                { sec.scale, sec.scale, sec.scale }, WHITE);
        }
    }
}

void Stage::End()
{
    modelBoundsCache_.clear();
    sections_.clear();
}

void Stage::CacheCollisionBounds(const std::string& key, const Model& model) const
{
    auto& cache = modelBoundsCache_[key];
    if (cache.cachedMeshes == model.meshes && cache.bounds.size() == static_cast<std::size_t>(model.meshCount)) return;
    cache.bounds.clear();
    if (model.meshes && model.meshCount > 0) {
        cache.bounds.reserve(model.meshCount);
        for (int i = 0; i < model.meshCount; ++i) {
            cache.bounds.push_back(GetMeshBoundingBox(model.meshes[i]));
        }
    }
    cache.cachedMeshes = model.meshes;
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
    if (below.hit && (!above.hit || std::fabs(below.point.y - preferred.y) <= std::fabs(above.point.y - preferred.y))) {
        preferred.y = below.point.y;
    }
    else if (above.hit) {
        preferred.y = above.point.y;
    }
    else {
        return preferred;
    }
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

    const float directionLength = Vector3Length(ray.direction);
    if (!std::isfinite(directionLength) || directionLength <= 0.0f) return closestHit;
    ray.direction = Vector3Scale(ray.direction, 1.0f / directionLength);

    for (const auto& sec : sections_) {
        if (!sec.active) continue;
        const Model stageModel = RM().GetModel(sec.modelKey);
        if (!stageModel.meshes || stageModel.meshCount <= 0) continue;
        CacheCollisionBounds(sec.modelKey, stageModel);
        const auto& boundsList = modelBoundsCache_[sec.modelKey].bounds;

        const Matrix transform = StageTransform(stageModel, sec.position, sec.rotation, sec.scale);
        const float determinant = MatrixDeterminant(transform);
        if (!std::isfinite(determinant) || determinant == 0.0f) continue;
        const Matrix inverse = MatrixInvert(transform);
        const Ray localRay = { Vector3Transform(ray.position, inverse),
            { inverse.m0 * ray.direction.x + inverse.m4 * ray.direction.y + inverse.m8 * ray.direction.z,
              inverse.m1 * ray.direction.x + inverse.m5 * ray.direction.y + inverse.m9 * ray.direction.z,
              inverse.m2 * ray.direction.x + inverse.m6 * ray.direction.y + inverse.m10 * ray.direction.z } };

        for (int i = 0; i < stageModel.meshCount; ++i) {
            if (i < static_cast<int>(boundsList.size()) && !RayReachesBounds(localRay, boundsList[i], closestHit.distance)) continue;
            const RayCollision hit = groundOnly ? RaycastGroundMesh(ray, stageModel.meshes[i], transform) :
                GetRayCollisionMesh(ray, stageModel.meshes[i], transform);
            if (hit.hit && hit.distance <= maxDistance && (!closestHit.hit || hit.distance < closestHit.distance)) {
                closestHit = hit;
            }
        }
    }
    return closestHit;
}