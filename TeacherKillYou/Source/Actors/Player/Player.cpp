#include "Player.h"
#include "CheckpointData.h"
#include "AudioManager.h"
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
    float HudScale()
    {
        return Clamp((std::min)(GetScreenWidth() / 1920.0f, GetScreenHeight() / 1080.0f), 0.45f, 1.5f);
    }

    void LoadHudTexture(const char* path, Texture2D& texture, Rectangle& source, const char* fallback = nullptr)
    {
        if (texture.id) return;
        Image image = LoadImage(path);
        if (!image.data && fallback) image = LoadImage(fallback);
        if (!image.data) return;
        // 透明な余白だけを除き、画像自体の縦横比とデザインは維持する。
        source = GetImageAlphaBorder(image, 0.05f);
        texture = LoadTextureFromImage(image);
        UnloadImage(image);
        if (texture.id) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    }

    Rectangle HealthHudPanel(Rectangle source)
    {
        const float scale = HudScale();
        const float width = 360.0f * scale;
        const float height = source.width > 0 && source.height > 0 ? width * source.height / source.width : 72.0f * scale;
        // 下部の懐中電灯キー表示との間隔を確保する。
        return {24.0f * scale, GetScreenHeight() - height - 48.0f, width, height};
    }

    void DrawHudText(const char* text, int x, int y, int size, Color color)
    {
        const int offset = (std::max)(1, static_cast<int>(2 * HudScale()));
        DrawText(text, x + offset, y + offset, size, Color{12, 16, 16, 230});
        DrawText(text, x, y, size, color);
    }

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

void Player::Init()
{
    Reset();
    LoadHudTexture("Data/Image/GunUI.png", ammoHudTexture_, ammoHudSource_, "Data/Image/PistolUI.png");
    LoadHudTexture("Data/Image/HPBackUI.png", healthHudTexture_, healthHudSource_);
}

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

    weaponBobTime_ = 0.0f;
    weaponBobBlend_ = 0.0f;
    weaponSprintBlend_ = 0.0f;
    weaponSway_ = { 0.0f, 0.0f };
    weaponMotionOffset_ = { 0.0f, 0.0f, 0.0f };
    weaponRoll_ = 0.0f;

	weaponRecoilBack_ = 0.0f;
	weaponRecoilPitch_ = 0.0f;

    damageImpactPending_ = false;
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

    UpdateWeaponMotion(deltaTime, hasMoveInput);
}


void Player::UpdateWeaponMotion(float deltaTime, bool hasMoveInput)
{
    const float blend = 1.0f - std::exp(-8.0f * (std::max)(deltaTime, 0.0f));
    const bool isWalkingOnGround = hasMoveInput && isGrounded_;

    const float targetBob = isWalkingOnGround ? 1.0f : 0.0f;
    const float targetSprint = (isWalkingOnGround && isSprinting_) ? 1.0f : 0.0f;

    weaponBobBlend_ = Lerp(weaponBobBlend_, targetBob, blend);
    weaponSprintBlend_ = Lerp(weaponSprintBlend_, targetSprint, blend);

    if (isWalkingOnGround)
    {
        const float bobFrequency = isSprinting_ ? 9.0f : 5.5f;
        weaponBobTime_ += bobFrequency * deltaTime;
    }
    const Vector2 mouseDelta = GetMouseDelta();
    const float targetSwayX = Clamp(mouseDelta.x * 0.0010f, -0.025f, 0.025f);
    const float targetSwayY = Clamp(mouseDelta.y * 0.0008f, -0.018f, 0.018f);

    weaponSway_.x = Lerp(weaponSway_.x, targetSwayX, blend);
    weaponSway_.y = Lerp(weaponSway_.y, targetSwayY, blend);

    const float bobStrength = 1.0f + weaponSprintBlend_ * 0.55f;
    const float horizontalBob = std::sin(weaponBobTime_) * 0.009f;
    const float verticalBob = -std::fabs(std::cos(weaponBobTime_)) * 0.007f;

    weaponMotionOffset_.x =
        horizontalBob * bobStrength * weaponBobBlend_ + weaponSway_.x;
    weaponMotionOffset_.y =
        verticalBob * bobStrength * weaponBobBlend_ + weaponSway_.y
        - 0.025f * weaponSprintBlend_;
    weaponMotionOffset_.z = 0.0f;

    weaponRoll_ = 0.075f * weaponSprintBlend_
        + 0.010f * std::sin(weaponBobTime_) * weaponBobBlend_;

    const float recoilRecovery =
        std::exp(-10.0f * (std::max)(deltaTime, 0.0f));

    weaponRecoilBack_ *= recoilRecovery;
    weaponRecoilPitch_ *= recoilRecovery;
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
        AM().PlaySe(SeId::Reload);
        reloadTimer_ = 0.0f;
        ApplyWeaponPose("PlayerReload", 0.0f);
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        // 空撃ちはSEだけを鳴らし、弾・マズルフラッシュ・命中処理を生成しない。
        if (currentAmmo_ <= 0)
        {
            AM().PlaySe(SeId::DryFire);
            return;
        }
        if (!HasWeaponAnimation("PlayerShoot"))
        {
            TraceLog(LOG_WARNING, "Player shoot model/animation unavailable");
            return;
        }

        isShooting_ = true;
        shootTimer_ = 0.0f;
        --currentAmmo_;
        shotPending_ = true;
        AM().PlaySe(SeId::Shoot);

        // Weapon recoil
        weaponRecoilBack_ = (std::min)(
            weaponRecoilBack_ + 0.025f, 0.06f);

        weaponRecoilPitch_ = (std::min)(
            weaponRecoilPitch_ + 0.040f, 0.10f);

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
    // 身長や幅を変更しても、足元・胴体・頭と左右の端を同じ基準で調べる。
    // 移動先までレイを伸ばし、大きいdeltaTimeでも壁を通り抜けないようにする。
    constexpr float margin = 0.001f;
    const float heights[] = {playerHeight_ * .1f, playerHeight_ * .5f, playerHeight_ - .04f};
    const float offsets[] = {-playerRadius_ * .8f, 0, playerRadius_ * .8f};
    for (int axis = 0; axis < 2; ++axis) {
        const float amount = axis == 0 ? moveAmount.x : moveAmount.z;
        if (amount == 0) continue;
        const float sign = amount > 0 ? 1.0f : -1.0f;
        const Vector3 direction = axis == 0 ? Vector3{sign, 0, 0} : Vector3{0, 0, sign};
        float travel = std::fabs(amount);
        for (float height : heights) for (float offset : offsets) {
            Vector3 origin = position_;
            origin.y += height;
            if (axis == 0) origin.z += offset;
            else origin.x += offset;
            const auto hit = stage.Raycast({origin, direction}, travel + playerRadius_ + margin);
            if (hit.hit) travel = (std::min)(travel, (std::max)(0.0f, hit.distance - playerRadius_ - margin));
        }
        if (axis == 0) position_.x += sign * travel;
        else position_.z += sign * travel;
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


	// 天井の衝突判定
    if (velocity_.y > 0.0f)
    {
        constexpr float headOffset = 0.1f;
        // プレイヤーの足元から頭のてっぺん付近までの高さを設定
        Vector3 headRayOrigin = { position_.x, previousY + playerHeight_ - headOffset, position_.z };

        // 1フレーム分の移動量 + 少しの余裕を持たせたプローブ距離
        float probeDistance = (position_.y - previousY) + headOffset;

        Ray upRay = { headRayOrigin, { 0, 1, 0 } };
        RayCollision ceilingHit = stage.Raycast(upRay, probeDistance);

        // 天井にヒットした場合、頭が天井にぶつかったとして位置を補正し、上方向の速度をゼロにする
        if (ceilingHit.hit)
        {
            // ヒットした天井の下面あたりに位置を固定
            position_.y = ceilingHit.point.y - playerHeight_;
            velocity_.y = 0.0f;
        }
    }

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
bool Player::ConsumeDamageImpact()
{
    if (!damageImpactPending_)
    {
        return false;
    }

    damageImpactPending_ = false;
    return true;
}

void Player::TakeDamage(int amount)
{
    if (amount <= 0 || !IsAlive()) return;
    isSprinting_ = false;
    hp_ = amount >= hp_ ? 0 : hp_ - amount;
    damageFlashTimer_ = damageFlashDuration_;
    AM().PlaySe(SeId::PlayerHit);

    if (!IsAlive())
    {
        AM().StopSe(SeId::Reload);
        isShooting_ = false;
        isReloading_ = false;
        shotPending_ = false;

        shootTimer_ = 0.0f;
        reloadTimer_ = 0.0f;

        velocity_ = {};
    }
	damageImpactPending_ = true;
}

void Player::DrawDamageOverlay() const
{
    if (damageFlashTimer_ <= 0.0f) return;

    const float progress = DamageFlashStrength(damageFlashTimer_, damageFlashDuration_);

    DrawRectangle(
        0, 0,
        GetScreenWidth(), GetScreenHeight(),
        Fade(Color{ 210, 8, 18, 255 }, progress * 0.36f));
}
void Player::DrawStaminaHUD() const
{
    const Rectangle panel = HealthHudPanel(healthHudSource_);
    const int value = static_cast<int>(std::round(Clamp(stamina_, 0.0f, maxStamina_)));
    // HPBackUIの横線より下に、毎フレームのスタミナ残量を整数で表示する。
    DrawHudText(TextFormat("STAMINA %d / %d", value, static_cast<int>(maxStamina_)),
        static_cast<int>(panel.x + panel.width * 0.1f), static_cast<int>(panel.y + panel.height * 0.56f),
        static_cast<int>(24 * HudScale()), sprintExhausted_ ? ORANGE : SKYBLUE);
}

void Player::DrawColliderDebug() const
{
    if (!debugDrawEnabled_) return;

    // プレイヤーの足元から頭までの中心位置
    Vector3 center = {
        position_.x,
        position_.y + (playerHeight_ * 0.5f),
        position_.z
    };

    // 判定範囲をワイヤーフレームの円柱で描画
    DrawCylinderWires(
        { position_.x, position_.y, position_.z }, // 底面の位置
        playerRadius_,                             // 底面半径
        playerRadius_,                             // 上面半径
        playerHeight_,                             // 高さ
        12,                                        // 分割数
        YELLOW                                     // 黄色
    );

    // 足元の当たり判定位置に小さな球を描画
    DrawSphereWires(center, playerRadius_, 8, 8, ORANGE);
}

void Player::DrawHealthHUD() const
{
    const Rectangle panel = HealthHudPanel(healthHudSource_);
    if (healthHudTexture_.id && healthHudSource_.width > 0 && healthHudSource_.height > 0)
        DrawTexturePro(healthHudTexture_, healthHudSource_, panel, {0, 0}, 0.0f, WHITE);

    // 上段はHP、下段はDrawStaminaHUDが担当する。危険なHPは数値の色で伝える。
    const Color color = hp_ <= 25 ? RED : hp_ <= 50 ? ORANGE : RAYWHITE;
    DrawHudText(TextFormat("HP %d / %d", hp_, maxHp_), static_cast<int>(panel.x + panel.width * 0.1f),
        static_cast<int>(panel.y + panel.height * 0.04f), static_cast<int>(24 * HudScale()), color);
}

Vector3 Player::GetMuzzlePosition(const Camera3D& view) const
{
    const Vector3 forward = Vector3Normalize(
        Vector3Subtract(view.target, view.position));

    const Vector3 right = Vector3Normalize(
        Vector3CrossProduct(forward, view.up));

    const Vector3 up = Vector3Normalize(
        Vector3CrossProduct(right, forward));


    Vector3 pos = Vector3Add(
        view.position, Vector3Scale(forward, 0.22f));

    pos = Vector3Add(pos, Vector3Scale(right, 0.18f));
    pos = Vector3Subtract(pos, Vector3Scale(up, 0.17f));

    pos = Vector3Add(pos, Vector3Scale(right, weaponMotionOffset_.x));
    pos = Vector3Add(pos, Vector3Scale(up, weaponMotionOffset_.y));
    pos = Vector3Add(pos, Vector3Scale(forward, weaponMotionOffset_.z));

    pos = Vector3Subtract(
        pos,
        Vector3Scale(forward, weaponRecoilBack_)
    );

    const float yaw = atan2f(forward.x, forward.z);
    const float pitch = asinf(Clamp(forward.y, -1.0f, 1.0f));

    const Quaternion cameraRotation = QuaternionNormalize(
        QuaternionMultiply(
            QuaternionFromAxisAngle({ 0, 1, 0 }, yaw),
            QuaternionFromAxisAngle({ 1, 0, 0 }, -pitch)));

    // Recoil pitch
    const Quaternion recoilRotation =
        QuaternionFromAxisAngle(
            { 1.0f, 0.0f, 0.0f },
            -weaponRecoilPitch_
        );

    // weapon roll
    const Quaternion rollRotation =
        QuaternionFromAxisAngle(
            { 0.0f, 0.0f, 1.0f },
            weaponRoll_
        );

    // Combine camera rotation, recoil, and sway roll
    const Quaternion rotation = QuaternionNormalize(
        QuaternionMultiply(
            cameraRotation,
            QuaternionMultiply(recoilRotation, rollRotation)
        )
    );

    const Vector3 muzzleLocal = { 0.0f, 7.0f, 25.5f };

    const Vector3 muzzleOffset = Vector3RotateByQuaternion(
        Vector3Scale(muzzleLocal, viewModelScale_), rotation);

    return Vector3Add(pos, muzzleOffset);
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

        Vector3 pos = Vector3Add(
            view.position, Vector3Scale(forward, 0.22f));

        pos = Vector3Add(pos, Vector3Scale(right, 0.18f));
        pos = Vector3Subtract(pos, Vector3Scale(up, 0.17f));

        pos = Vector3Add(pos, Vector3Scale(right, weaponMotionOffset_.x));
        pos = Vector3Add(pos, Vector3Scale(up, weaponMotionOffset_.y));
        pos = Vector3Add(pos, Vector3Scale(forward, weaponMotionOffset_.z));

        // Recoil: move pistol backward
        pos = Vector3Subtract(
            pos,
            Vector3Scale(forward, weaponRecoilBack_)
        );

        const float yaw = atan2f(forward.x, forward.z);
        const float pitch = asinf(Clamp(forward.y, -1.0f, 1.0f));

        const Quaternion yawRotation =
            QuaternionFromAxisAngle({ 0.0f, 1.0f, 0.0f }, yaw);

        const Quaternion pitchRotation =
            QuaternionFromAxisAngle({ 1.0f, 0.0f, 0.0f }, -pitch);

        const Quaternion cameraRotation = QuaternionNormalize(
            QuaternionMultiply(yawRotation, pitchRotation));

        // Local Z roll leaves the camera untouched; only pistol/arms tilt.
        const Quaternion rotation = QuaternionNormalize(
            QuaternionMultiply(cameraRotation,
                QuaternionFromAxisAngle({ 0.0f, 0.0f, 1.0f }, weaponRoll_)));

        Vector3 axis = { 0.0f, 1.0f, 0.0f };
        float angle = 0.0f;

        QuaternionToAxisAngle(rotation, &axis, &angle);

        // Apply the FPS lighting profile to the gun and arms in every animation pose.
        DrawHorrorModelEx(
            handGun,
            pos,
            axis,
            angle * RAD2DEG,
            { viewModelScale_, viewModelScale_, viewModelScale_ },
            WHITE, 0.0f, true
        );
    }

    if (camera == nullptr)
    {
        DrawColliderDebug();
    }
}

void Player::DrawAmmoHUD() const
{
    if (ammoHudTexture_.id && ammoHudSource_.width > 0 && ammoHudSource_.height > 0)
    {
        const float uiScale = HudScale();
        const float width = 480.0f * uiScale;
        const float height = width * ammoHudSource_.height / ammoHudSource_.width;
        const float margin = 24.0f * uiScale;
        const Rectangle panel{ GetScreenWidth() - width - margin, GetScreenHeight() - height - margin, width, height };
        DrawTexturePro(ammoHudTexture_, ammoHudSource_, panel, {0, 0}, 0.0f, WHITE);

        // 銃と弾のイラストの右側へ、実際の残弾を表示する。
        const int currentSize = static_cast<int>(36 * uiScale);
        const int reserveSize = static_cast<int>(24 * uiScale);
        const int x = static_cast<int>(panel.x + width * 0.66f);
        const int y = static_cast<int>(panel.y + height * 0.53f);
        const char* current = TextFormat("%02d", currentAmmo_);
        const int currentWidth = MeasureText(current, currentSize);
        DrawHudText(current, x, y, currentSize, currentAmmo_ > 0 ? RAYWHITE : RED);
        DrawHudText(TextFormat(" / %02d", reserveAmmo_), x + currentWidth + static_cast<int>(4 * uiScale),
            y + currentSize - reserveSize, reserveSize, RAYWHITE);
        if (isReloading_)
            DrawText("RELOADING", x, static_cast<int>(panel.y - 24 * uiScale), static_cast<int>(18 * uiScale), YELLOW);
        return;
    }

    // 画像がない環境でも、従来の数字表示で残弾を確認できる。
    const int x = GetScreenWidth() - 210;
    const int y = GetScreenHeight() - 70;
    DrawRectangle(x - 15, y - 12, 195, 60, Fade(BLACK, 0.65f));
    DrawText(TextFormat("%02d / %02d", currentAmmo_, reserveAmmo_), x, y, 30, RAYWHITE);
    if (isReloading_) DrawText("RELOADING", x, y - 35, 18, YELLOW);
}
// =======================================
// Med Kit - Restore HP
// =======================================
void Player::Heal(int amount)
{
    if (amount <= 0 || !IsAlive())
        return;

    hp_ = (std::min)(hp_ + amount, maxHp_);
}

// =======================================
// Ammo - Add Reserve Bullets
// =======================================
void Player::AddReserveAmmo(int amount)
{
    if (amount <= 0)
        return;

    reserveAmmo_ += amount;
}

void Player::End()
{
    if (ammoHudTexture_.id) UnloadTexture(ammoHudTexture_);
    ammoHudTexture_ = {};
    ammoHudSource_ = {};
    if (healthHudTexture_.id) UnloadTexture(healthHudTexture_);
    healthHudTexture_ = {};
    healthHudSource_ = {};
}
bool Player::IsGrounded() const { return isGrounded_; }
bool Player::IsSprinting() const { return isSprinting_; }

PlayerCheckpoint Player::CaptureCheckpoint() const
{
    return {position_, hp_, currentAmmo_, reserveAmmo_, stamina_};
}

void Player::RestoreCheckpoint(const PlayerCheckpoint& data)
{
    Reset();
    position_ = data.position;
    hp_ = std::clamp(data.hp, 1, maxHp_);
    currentAmmo_ = std::clamp(data.ammo, 0, magazineCapacity_);
    reserveAmmo_ = std::clamp(data.reserveAmmo, 0, 1000000);
    stamina_ = Clamp(data.stamina, 0, maxStamina_);
}
