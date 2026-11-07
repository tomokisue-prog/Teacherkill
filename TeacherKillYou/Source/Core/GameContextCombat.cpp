#include "GameContext.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

void GameContext::ProcessPlayerShot()
{
    if (!player_.HasGun()) return;
    if (!player_.ConsumeShot()) return;
    if (!player_.IsAlive()) return;

    if (cameraController_.GetActiveType() != CameraType::Player)
    {
        return;
    }

    constexpr float shotRange = 100.0f;
    constexpr int shotDamage = 25;

    const Camera3D camera =
        cameraController_.GetPlayerCamera().GetRaylibCamera();

    const Ray ray = {
        camera.position,
        Vector3Normalize(Vector3Subtract(camera.target, camera.position))
    };

    float closestDistance = shotRange;

    const RayCollision wallHit = stage_.Raycast(ray, shotRange);
    if (wallHit.hit)
    {
        closestDistance = wallHit.distance;
    }

    demo::EnemyId targetId = 0;
    bool foundTarget = false;
#ifndef NDEBUG
    int targetHpBefore = 0;
#endif
    Vector3 impactNormal = Vector3Negate(ray.direction);

    for (const auto& enemy : enemyManager_.GetEnemies())
    {
        if (!enemy.IsAlive()) continue;

        const BoundingBox bounds = enemy.GetBodyBounds();
        RayCollision hit = GetRayCollisionBox(ray, bounds);

        const bool inside =
            ray.position.x >= bounds.min.x &&
            ray.position.x <= bounds.max.x &&
            ray.position.y >= bounds.min.y &&
            ray.position.y <= bounds.max.y &&
            ray.position.z >= bounds.min.z &&
            ray.position.z <= bounds.max.z;

        if (inside)
        {
            hit.hit = true;
            hit.distance = 0.0f;
        }

        if (hit.hit &&
            hit.distance >= 0.0f &&
            hit.distance < closestDistance)
        {
            closestDistance = hit.distance;
            targetId = enemy.GetId();
#ifndef NDEBUG
            targetHpBefore = enemy.GetHp();
#endif
            foundTarget = true;
            impactNormal = inside ? Vector3Negate(ray.direction) : hit.normal;
        }
    }

    muzzleFlashTimer_ = 0.06f;
    muzzleFlashFrame_ = GetRandomValue(0, 3);

    tracerStart_ = player_.GetMuzzlePosition(camera);
    tracerEnd_ = Vector3Add(
        ray.position,
        Vector3Scale(ray.direction, closestDistance));
    tracerAge_ = 0.0f;
    tracerActive_ = true;

    if (wallHit.hit && !foundTarget)
    {
        if (std::fabs(wallHit.normal.y) < 0.65f &&
            Vector3LengthSqr(wallHit.normal) > 0.0001f)
        {
            BulletMark mark;

            mark.position = wallHit.point;
            mark.normal = wallHit.normal;
            mark.remainingTime = bulletMarkLifetime_;

            if (bulletMarks_.size() >=
                static_cast<std::vector<BulletMark>::size_type>(maxBulletMarks_))
            {
                bulletMarks_.erase(bulletMarks_.begin());
            }

            bulletMarks_.push_back(mark);
        }
    }

#ifndef NDEBUG
    DebugShot debugShot;
    debugShot.ray=ray;debugShot.end=tracerEnd_;
    debugShot.normal=foundTarget?impactNormal:wallHit.hit?wallHit.normal:Vector3{};
    debugShot.target=foundTarget?DebugShotTarget::Enemy:wallHit.hit?DebugShotTarget::Wall:DebugShotTarget::Miss;
    debugShot.enemyId=targetId;
#endif
    if (!foundTarget)
    {
#ifndef NDEBUG
        debugGameplay_.RecordShot(debugShot);
#endif
        TraceLog(LOG_INFO, "SHOT: No enemy hit");
        return;
    }

    const demo::EnemyDamageResult result =
        enemyManager_.ApplyDamage(targetId, shotDamage);
#ifndef NDEBUG
    debugShot.damage=result.applied?targetHpBefore-result.hp:0;
    debugShot.hpAfter=result.hp;debugShot.killed=result.becameDead;
    debugGameplay_.RecordShot(debugShot);
#endif

    if (result.applied)
    {
        bloodEffect_.Spawn(tracerEnd_, impactNormal);
        hitMarkerTimer_ = 0.12f;
        hitMarkerKill_ = result.becameDead;

        TraceLog(LOG_INFO, "HIT: Enemy HP = %d", result.hp);
        if (result.becameDead)
        {
            TraceLog(LOG_INFO, "Enemy defeated");
        }
    }
}

void GameContext::UpdateShotEffects(float deltaTime)
{
    bloodEffect_.Update(deltaTime);
    muzzleFlashTimer_ = (std::max)(
        0.0f, muzzleFlashTimer_ - deltaTime);

    if (tracerActive_)
    {
        tracerAge_ += deltaTime;

        if (tracerAge_ >= tracerDuration_)
        {
            tracerActive_ = false;
        }
    }

    if (!player_.IsAlive())
    {
        muzzleFlashTimer_ = 0.0f;
    }

    for (BulletMark& mark : bulletMarks_)
    {
        mark.remainingTime -= deltaTime;
    }

    bulletMarks_.erase(
        std::remove_if(
            bulletMarks_.begin(),
            bulletMarks_.end(),
            [](const BulletMark& mark)
            {
                return mark.remainingTime <= 0.0f;
            }
        ),
        bulletMarks_.end()
    );
}
