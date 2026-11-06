#pragma once
#include "raylib.h"
#include <vector>

class Stage
{
public:
    Stage() = default;
    ~Stage() = default;

    void Init();
    void Reset();
    void Draw() const;
    void End();

    // レイキャストを実行し、衝突情報（ヒットした座標や距離）を返す
    RayCollision Raycast(Ray ray) const;
    // Limit collision work to the portion of the ray needed by the caller.
    RayCollision Raycast(Ray ray, float maxDistance) const;

    // 上向きの面だけを床として判定し、同じメッシュ内の天井を除外する。
    RayCollision RaycastGround(Ray ray, float maxDistance = 999999.0f) const;
    // 希望するX/Zを保持し、最も近い床の高さへ初期配置する。床がなければ元の位置を返す。
    Vector3 FindGroundPosition(Vector3 preferred) const;

    Vector3 GetPosition() const { return position_; }
    void SetPosition(Vector3 pos) { position_ = pos; }

    float GetScale() const { return scale_; }
    void SetScale(float scale) { scale_ = scale; }

private:
    RayCollision RaycastSurfaces(Ray ray, float maxDistance, bool groundOnly) const;
    void CacheCollisionBounds(const Model& model) const;
    mutable const Mesh* cachedMeshes_ = nullptr;
    mutable std::vector<BoundingBox> meshBounds_;

    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    float scale_{ 1.0f };
};