#include "Player.h"
#include "DamageFlash.h"
#include "HorrorLighting.h"
#include "Stage.h"
#include "ResourceManager.h"
#include "ResourceKeys.h"
#include "PlayerCamera.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include  <vector>
#include <cmath>
namespace
{
    bool HasWeaponAnimation(const char* key)
    {
        int count = 0;
        ModelAnimation* animations = RM().GetModelAnimations(key, &count);
        const Model model = RM().GetModel(key);

        return model.meshCount > 0 &&
            animations != nullptr &&
            count > 0 &&
            animations[0].frameCount > 0 &&
            IsModelAnimationValid(model, animations[0]);
    }

    void ApplyWeaponPose(const char* key, float progress)
    {
        if (!HasWeaponAnimation(key)) return;

        Model& model = RM().GetModelRef(key);

        int count = 0;
        ModelAnimation* animations =
            RM().GetModelAnimations(key, &count);

        const ModelAnimation& animation = animations[0];

        if (animation.boneCount <= 0 ||
            animation.framePoses == nullptr)
        {
            return;
        }

        const float framePosition =
            Clamp(progress, 0.0f, 1.0f) * (animation.frameCount - 1);

        const int frameA = static_cast<int>(framePosition);
        const int frameB = (std::min)(
            frameA + 1, animation.frameCount - 1);

        const float blend = framePosition - frameA;

        // Reuse storage instead of allocating on every update.
        static std::vector<Transform> blendedPose;
        blendedPose.resize(animation.boneCount);

        for (int bone = 0; bone < animation.boneCount; ++bone)
        {
            const Transform& a = animation.framePoses[frameA][bone];
            const Transform& b = animation.framePoses[frameB][bone];

            blendedPose[bone].translation =
                Vector3Lerp(a.translation, b.translation, blend);

            blendedPose[bone].rotation =
                QuaternionSlerp(a.rotation, b.rotation, blend);

            blendedPose[bone].scale =
                Vector3Lerp(a.scale, b.scale, blend);
        }

        // Present the blended pose as a temporary one-frame animation.
        Transform* poseFrames[] = { blendedPose.data() };

        ModelAnimation blendedAnimation = animation;
        blendedAnimation.frameCount = 1;
        blendedAnimation.framePoses = poseFrames;

        for (int i = 0; i < model.meshCount; ++i)
        {
            Mesh& mesh = model.meshes[i];

            if (mesh.boneWeights == nullptr ||
                mesh.boneIds == nullptr ||
                mesh.animVertices == nullptr)
            {
                continue;
            }

            Model skinnedView = model;
            skinnedView.meshes = &mesh;
            skinnedView.meshCount = 1;

            UpdateModelAnimation(skinnedView, blendedAnimation, 0);
        }
    }
}

void Player::Init() { Reset(); }

void Player::Reset()
{
    position_ = { 0.0f, 4.0f, 0.0f };
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
    velocity_ = { 0.0f, 0.0f, 0.0f };
    isGrounded_ = false;
    isShooting_ = false;
    isReloading_ = false;
    shotPending_ = false;

    shootTimer_ = 0.0f;
    reloadTimer_ = 0.0f;

    currentAmmo_ = magazineCapacity_;
    reserveAmmo_ = 90;

    hp_ = maxHp_;
    damageFlashTimer_ = 0.0f;

    stamina_ = maxStamina_;
    staminaRecoveryTimer_ = 0.0f;
    isSprinting_ = false;
    sprintExhausted_ = false;
}

void Player::Update(float deltaTime, const Stage& stage,
    Vector3 forward, Vector3 right)
{
    damageFlashTimer_ = (std::max)(
        0.0f, damageFlashTimer_ - deltaTime);

    if (IsKeyPressed(KEY_H))
    {
        TakeDamage(10);
    }

    isSprinting_ = false;

    if (!IsAlive()) return;

    // Update first so pressing R blocks sprinting immediately.
    UpdateWeapon(deltaTime);

    Vector3 moveDir = GetInputMoveDirection(forward, right);

    // Keep movement horizontal, even when looking up or down.
    moveDir.y = 0.0f;

    const bool hasMoveInput = Vector3Length(moveDir) > 0.0f;

    if (sprintExhausted_ && stamina_ >= maxStamina_ * 0.25f)
    {
        sprintExhausted_ = false;
    }

    const bool wantsSprint =
        IsKeyDown(KEY_LEFT_SHIFT) &&
        hasMoveInput &&
        isGrounded_ &&
        !isReloading_ &&
        !sprintExhausted_ &&
        stamina_ > 0.0f;

    float currentSpeed = moveSpeed_;

    if (wantsSprint)
    {
        // Only grant sprint speed for the time stamina can cover.
        const float sprintTime = (std::min)(
            deltaTime, stamina_ / staminaDrainRate_);

        if (deltaTime > 0.0f)
        {
            currentSpeed +=
                (sprintSpeed_ - moveSpeed_) * (sprintTime / deltaTime);
        }

        isSprinting_ = sprintTime > 0.0f;

        stamina_ = (std::max)(
            0.0f, stamina_ - staminaDrainRate_ * sprintTime);

        staminaRecoveryTimer_ = staminaRecoveryDelay_;

        if (stamina_ <= 0.0f)
        {
            sprintExhausted_ = true;
        }
    }
    else
    {
        const float recoveryTime = (std::max)(
            0.0f, deltaTime - staminaRecoveryTimer_);

        staminaRecoveryTimer_ = (std::max)(
            0.0f, staminaRecoveryTimer_ - deltaTime);

        stamina_ = (std::min)(
            maxStamina_, stamina_ + staminaRegenRate_ * recoveryTime);
    }

    if (hasMoveInput)
    {
        Vector3 moveAmount = Vector3Scale(
            Vector3Normalize(moveDir), currentSpeed * deltaTime);

        MoveWithCollision(moveAmount, stage);
    }

    UpdateVerticalPhysics(deltaTime, stage);
}
void Player::UpdateWeapon(float deltaTime)
{
    if (!IsAlive()) return;
    if (isReloading_)
    {
        reloadTimer_ += deltaTime;

        const float progress = reloadTimer_ / reloadDuration_;
        ApplyWeaponPose("PlayerReload", progress);

        if (reloadTimer_ >= reloadDuration_)
        {
            const int needed = magazineCapacity_ - currentAmmo_;
            const int loaded = (std::min)(needed, reserveAmmo_);

            currentAmmo_ += loaded;
            reserveAmmo_ -= loaded;

            isReloading_ = false;
            reloadTimer_ = 0.0f;
        }

        return;
    }

    if (isShooting_)
    {
        shootTimer_ += deltaTime;

        const float progress = shootTimer_ / shootDuration_;
        ApplyWeaponPose("PlayerShoot", progress);

        if (shootTimer_ >= shootDuration_)
        {
            isShooting_ = false;
            shootTimer_ = 0.0f;
        }

        return;
    }

    if (IsKeyPressed(KEY_R) &&
        currentAmmo_ < magazineCapacity_ &&
        reserveAmmo_ > 0)
    {
        if (!HasWeaponAnimation("PlayerReload"))
        {
            TraceLog(LOG_WARNING, "Player reload model/animation unavailable");
            return;
        }

        isReloading_ = true;
        reloadTimer_ = 0.0f;
        ApplyWeaponPose("PlayerReload", 0.0f);
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && currentAmmo_ > 0)
    {
        if (!HasWeaponAnimation("PlayerShoot"))
        {
            TraceLog(LOG_WARNING, "Player shoot model/animation unavailable");
            return;
        }

        isShooting_ = true;
        shootTimer_ = 0.0f;
        --currentAmmo_;
        shotPending_ = true;

        ApplyWeaponPose("PlayerShoot", 0.0f);
    }
}
Vector3 Player::GetInputMoveDirection(Vector3 forward, Vector3 right) const
{
    Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
    if (IsKeyDown(KEY_W)) moveDir = Vector3Add(moveDir, forward);
    if (IsKeyDown(KEY_S)) moveDir = Vector3Subtract(moveDir, forward);
    if (IsKeyDown(KEY_A)) moveDir = Vector3Add(moveDir, right);
    if (IsKeyDown(KEY_D)) moveDir = Vector3Subtract(moveDir, right);
    return moveDir;
}

void Player::MoveWithCollision(Vector3 moveAmount, const Stage& stage)
{
    Vector3 rayPos = Vector3Add(position_, { 0.0f, 0.8f, 0.0f });

    if (moveAmount.x != 0.0f)
    {
        Vector3 dir = { moveAmount.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f };
        RayCollision hit = stage.Raycast({ rayPos, dir });
        if (!hit.hit || hit.distance > playerRadius_) position_.x += moveAmount.x;
    }

    if (moveAmount.z != 0.0f)
    {
        Vector3 dir = { 0.0f, 0.0f, moveAmount.z > 0.0f ? 1.0f : -1.0f };
        RayCollision hit = stage.Raycast({ rayPos, dir });
        if (!hit.hit || hit.distance > playerRadius_) position_.z += moveAmount.z;
    }
}

void Player::UpdateVerticalPhysics(float deltaTime, const Stage& stage)
{
    if (IsKeyDown(KEY_SPACE) && isGrounded_)
    {
        velocity_.y = jumpForce_;
        isGrounded_ = false;
    }

    if (!isGrounded_) velocity_.y += gravity_ * deltaTime;
    const float previousY = position_.y;
    position_.y += velocity_.y * deltaTime;
    // 移動前から落下区間を検査し、天井面の誤判定と床のすり抜けを防ぐ。
    constexpr float probeHeight = 0.25f;
    const Ray downRay = { { position_.x, previousY + probeHeight, position_.z }, { 0, -1, 0 } };
    const float probeDistance = probeHeight + (std::max)(0.0f, previousY - position_.y) + 0.01f;
    const RayCollision groundHit = stage.RaycastGround(downRay, probeDistance);
    if (velocity_.y <= 0.0f && groundHit.hit && position_.y <= groundHit.point.y + 0.01f)
    {
        position_.y = groundHit.point.y;
        velocity_.y = 0.0f;
        isGrounded_ = true;
        return;
    }
    isGrounded_ = false;
}
void Player::TakeDamage(int amount)
{
    if (amount <= 0 || !IsAlive()) return;
    isSprinting_ = false;
    hp_ = amount >= hp_ ? 0 : hp_ - amount;
    damageFlashTimer_ = damageFlashDuration_;

    if (!IsAlive())
    {
        isShooting_ = false;
        isReloading_ = false;
        shotPending_ = false;

        shootTimer_ = 0.0f;
        reloadTimer_ = 0.0f;

        velocity_ = {};
    }
}

void Player::DrawDamageOverlay() const
{
    if (damageFlashTimer_ <= 0.0f) return;

    const float progress = DamageFlashStrength(damageFlashTimer_, damageFlashDuration_);

    DrawRectangle(
        0, 0,
        GetScreenWidth(), GetScreenHeight(),
        Fade(Color{210, 8, 18, 255}, progress * 0.36f));
}
void Player::DrawStaminaHUD() const
{
    const int x = 24;
    const int y = GetScreenHeight() - 58;
    const int width = 200;
    const int height = 8;

    const float ratio = Clamp(
        stamina_ / maxStamina_, 0.0f, 1.0f);

    const Color color = sprintExhausted_ ? ORANGE : SKYBLUE;

    DrawRectangle(x, y, width, height, DARKGRAY);

    DrawRectangle(
        x, y,
        static_cast<int>(width * ratio),
        height, color);

    DrawRectangleLines(x, y, width, height, RAYWHITE);
}

void Player::DrawHealthHUD() const
{
    const int x = 24;
    const int y = GetScreenHeight() - 110;
    const int width = 200;
    const int height = 18;

    const float ratio = Clamp(
        static_cast<float>(hp_) / maxHp_, 0.0f, 1.0f);

    Color color = GREEN;

    if (hp_ <= 25)
    {
        color = RED;
    }
    else if (hp_ <= 50)
    {
        color = ORANGE;
    }

    DrawRectangle(
        x - 12, y - 12,
        width + 24, 72,
        Fade(BLACK, 0.65f));

    DrawText(
        TextFormat("HP %d / %d", hp_, maxHp_),
        x, y, 20, RAYWHITE);

    DrawRectangle(x, y + 30, width, height, DARKGRAY);

    DrawRectangle(
        x, y + 30,
        static_cast<int>(width * ratio),
        height, color);

    DrawRectangleLines(x, y + 30, width, height, RAYWHITE);
}

void Player::Draw(const PlayerCamera* camera) const
{
    Model handGun{};

    if (isReloading_)
    {
        handGun = RM().GetModel("PlayerReload");
    }
    else if (isShooting_)
    {
        handGun = RM().GetModel("PlayerShoot");
    }
    else
    {
        handGun = RM().GetModel(ResourceKeys::Model_HandGunView);
    }

    if (camera != nullptr)
    {
        const Camera3D view = camera->GetRaylibCamera();

        const Vector3 forward = Vector3Normalize(
            Vector3Subtract(view.target, view.position));

        const Vector3 right = Vector3Normalize(
            Vector3CrossProduct(forward, view.up));

        const Vector3 up = Vector3Normalize(
            Vector3CrossProduct(right, forward));

        // Keep your existing viewmodel offsets.
        Vector3 pos = Vector3Add(
            view.position, Vector3Scale(forward, 0.22f));

        pos = Vector3Add(pos, Vector3Scale(right, 0.1f));
        pos = Vector3Subtract(pos, Vector3Scale(up, 0.15f));

        const float yaw = atan2f(forward.x, forward.z);
        const float pitch = asinf(Clamp(forward.y, -1.0f, 1.0f));

        const Quaternion yawRotation =
            QuaternionFromAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);

        const Quaternion pitchRotation =
            QuaternionFromAxisAngle({ 1.0f, 0.0f, 0.0f }, -pitch);

        const Quaternion rotation = QuaternionNormalize(
            QuaternionMultiply(yawRotation, pitchRotation));

        Vector3 axis = { 0.0f, 1.0f, 0.0f };
        float angle = 0.0f;

        QuaternionToAxisAngle(rotation, &axis, &angle);

        DrawModelEx(
            handGun,
            pos,
            axis,
            angle * RAD2DEG,
            { viewModelScale_, viewModelScale_, viewModelScale_ },
            WHITE
        );
    }
}

void Player::DrawAmmoHUD() const
{
    const int x = GetScreenWidth() - 210;
    const int y = GetScreenHeight() - 70;

    DrawRectangle(x - 15, y - 12, 195, 60, Fade(BLACK, 0.65f));

    DrawText(
        TextFormat("%02d / %02d", currentAmmo_, reserveAmmo_),
        x, y, 30, RAYWHITE);

    if (isReloading_)
    {
        DrawText("RELOADING", x, y - 35, 18, YELLOW);
    }
}

void Player::End() {}